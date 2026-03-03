#include "Commands/MCPPropertyCommands.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "ScopedTransaction.h"
#include "Components/PrimitiveComponent.h"

namespace PropertyCommandsLocal
{
static UWorld* GetEditorWorld()
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

static AActor* FindActorByName(const FString& Name)
{
	UWorld* World = GetEditorWorld();
	if (!World) return nullptr;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetActorLabel() == Name || It->GetName() == Name)
			return *It;
	}
	return nullptr;
}

static FString PropertyValueToString(FProperty* Prop, const void* ValuePtr)
{
	FString Result;
	Prop->ExportTextItem_Direct(Result, ValuePtr, nullptr, nullptr, PPF_None);
	return Result;
}

static TSharedPtr<FJsonObject> PropertyToJson(FProperty* Prop, const void* ContainerPtr)
{
	TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
	Info->SetStringField(TEXT("name"), Prop->GetName());
	Info->SetStringField(TEXT("type"), Prop->GetCPPType());
	Info->SetStringField(TEXT("category"), Prop->GetMetaData(TEXT("Category")));

	const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ContainerPtr);
	Info->SetStringField(TEXT("value"), PropertyValueToString(Prop, ValuePtr));

	Info->SetBoolField(TEXT("editable"), Prop->HasAnyPropertyFlags(CPF_Edit));
	Info->SetBoolField(TEXT("blueprint_visible"), Prop->HasAnyPropertyFlags(CPF_BlueprintVisible));

	return Info;
}
} // namespace PropertyCommandsLocal

using namespace PropertyCommandsLocal;

// --- Get Object Properties ---
TSharedPtr<FJsonObject> FMCPGetObjectPropertiesCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ObjectPath = Params->GetStringField(TEXT("object_path"));
	FString CategoryFilter = Params->GetStringField(TEXT("category_filter"));

	// Try to find as actor first
	UObject* Object = PropertyCommandsLocal::FindActorByName(ObjectPath);

	// Try as asset path
	if (!Object)
	{
		Object = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	}

	if (!Object)
	{
		return ErrorResponse(FString::Printf(TEXT("Object not found: %s"), *ObjectPath));
	}

	TArray<TSharedPtr<FJsonValue>> Properties;

	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		FProperty* Prop = *It;

		if (!CategoryFilter.IsEmpty())
		{
			FString Cat = Prop->GetMetaData(TEXT("Category"));
			if (!Cat.Contains(CategoryFilter))
				continue;
		}

		Properties.Add(MakeShared<FJsonValueObject>(PropertyToJson(Prop, Object)));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("object"), Object->GetName());
	Data->SetStringField(TEXT("class"), Object->GetClass()->GetName());
	Data->SetArrayField(TEXT("properties"), Properties);
	return SuccessResponse(Data);
}

// --- Set Object Property ---
TSharedPtr<FJsonObject> FMCPSetObjectPropertyCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ObjectPath = Params->GetStringField(TEXT("object_path"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	FString PropertyValue = Params->GetStringField(TEXT("property_value"));

	UObject* Object = PropertyCommandsLocal::FindActorByName(ObjectPath);
	if (!Object)
	{
		Object = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	}
	if (!Object)
	{
		return ErrorResponse(FString::Printf(TEXT("Object not found: %s"), *ObjectPath));
	}

	// Resolve property — support dot-notation for nested struct properties (e.g. "BodyInstance.CollisionProfileName")
	FProperty* Prop = nullptr;
	void* ContainerPtr = Object;

	if (PropertyName.Contains(TEXT(".")))
	{
		TArray<FString> PropertyChain;
		PropertyName.ParseIntoArray(PropertyChain, TEXT("."), true);

		UStruct* CurrentStruct = Object->GetClass();
		void* CurrentPtr = Object;

		for (int32 i = 0; i < PropertyChain.Num(); ++i)
		{
			FProperty* ChainProp = CurrentStruct->FindPropertyByName(*PropertyChain[i]);
			if (!ChainProp)
			{
				return ErrorResponse(FString::Printf(TEXT("Property '%s' not found in chain '%s' on %s"),
					*PropertyChain[i], *PropertyName, *Object->GetClass()->GetName()));
			}

			if (i == PropertyChain.Num() - 1)
			{
				Prop = ChainProp;
				ContainerPtr = CurrentPtr;
			}
			else
			{
				FStructProperty* StructProp = CastField<FStructProperty>(ChainProp);
				if (!StructProp)
				{
					return ErrorResponse(FString::Printf(TEXT("'%s' is not a struct property, cannot navigate further in '%s'"),
						*PropertyChain[i], *PropertyName));
				}
				CurrentPtr = StructProp->ContainerPtrToValuePtr<void>(CurrentPtr);
				CurrentStruct = StructProp->Struct;
			}
		}
	}
	else
	{
		Prop = Object->GetClass()->FindPropertyByName(FName(*PropertyName));
	}

	if (!Prop)
	{
		return ErrorResponse(FString::Printf(TEXT("Property '%s' not found on %s"), *PropertyName, *Object->GetClass()->GetName()));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Set Property")));
	Object->Modify();
	Object->PreEditChange(Prop);

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ContainerPtr);
	FString OldValue = PropertyValueToString(Prop, ValuePtr);

	if (!Prop->ImportText_Direct(*PropertyValue, ValuePtr, Object, PPF_None))
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to set '%s' to '%s' (ImportText_Direct failed). Check value format for type '%s'."),
			*PropertyName, *PropertyValue, *Prop->GetCPPType()));
	}

	FPropertyChangedEvent ChangedEvent(Prop, EPropertyChangeType::ValueSet);
	Object->PostEditChangeProperty(ChangedEvent);

	// Mark package dirty to ensure level save persists the change
	Object->MarkPackageDirty();

	// Re-read the value to confirm it was actually persisted in memory
	FString ActualNewValue = PropertyValueToString(Prop, ValuePtr);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("property"), PropertyName);
	Data->SetStringField(TEXT("old_value"), OldValue);
	Data->SetStringField(TEXT("new_value"), ActualNewValue);
	return SuccessResponse(Data);
}

// --- Get Component Hierarchy ---
TSharedPtr<FJsonObject> FMCPGetComponentHierarchyCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName = Params->GetStringField(TEXT("actor_name"));

	AActor* Actor = PropertyCommandsLocal::FindActorByName(ActorName);
	if (!Actor)
	{
		return ErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	TArray<TSharedPtr<FJsonValue>> Components;
	TInlineComponentArray<UActorComponent*> ActorComponents;
	Actor->GetComponents(ActorComponents);

	for (UActorComponent* Comp : ActorComponents)
	{
		TSharedPtr<FJsonObject> CompInfo = MakeShared<FJsonObject>();
		CompInfo->SetStringField(TEXT("name"), Comp->GetName());
		CompInfo->SetStringField(TEXT("class"), Comp->GetClass()->GetName());
		CompInfo->SetBoolField(TEXT("is_scene_component"), Comp->IsA<USceneComponent>());

		if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
		{
			FVector Loc = SceneComp->GetRelativeLocation();
			TArray<TSharedPtr<FJsonValue>> LocArr = {
				MakeShared<FJsonValueNumber>(Loc.X),
				MakeShared<FJsonValueNumber>(Loc.Y),
				MakeShared<FJsonValueNumber>(Loc.Z)
			};
			CompInfo->SetArrayField(TEXT("relative_location"), LocArr);

			CompInfo->SetStringField(TEXT("parent"),
				SceneComp->GetAttachParent() ? SceneComp->GetAttachParent()->GetName() : TEXT(""));
		}

		Components.Add(MakeShared<FJsonValueObject>(CompInfo));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor"), Actor->GetActorLabel());
	Data->SetArrayField(TEXT("components"), Components);
	return SuccessResponse(Data);
}

// --- Get Class Defaults ---
TSharedPtr<FJsonObject> FMCPGetClassDefaultsCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ClassName = Params->GetStringField(TEXT("class_name"));

	UClass* Class = nullptr;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->GetName() == ClassName || It->GetName() == FString::Printf(TEXT("A%s"), *ClassName))
		{
			Class = *It;
			break;
		}
	}

	if (!Class)
	{
		return ErrorResponse(FString::Printf(TEXT("Class not found: %s"), *ClassName));
	}

	UObject* CDO = Class->GetDefaultObject();
	if (!CDO)
	{
		return ErrorResponse(TEXT("Failed to get Class Default Object"));
	}

	TArray<TSharedPtr<FJsonValue>> Properties;
	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		FProperty* Prop = *It;
		if (Prop->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			Properties.Add(MakeShared<FJsonValueObject>(PropertyToJson(Prop, CDO)));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("class"), Class->GetName());
	Data->SetArrayField(TEXT("defaults"), Properties);
	return SuccessResponse(Data);
}

// --- Set Component Property ---
TSharedPtr<FJsonObject> FMCPSetComponentPropertyCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName = Params->GetStringField(TEXT("actor_name"));
	FString ComponentName = Params->GetStringField(TEXT("component_name"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	FString PropertyValue = Params->GetStringField(TEXT("property_value"));

	AActor* Actor = PropertyCommandsLocal::FindActorByName(ActorName);
	if (!Actor)
	{
		return ErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	UActorComponent* Component = nullptr;
	TInlineComponentArray<UActorComponent*> Components;
	Actor->GetComponents(Components);
	for (UActorComponent* Comp : Components)
	{
		if (Comp->GetName() == ComponentName)
		{
			Component = Comp;
			break;
		}
	}

	if (!Component)
	{
		// Build list of available components for error message
		TArray<FString> AvailableNames;
		for (UActorComponent* Comp : Components)
		{
			AvailableNames.Add(Comp->GetName());
		}
		return ErrorResponse(FString::Printf(TEXT("Component '%s' not found on actor '%s'. Available: [%s]"),
			*ComponentName, *ActorName, *FString::Join(AvailableNames, TEXT(", "))));
	}

	// Special case: CollisionProfileName on primitive components (stored in BodyInstance struct)
	if (PropertyName == TEXT("CollisionProfileName") || PropertyName == TEXT("BodyInstance.CollisionProfileName"))
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
		if (!PrimComp)
		{
			return ErrorResponse(FString::Printf(TEXT("Component '%s' is not a PrimitiveComponent, cannot set CollisionProfileName"), *ComponentName));
		}
		FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("MCP: Set %s.CollisionProfileName = %s"), *ComponentName, *PropertyValue)));
		Component->Modify();
		PrimComp->SetCollisionProfileName(FName(*PropertyValue));
		Actor->MarkPackageDirty();

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("component"), ComponentName);
		Data->SetStringField(TEXT("property"), TEXT("CollisionProfileName"));
		Data->SetStringField(TEXT("value"), PropertyValue);
		return SuccessResponse(Data);
	}

	// Resolve property — support dot-notation for nested struct properties (e.g. "BodyInstance.bNotifyRigidBodyCollision")
	FProperty* Prop = nullptr;
	void* ContainerPtr = Component;

	if (PropertyName.Contains(TEXT(".")))
	{
		TArray<FString> PropertyChain;
		PropertyName.ParseIntoArray(PropertyChain, TEXT("."), true);

		UStruct* CurrentStruct = Component->GetClass();
		void* CurrentPtr = Component;

		for (int32 i = 0; i < PropertyChain.Num(); ++i)
		{
			FProperty* ChainProp = CurrentStruct->FindPropertyByName(*PropertyChain[i]);
			if (!ChainProp)
			{
				return ErrorResponse(FString::Printf(TEXT("Property '%s' not found in chain '%s' on component '%s' (class: %s)"),
					*PropertyChain[i], *PropertyName, *ComponentName, *Component->GetClass()->GetName()));
			}

			if (i == PropertyChain.Num() - 1)
			{
				Prop = ChainProp;
				ContainerPtr = CurrentPtr;
			}
			else
			{
				FStructProperty* StructProp = CastField<FStructProperty>(ChainProp);
				if (!StructProp)
				{
					return ErrorResponse(FString::Printf(TEXT("'%s' is not a struct property, cannot navigate further in '%s'"),
						*PropertyChain[i], *PropertyName));
				}
				CurrentPtr = StructProp->ContainerPtrToValuePtr<void>(CurrentPtr);
				CurrentStruct = StructProp->Struct;
			}
		}
	}
	else
	{
		Prop = Component->GetClass()->FindPropertyByName(FName(*PropertyName));
	}

	if (!Prop)
	{
		return ErrorResponse(FString::Printf(TEXT("Property '%s' not found on component '%s' (class: %s)"),
			*PropertyName, *ComponentName, *Component->GetClass()->GetName()));
	}

	FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("MCP: Set %s.%s = %s"), *ComponentName, *PropertyName, *PropertyValue)));
	Component->Modify();
	Component->PreEditChange(Prop);

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ContainerPtr);
	FString OldValue = PropertyValueToString(Prop, ValuePtr);

	if (!Prop->ImportText_Direct(*PropertyValue, ValuePtr, Component, PPF_None))
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to set '%s' to '%s' (ImportText_Direct failed). Check value format for type '%s'."),
			*PropertyName, *PropertyValue, *Prop->GetCPPType()));
	}

	FPropertyChangedEvent ChangedEvent(Prop, EPropertyChangeType::ValueSet);
	Component->PostEditChangeProperty(ChangedEvent);
	Actor->MarkPackageDirty();

	FString ActualNewValue = PropertyValueToString(Prop, ValuePtr);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("component"), ComponentName);
	Data->SetStringField(TEXT("property"), PropertyName);
	Data->SetStringField(TEXT("old_value"), OldValue);
	Data->SetStringField(TEXT("new_value"), ActualNewValue);
	return SuccessResponse(Data);
}
