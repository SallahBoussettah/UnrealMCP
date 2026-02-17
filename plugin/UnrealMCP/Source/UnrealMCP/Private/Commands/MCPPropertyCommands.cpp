#include "Commands/MCPPropertyCommands.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "ScopedTransaction.h"

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

// --- Get Object Properties ---
TSharedPtr<FJsonObject> FMCPGetObjectPropertiesCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ObjectPath = Params->GetStringField(TEXT("object_path"));
	FString CategoryFilter = Params->GetStringField(TEXT("category_filter"));

	// Try to find as actor first
	UObject* Object = FindActorByName(ObjectPath);

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

	UObject* Object = FindActorByName(ObjectPath);
	if (!Object)
	{
		Object = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	}
	if (!Object)
	{
		return ErrorResponse(FString::Printf(TEXT("Object not found: %s"), *ObjectPath));
	}

	FProperty* Prop = Object->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Prop)
	{
		return ErrorResponse(FString::Printf(TEXT("Property '%s' not found on %s"), *PropertyName, *Object->GetClass()->GetName()));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Set Property")));
	Object->PreEditChange(Prop);

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Object);
	FString OldValue = PropertyValueToString(Prop, ValuePtr);

	Prop->ImportText_Direct(*PropertyValue, ValuePtr, Object, PPF_None);

	FPropertyChangedEvent ChangedEvent(Prop);
	Object->PostEditChangeProperty(ChangedEvent);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("property"), PropertyName);
	Data->SetStringField(TEXT("old_value"), OldValue);
	Data->SetStringField(TEXT("new_value"), PropertyValue);
	return SuccessResponse(Data);
}

// --- Get Component Hierarchy ---
TSharedPtr<FJsonObject> FMCPGetComponentHierarchyCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName = Params->GetStringField(TEXT("actor_name"));

	AActor* Actor = FindActorByName(ActorName);
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

	AActor* Actor = FindActorByName(ActorName);
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
		return ErrorResponse(FString::Printf(TEXT("Component '%s' not found on actor '%s'"), *ComponentName, *ActorName));
	}

	FProperty* Prop = Component->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Prop)
	{
		return ErrorResponse(FString::Printf(TEXT("Property '%s' not found on component '%s'"), *PropertyName, *ComponentName));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Set Component Property")));
	Component->PreEditChange(Prop);

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Component);
	Prop->ImportText_Direct(*PropertyValue, ValuePtr, Component, PPF_None);

	FPropertyChangedEvent ChangedEvent(Prop);
	Component->PostEditChangeProperty(ChangedEvent);

	return SuccessResponse(FString::Printf(TEXT("Set %s.%s = %s"), *ComponentName, *PropertyName, *PropertyValue));
}
