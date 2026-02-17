#include "Commands/MCPBlueprintCommands.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
#include "Misc/ScopedSlowTask.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/SavePackage.h"
#include "Logging/TokenizedMessage.h"
#include "Components/PrimitiveComponent.h"

static UBlueprint* LoadBlueprintFromPath(const FString& AssetPath)
{
	UObject* Asset = StaticLoadObject(UBlueprint::StaticClass(), nullptr, *AssetPath);
	return Cast<UBlueprint>(Asset);
}

static UClass* FindClassByName(const FString& ClassName)
{
	// Try common classes first
	static TMap<FString, UClass*> CommonClasses;
	if (CommonClasses.Num() == 0)
	{
		CommonClasses.Add(TEXT("Actor"), AActor::StaticClass());
		CommonClasses.Add(TEXT("Pawn"), APawn::StaticClass());
		CommonClasses.Add(TEXT("Character"), ACharacter::StaticClass());
		CommonClasses.Add(TEXT("PlayerController"), APlayerController::StaticClass());
		CommonClasses.Add(TEXT("GameModeBase"), AGameModeBase::StaticClass());
		CommonClasses.Add(TEXT("ActorComponent"), UActorComponent::StaticClass());
		CommonClasses.Add(TEXT("SceneComponent"), USceneComponent::StaticClass());
	}

	if (UClass** Found = CommonClasses.Find(ClassName))
	{
		return *Found;
	}

	// Search by name
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->GetName() == ClassName || It->GetName() == FString::Printf(TEXT("A%s"), *ClassName) || It->GetName() == FString::Printf(TEXT("U%s"), *ClassName))
		{
			return *It;
		}
	}

	return nullptr;
}

// --- Create Blueprint ---
TSharedPtr<FJsonObject> FMCPCreateBlueprintCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Name = Params->GetStringField(TEXT("name"));
	FString ParentClassName = Params->GetStringField(TEXT("parent_class"));
	FString Path = Params->GetStringField(TEXT("path"));

	if (Name.IsEmpty())
	{
		return ErrorResponse(TEXT("Blueprint name is required"));
	}

	UClass* ParentClass = FindClassByName(ParentClassName);
	if (!ParentClass)
	{
		return ErrorResponse(FString::Printf(TEXT("Parent class '%s' not found"), *ParentClassName));
	}

	FString PackagePath = Path / Name;
	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to create package at '%s'"), *PackagePath));
	}

	UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(
		ParentClass,
		Package,
		*Name,
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass()
	);

	if (!NewBP)
	{
		return ErrorResponse(TEXT("Failed to create Blueprint"));
	}

	// Compile the new blueprint
	FKismetEditorUtilities::CompileBlueprint(NewBP);

	// Save the package
	FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, NewBP, *PackageFileName, SaveArgs);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Name);
	Data->SetStringField(TEXT("asset_path"), NewBP->GetPathName());
	Data->SetStringField(TEXT("parent_class"), ParentClass->GetName());
	return SuccessResponse(Data);
}

// --- List Blueprints ---
TSharedPtr<FJsonObject> FMCPListBlueprintsCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Path = Params->GetStringField(TEXT("path"));
	bool bRecursive = true;
	Params->TryGetBoolField(TEXT("recursive"), bRecursive);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(*Path);
	Filter.bRecursivePaths = bRecursive;

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	TArray<TSharedPtr<FJsonValue>> BlueprintArray;
	for (const FAssetData& Asset : Assets)
	{
		TSharedPtr<FJsonObject> BPInfo = MakeShared<FJsonObject>();
		BPInfo->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		BPInfo->SetStringField(TEXT("asset_path"), Asset.GetObjectPathString());
		BPInfo->SetStringField(TEXT("package_path"), Asset.PackagePath.ToString());

		FString ParentClassName;
		Asset.GetTagValue(FBlueprintTags::ParentClassPath, ParentClassName);
		BPInfo->SetStringField(TEXT("parent_class"), ParentClassName);

		BlueprintArray.Add(MakeShared<FJsonValueObject>(BPInfo));
	}

	return SuccessResponse(BlueprintArray);
}

// --- Get Blueprint Info ---
TSharedPtr<FJsonObject> FMCPGetBlueprintInfoCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UBlueprint* BP = LoadBlueprintFromPath(AssetPath);
	if (!BP)
	{
		return ErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), BP->GetName());
	Data->SetStringField(TEXT("parent_class"), BP->ParentClass ? BP->ParentClass->GetName() : TEXT("None"));

	// Variables
	TArray<TSharedPtr<FJsonValue>> Variables;
	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		TSharedPtr<FJsonObject> VarInfo = MakeShared<FJsonObject>();
		VarInfo->SetStringField(TEXT("name"), Var.VarName.ToString());
		VarInfo->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());
		VarInfo->SetStringField(TEXT("category"), Var.Category.ToString());
		VarInfo->SetBoolField(TEXT("instance_editable"), Var.PropertyFlags & CPF_Edit ? true : false);
		Variables.Add(MakeShared<FJsonValueObject>(VarInfo));
	}
	Data->SetArrayField(TEXT("variables"), Variables);

	// Functions
	TArray<TSharedPtr<FJsonValue>> Functions;
	for (UEdGraph* Graph : BP->FunctionGraphs)
	{
		TSharedPtr<FJsonObject> FuncInfo = MakeShared<FJsonObject>();
		FuncInfo->SetStringField(TEXT("name"), Graph->GetName());
		FuncInfo->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
		Functions.Add(MakeShared<FJsonValueObject>(FuncInfo));
	}
	Data->SetArrayField(TEXT("functions"), Functions);

	// Graphs
	TArray<TSharedPtr<FJsonValue>> Graphs;
	for (UEdGraph* Graph : BP->UbergraphPages)
	{
		TSharedPtr<FJsonObject> GraphInfo = MakeShared<FJsonObject>();
		GraphInfo->SetStringField(TEXT("name"), Graph->GetName());
		GraphInfo->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
		Graphs.Add(MakeShared<FJsonValueObject>(GraphInfo));
	}
	Data->SetArrayField(TEXT("event_graphs"), Graphs);

	// Components (SCS)
	TArray<TSharedPtr<FJsonValue>> Components;
	if (BP->SimpleConstructionScript)
	{
		for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
		{
			TSharedPtr<FJsonObject> CompInfo = MakeShared<FJsonObject>();
			CompInfo->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
			CompInfo->SetStringField(TEXT("class"), Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT("None"));
			Components.Add(MakeShared<FJsonValueObject>(CompInfo));
		}
	}
	Data->SetArrayField(TEXT("components"), Components);

	return SuccessResponse(Data);
}

// --- Compile Blueprint ---
TSharedPtr<FJsonObject> FMCPCompileBlueprintCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UBlueprint* BP = LoadBlueprintFromPath(AssetPath);
	if (!BP)
	{
		return ErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	// Force the Blueprint into dirty state so CompileBlueprint always regenerates bytecode
	// Without this, CompileBlueprint may skip recompilation if Status is already BS_UpToDate
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), BP->GetName());
	Data->SetBoolField(TEXT("has_errors"), BP->Status == BS_Error);
	Data->SetStringField(TEXT("status"),
		BP->Status == BS_Error ? TEXT("Error") :
		BP->Status == BS_UpToDate ? TEXT("UpToDate") :
		TEXT("Dirty"));

	// Collect detailed compilation messages from all graphs and nodes
	TArray<TSharedPtr<FJsonValue>> Errors;
	TArray<TSharedPtr<FJsonValue>> Warnings;

	TArray<UEdGraph*> AllGraphs;
	BP->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node->bHasCompilerMessage && !Node->ErrorMsg.IsEmpty())
			{
				TSharedPtr<FJsonObject> MsgInfo = MakeShared<FJsonObject>();
				MsgInfo->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
				MsgInfo->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
				MsgInfo->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
				MsgInfo->SetStringField(TEXT("graph"), Graph->GetName());
				MsgInfo->SetStringField(TEXT("message"), Node->ErrorMsg);
				MsgInfo->SetNumberField(TEXT("pos_x"), Node->NodePosX);
				MsgInfo->SetNumberField(TEXT("pos_y"), Node->NodePosY);

				// ErrorType maps to EMessageSeverity::Type (0=CriticalError, 1=Error, 2=PerfWarning, 3=Warning, 4=Info)
				bool bIsError = Node->ErrorType <= EMessageSeverity::Error;
				MsgInfo->SetStringField(TEXT("severity"), bIsError ? TEXT("Error") : TEXT("Warning"));

				if (bIsError)
					Errors.Add(MakeShared<FJsonValueObject>(MsgInfo));
				else
					Warnings.Add(MakeShared<FJsonValueObject>(MsgInfo));
			}
		}
	}

	Data->SetArrayField(TEXT("errors"), Errors);
	Data->SetArrayField(TEXT("warnings"), Warnings);
	Data->SetNumberField(TEXT("error_count"), Errors.Num());
	Data->SetNumberField(TEXT("warning_count"), Warnings.Num());

	return SuccessResponse(Data);
}

// --- Delete Blueprint ---
TSharedPtr<FJsonObject> FMCPDeleteBlueprintCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		return ErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
	}

	bool bDeleted = UEditorAssetLibrary::DeleteAsset(AssetPath);
	if (!bDeleted)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to delete: %s"), *AssetPath));
	}

	return SuccessResponse(FString::Printf(TEXT("Deleted: %s"), *AssetPath));
}

// --- Add Blueprint Variable ---
TSharedPtr<FJsonObject> FMCPAddBlueprintVariableCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString VarName = Params->GetStringField(TEXT("variable_name"));
	FString VarType = Params->GetStringField(TEXT("variable_type"));

	UBlueprint* BP = LoadBlueprintFromPath(AssetPath);
	if (!BP)
	{
		return ErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	// Map type string to pin type
	FEdGraphPinType PinType;
	if (VarType == TEXT("Boolean")) PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	else if (VarType == TEXT("Byte")) PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
	else if (VarType == TEXT("Integer")) PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	else if (VarType == TEXT("Integer64")) PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
	else if (VarType == TEXT("Float")) PinType.PinCategory = UEdGraphSchema_K2::PC_Float;
	else if (VarType == TEXT("Double")) PinType.PinCategory = UEdGraphSchema_K2::PC_Double;
	else if (VarType == TEXT("String")) PinType.PinCategory = UEdGraphSchema_K2::PC_String;
	else if (VarType == TEXT("Text")) PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
	else if (VarType == TEXT("Name")) PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
	else if (VarType == TEXT("Vector"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (VarType == TEXT("Rotator"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else if (VarType == TEXT("Transform"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	}
	else
	{
		return ErrorResponse(FString::Printf(TEXT("Unsupported variable type: %s"), *VarType));
	}

	bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(BP, FName(*VarName), PinType);
	if (!bSuccess)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to add variable '%s'"), *VarName));
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("variable_name"), VarName);
	Data->SetStringField(TEXT("variable_type"), VarType);
	return SuccessResponse(Data);
}

// --- Remove Blueprint Variable ---
TSharedPtr<FJsonObject> FMCPRemoveBlueprintVariableCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString VarName = Params->GetStringField(TEXT("variable_name"));

	UBlueprint* BP = LoadBlueprintFromPath(AssetPath);
	if (!BP)
	{
		return ErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	FBlueprintEditorUtils::RemoveMemberVariable(BP, FName(*VarName));
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

	return SuccessResponse(FString::Printf(TEXT("Removed variable: %s"), *VarName));
}

// --- Add Blueprint Component ---
TSharedPtr<FJsonObject> FMCPAddBlueprintComponentCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ComponentClassName = Params->GetStringField(TEXT("component_class"));
	FString ComponentName = Params->GetStringField(TEXT("component_name"));

	UBlueprint* BP = LoadBlueprintFromPath(AssetPath);
	if (!BP)
	{
		return ErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	UClass* ComponentClass = FindClassByName(ComponentClassName);
	if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return ErrorResponse(FString::Printf(TEXT("Invalid component class: %s"), *ComponentClassName));
	}

	if (!BP->SimpleConstructionScript)
	{
		return ErrorResponse(TEXT("Blueprint does not have a SimpleConstructionScript"));
	}

	USCS_Node* NewNode = BP->SimpleConstructionScript->CreateNode(ComponentClass, *ComponentName);
	if (!NewNode)
	{
		return ErrorResponse(TEXT("Failed to create SCS node"));
	}

	// Handle parenting
	FString ParentComponent = Params->GetStringField(TEXT("parent_component"));
	if (!ParentComponent.IsEmpty())
	{
		USCS_Node* ParentNode = nullptr;
		for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
		{
			if (Node->GetVariableName().ToString() == ParentComponent)
			{
				ParentNode = Node;
				break;
			}
		}
		if (ParentNode)
		{
			ParentNode->AddChildNode(NewNode);
		}
		else
		{
			BP->SimpleConstructionScript->AddNode(NewNode);
		}
	}
	else
	{
		BP->SimpleConstructionScript->AddNode(NewNode);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("component_name"), NewNode->GetVariableName().ToString());
	Data->SetStringField(TEXT("component_class"), ComponentClass->GetName());
	return SuccessResponse(Data);
}

// --- Set Blueprint Component Defaults ---
TSharedPtr<FJsonObject> FMCPSetBlueprintComponentDefaultsCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ComponentName = Params->GetStringField(TEXT("component_name"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	FString PropertyValue = Params->GetStringField(TEXT("property_value"));

	if (ComponentName.IsEmpty() || PropertyName.IsEmpty())
	{
		return ErrorResponse(TEXT("component_name and property_name are required"));
	}

	UBlueprint* BP = LoadBlueprintFromPath(AssetPath);
	if (!BP)
	{
		return ErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	if (!BP->SimpleConstructionScript)
	{
		return ErrorResponse(TEXT("Blueprint does not have a SimpleConstructionScript (not an Actor-based BP?)"));
	}

	// Find the SCS node by component variable name
	USCS_Node* TargetNode = nullptr;
	for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
	{
		if (Node->GetVariableName().ToString() == ComponentName)
		{
			TargetNode = Node;
			break;
		}
	}

	if (!TargetNode)
	{
		// Build list of available components for the error message
		TArray<FString> AvailableNames;
		for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
		{
			AvailableNames.Add(Node->GetVariableName().ToString());
		}
		return ErrorResponse(FString::Printf(TEXT("Component '%s' not found in SCS. Available: [%s]"),
			*ComponentName, *FString::Join(AvailableNames, TEXT(", "))));
	}

	UActorComponent* ComponentTemplate = TargetNode->ComponentTemplate;
	if (!ComponentTemplate)
	{
		return ErrorResponse(FString::Printf(TEXT("Component '%s' has no template object"), *ComponentName));
	}

	// Special case: CollisionProfileName on primitive components (stored in BodyInstance struct)
	if (PropertyName == TEXT("CollisionProfileName") || PropertyName == TEXT("BodyInstance.CollisionProfileName"))
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(ComponentTemplate);
		if (!PrimComp)
		{
			return ErrorResponse(FString::Printf(TEXT("Component '%s' is not a PrimitiveComponent, cannot set CollisionProfileName"), *ComponentName));
		}
		FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("MCP: Set %s.CollisionProfileName = %s"), *ComponentName, *PropertyValue)));
		PrimComp->SetCollisionProfileName(FName(*PropertyValue));
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("component_name"), ComponentName);
		Data->SetStringField(TEXT("property_name"), TEXT("CollisionProfileName"));
		Data->SetStringField(TEXT("property_value"), PropertyValue);
		Data->SetStringField(TEXT("component_class"), ComponentTemplate->GetClass()->GetName());
		return SuccessResponse(Data);
	}

	// Resolve property - support dot-notation for nested struct properties (e.g. "BodyInstance.bNotifyRigidBodyCollision")
	FProperty* Property = nullptr;
	void* ContainerPtr = ComponentTemplate;

	if (PropertyName.Contains(TEXT(".")))
	{
		TArray<FString> PropertyChain;
		PropertyName.ParseIntoArray(PropertyChain, TEXT("."), true);

		UStruct* CurrentStruct = ComponentTemplate->GetClass();
		void* CurrentPtr = ComponentTemplate;

		for (int32 i = 0; i < PropertyChain.Num(); ++i)
		{
			FProperty* ChainProp = CurrentStruct->FindPropertyByName(*PropertyChain[i]);
			if (!ChainProp)
			{
				return ErrorResponse(FString::Printf(TEXT("Property '%s' not found in chain '%s' on component '%s' (class: %s)"),
					*PropertyChain[i], *PropertyName, *ComponentName, *ComponentTemplate->GetClass()->GetName()));
			}

			if (i == PropertyChain.Num() - 1)
			{
				// Last in chain - this is the target property
				Property = ChainProp;
				ContainerPtr = CurrentPtr;
			}
			else
			{
				// Intermediate struct - navigate into it
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
		Property = ComponentTemplate->GetClass()->FindPropertyByName(*PropertyName);
		ContainerPtr = ComponentTemplate;
	}

	if (!Property)
	{
		return ErrorResponse(FString::Printf(TEXT("Property '%s' not found on component '%s' (class: %s)"),
			*PropertyName, *ComponentName, *ComponentTemplate->GetClass()->GetName()));
	}

	// Set the property value
	FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("MCP: Set %s.%s = %s"), *ComponentName, *PropertyName, *PropertyValue)));
	ComponentTemplate->PreEditChange(Property);

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ContainerPtr);
	if (!Property->ImportText_Direct(*PropertyValue, ValuePtr, ComponentTemplate, PPF_None))
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to set '%s' to '%s' (ImportText_Direct failed). Check value format."),
			*PropertyName, *PropertyValue));
	}

	FPropertyChangedEvent ChangeEvent(Property);
	ComponentTemplate->PostEditChangeProperty(ChangeEvent);

	// Mark the blueprint as modified so it recompiles with the new defaults
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("component_name"), ComponentName);
	Data->SetStringField(TEXT("property_name"), PropertyName);
	Data->SetStringField(TEXT("property_value"), PropertyValue);
	Data->SetStringField(TEXT("component_class"), ComponentTemplate->GetClass()->GetName());
	return SuccessResponse(Data);
}
