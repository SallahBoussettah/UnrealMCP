#include "Commands/MCPAnimCommands.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"

// Animation core
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"

// Factories
#include "Factories/AnimBlueprintFactory.h"
#include "Factories/BlendSpaceFactory1D.h"
#include "Factories/BlendSpaceFactoryNew.h"
#include "Factories/AnimMontageFactory.h"

// AnimGraph editor nodes
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimStateEntryNode.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_Root.h"
#include "AnimationStateGraph.h"
#include "AnimationTransitionGraph.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimGraphNode_StateResult.h"
#include "K2Node_VariableGet.h"

namespace AnimCommandsLocal
{

// ============================================================================
// Helpers
// ============================================================================

static FString EnsureObjectPath(const FString& AssetPath)
{
	FString FullPath = AssetPath;
	if (!FullPath.Contains(TEXT(".")))
	{
		FString AssetName = FPaths::GetBaseFilename(FullPath);
		FullPath = FullPath + TEXT(".") + AssetName;
	}
	return FullPath;
}

static UAnimBlueprint* LoadAnimBP(const FString& AssetPath)
{
	FString FullPath = EnsureObjectPath(AssetPath);
	UObject* Loaded = StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *FullPath);
	return Cast<UAnimBlueprint>(Loaded);
}

static USkeleton* LoadSkeleton(const FString& SkeletonPath)
{
	if (SkeletonPath.IsEmpty()) return nullptr;

	FString FullPath = EnsureObjectPath(SkeletonPath);

	// Try loading as USkeleton first
	UObject* Loaded = StaticLoadObject(USkeleton::StaticClass(), nullptr, *FullPath);
	if (USkeleton* Skeleton = Cast<USkeleton>(Loaded))
	{
		return Skeleton;
	}

	// Try loading as USkeletalMesh and getting its skeleton
	Loaded = StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *FullPath);
	if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Loaded))
	{
		return Mesh->GetSkeleton();
	}

	return nullptr;
}

static UEdGraph* FindAnimGraph(UAnimBlueprint* AnimBP)
{
	TArray<UEdGraph*> AllGraphs;
	AnimBP->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph->GetName() == TEXT("AnimGraph"))
		{
			return Graph;
		}
	}
	return nullptr;
}

static UAnimationStateMachineGraph* FindStateMachineGraph(UAnimBlueprint* AnimBP, const FString& SMName)
{
	TArray<UEdGraph*> AllGraphs;
	AnimBP->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node);
			if (SMNode && SMNode->EditorStateMachineGraph)
			{
				if (SMName.IsEmpty())
				{
					return SMNode->EditorStateMachineGraph;
				}

				FString NodeTitle = SMNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
				if (NodeTitle.Contains(SMName))
				{
					return SMNode->EditorStateMachineGraph;
				}
			}
		}
	}

	return nullptr;
}

static UAnimStateNode* FindStateByName(UAnimationStateMachineGraph* SMGraph, const FString& StateName)
{
	if (!SMGraph) return nullptr;

	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node);
		if (StateNode && StateNode->BoundGraph)
		{
			if (StateNode->BoundGraph->GetName() == StateName)
			{
				return StateNode;
			}
		}
	}
	return nullptr;
}

static UAnimStateTransitionNode* FindTransitionBetween(
	UAnimationStateMachineGraph* SMGraph,
	UAnimStateNode* Source,
	UAnimStateNode* Target)
{
	if (!SMGraph || !Source || !Target) return nullptr;

	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		UAnimStateTransitionNode* TransNode = Cast<UAnimStateTransitionNode>(Node);
		if (!TransNode) continue;

		UEdGraphPin* TransInputPin = TransNode->GetInputPin();
		UEdGraphPin* TransOutputPin = TransNode->GetOutputPin();

		bool bFromSource = false;
		bool bToTarget = false;

		if (TransInputPin)
		{
			for (UEdGraphPin* Link : TransInputPin->LinkedTo)
			{
				if (Link->GetOwningNode() == Source)
				{
					bFromSource = true;
					break;
				}
			}
		}

		if (TransOutputPin)
		{
			for (UEdGraphPin* Link : TransOutputPin->LinkedTo)
			{
				if (Link->GetOwningNode() == Target)
				{
					bToTarget = true;
					break;
				}
			}
		}

		if (bFromSource && bToTarget)
		{
			return TransNode;
		}
	}

	return nullptr;
}

static TSharedPtr<FJsonObject> StateToJson(UAnimStateNode* StateNode)
{
	if (!StateNode) return nullptr;

	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("name"), StateNode->BoundGraph ? StateNode->BoundGraph->GetName() : TEXT("unnamed"));
	Obj->SetNumberField(TEXT("pos_x"), StateNode->NodePosX);
	Obj->SetNumberField(TEXT("pos_y"), StateNode->NodePosY);

	// Check for animation in state's graph
	if (UAnimationStateGraph* StateGraph = Cast<UAnimationStateGraph>(StateNode->BoundGraph))
	{
		for (UEdGraphNode* InnerNode : StateGraph->Nodes)
		{
			if (UAnimGraphNode_SequencePlayer* SeqPlayer = Cast<UAnimGraphNode_SequencePlayer>(InnerNode))
			{
				if (SeqPlayer->Node.GetSequence())
				{
					Obj->SetStringField(TEXT("animation"), SeqPlayer->Node.GetSequence()->GetPathName());
				}
				break;
			}
		}
	}

	// Serialize outgoing transitions
	TArray<TSharedPtr<FJsonValue>> TransArr;
	if (UEdGraphPin* OutputPin = StateNode->GetOutputPin())
	{
		for (UEdGraphPin* Link : OutputPin->LinkedTo)
		{
			if (UAnimStateTransitionNode* TransNode = Cast<UAnimStateTransitionNode>(Link->GetOwningNode()))
			{
				TSharedPtr<FJsonObject> TransObj = MakeShared<FJsonObject>();
				TransObj->SetNumberField(TEXT("crossfade_duration"), TransNode->CrossfadeDuration);

				// Find target state
				if (UEdGraphPin* TransOutput = TransNode->GetOutputPin())
				{
					for (UEdGraphPin* TargetLink : TransOutput->LinkedTo)
					{
						if (UAnimStateNode* TargetState = Cast<UAnimStateNode>(TargetLink->GetOwningNode()))
						{
							TransObj->SetStringField(TEXT("target"),
								TargetState->BoundGraph ? TargetState->BoundGraph->GetName() : TEXT("unnamed"));
							break;
						}
					}
				}

				TransObj->SetBoolField(TEXT("automatic_rule"),
					TransNode->bAutomaticRuleBasedOnSequencePlayerInState);

				TransArr.Add(MakeShared<FJsonValueObject>(TransObj));
			}
		}
	}
	Obj->SetArrayField(TEXT("transitions"), TransArr);

	return Obj;
}

static void SaveAssetPackage(UObject* Asset)
{
	if (!Asset) return;

	UPackage* Package = Asset->GetOutermost();
	FString PackagePath = Package->GetName();
	FString PackageFileName = FPackageName::LongPackageNameToFilename(
		PackagePath, FPackageName::GetAssetPackageExtension());

	FAssetRegistryModule::AssetCreated(Asset);
	Package->FullyLoad();
	Package->SetDirtyFlag(true);

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);
}

} // namespace AnimCommandsLocal

using namespace AnimCommandsLocal;

// ============================================================================
// create_anim_blueprint
// ============================================================================

TSharedPtr<FJsonObject> FMCPCreateAnimBlueprintCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Name = Params->GetStringField(TEXT("name"));
	FString Path = Params->GetStringField(TEXT("path"));
	FString SkeletonPath = Params->GetStringField(TEXT("skeleton_path"));
	FString SkeletalMeshPath = Params->GetStringField(TEXT("skeletal_mesh_path"));

	if (Name.IsEmpty())
	{
		return ErrorResponse(TEXT("Animation Blueprint name is required"));
	}
	if (Path.IsEmpty())
	{
		Path = TEXT("/Game/Animations");
	}

	// Load skeleton (try skeleton_path first, then skeletal_mesh_path)
	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton && !SkeletalMeshPath.IsEmpty())
	{
		Skeleton = LoadSkeleton(SkeletalMeshPath);
	}
	if (!Skeleton)
	{
		return ErrorResponse(FString::Printf(
			TEXT("Could not load skeleton from '%s' or skeletal mesh from '%s'"),
			*SkeletonPath, *SkeletalMeshPath));
	}

	FString PackagePath = Path / Name;
	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to create package at '%s'"), *PackagePath));
	}

	// Create AnimBP using factory
	UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
	Factory->ParentClass = UAnimInstance::StaticClass();
	Factory->TargetSkeleton = Skeleton;

	// Set preview mesh if available
	if (!SkeletalMeshPath.IsEmpty())
	{
		FString MeshFullPath = EnsureObjectPath(SkeletalMeshPath);
		UObject* MeshObj = StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *MeshFullPath);
		if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(MeshObj))
		{
			Factory->PreviewSkeletalMesh = Mesh;
		}
	}

	UObject* NewAsset = Factory->FactoryCreateNew(
		UAnimBlueprint::StaticClass(),
		Package,
		FName(*Name),
		RF_Public | RF_Standalone,
		nullptr,
		GWarn);

	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(NewAsset);
	if (!AnimBP)
	{
		return ErrorResponse(TEXT("Failed to create Animation Blueprint"));
	}

	// Find the AnimGraph and add a default state machine
	UEdGraph* AnimGraph = FindAnimGraph(AnimBP);
	if (AnimGraph)
	{
		FScopedTransaction Transaction(FText::FromString(TEXT("MCP Add Default State Machine")));

		UAnimGraphNode_StateMachine* SMNode = NewObject<UAnimGraphNode_StateMachine>(AnimGraph);
		SMNode->CreateNewGuid();
		AnimGraph->AddNode(SMNode, false, false);
		SMNode->PostPlacedNewNode();
		SMNode->AllocateDefaultPins();
		SMNode->NodePosX = -300;
		SMNode->NodePosY = 0;

		// Connect SM output to the Output Pose (Root) node
		UAnimGraphNode_Root* RootNode = nullptr;
		for (UEdGraphNode* Node : AnimGraph->Nodes)
		{
			RootNode = Cast<UAnimGraphNode_Root>(Node);
			if (RootNode) break;
		}

		if (RootNode && SMNode->Pins.Num() > 0 && RootNode->Pins.Num() > 0)
		{
			UEdGraphPin* SMOutput = nullptr;
			for (UEdGraphPin* Pin : SMNode->Pins)
			{
				if (Pin->Direction == EGPD_Output)
				{
					SMOutput = Pin;
					break;
				}
			}

			UEdGraphPin* RootInput = nullptr;
			for (UEdGraphPin* Pin : RootNode->Pins)
			{
				if (Pin->Direction == EGPD_Input)
				{
					RootInput = Pin;
					break;
				}
			}

			if (SMOutput && RootInput)
			{
				const UEdGraphSchema* Schema = AnimGraph->GetSchema();
				if (Schema)
				{
					Schema->TryCreateConnection(SMOutput, RootInput);
				}
			}
		}
	}

	// Compile and save
	FKismetEditorUtilities::CompileBlueprint(AnimBP);
	SaveAssetPackage(AnimBP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Name);
	Data->SetStringField(TEXT("asset_path"), AnimBP->GetPathName());
	Data->SetStringField(TEXT("skeleton"), Skeleton->GetPathName());
	Data->SetBoolField(TEXT("has_state_machine"), AnimGraph != nullptr);
	return SuccessResponse(Data);
}

// ============================================================================
// add_anim_state
// ============================================================================

TSharedPtr<FJsonObject> FMCPAddAnimStateCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString StateName = Params->GetStringField(TEXT("state_name"));
	FString AnimAssetPath = Params->GetStringField(TEXT("animation_asset"));
	FString SMName = Params->GetStringField(TEXT("state_machine_name"));

	if (AssetPath.IsEmpty() || StateName.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_path and state_name are required"));
	}

	UAnimBlueprint* AnimBP = LoadAnimBP(AssetPath);
	if (!AnimBP)
	{
		return ErrorResponse(FString::Printf(TEXT("Animation Blueprint not found: %s"), *AssetPath));
	}

	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(AnimBP, SMName);
	if (!SMGraph)
	{
		return ErrorResponse(TEXT("No state machine found in the Animation Blueprint. Create the AnimBP with create_anim_blueprint first."));
	}

	// Check for duplicate state name
	if (FindStateByName(SMGraph, StateName))
	{
		return ErrorResponse(FString::Printf(TEXT("State '%s' already exists in the state machine"), *StateName));
	}

	// Parse optional position
	int32 PosX = 200;
	int32 PosY = 0;
	const TArray<TSharedPtr<FJsonValue>>* PosArr;
	if (Params->TryGetArrayField(TEXT("position"), PosArr) && PosArr->Num() >= 2)
	{
		PosX = static_cast<int32>((*PosArr)[0]->AsNumber());
		PosY = static_cast<int32>((*PosArr)[1]->AsNumber());
	}
	else
	{
		// Auto-position: count existing states and offset
		int32 StateCount = 0;
		for (UEdGraphNode* Node : SMGraph->Nodes)
		{
			if (Cast<UAnimStateNode>(Node))
			{
				StateCount++;
			}
		}
		PosX = 300 + (StateCount % 3) * 300;
		PosY = (StateCount / 3) * 200;
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Add Anim State")));
	AnimBP->Modify();

	// Create the state node
	UAnimStateNode* StateNode = NewObject<UAnimStateNode>(SMGraph);
	StateNode->CreateNewGuid();
	SMGraph->AddNode(StateNode, false, false);
	StateNode->PostPlacedNewNode();
	StateNode->AllocateDefaultPins();
	StateNode->NodePosX = PosX;
	StateNode->NodePosY = PosY;

	// Rename the state (the state's name comes from its BoundGraph name)
	if (StateNode->BoundGraph)
	{
		FBlueprintEditorUtils::RenameGraph(StateNode->BoundGraph, StateName);
	}

	// If this is the first state, connect the entry node to it
	{
		int32 StateCountAfter = 0;
		for (UEdGraphNode* Node : SMGraph->Nodes)
		{
			if (Cast<UAnimStateNode>(Node)) StateCountAfter++;
		}

		if (StateCountAfter == 1)
		{
			for (UEdGraphNode* Node : SMGraph->Nodes)
			{
				UAnimStateEntryNode* EntryNode = Cast<UAnimStateEntryNode>(Node);
				if (EntryNode)
				{
					UEdGraphPin* EntryOutput = nullptr;
					for (UEdGraphPin* Pin : EntryNode->Pins)
					{
						if (Pin->Direction == EGPD_Output)
						{
							EntryOutput = Pin;
							break;
						}
					}

					UEdGraphPin* StateInput = StateNode->GetInputPin();
					if (EntryOutput && StateInput)
					{
						const UEdGraphSchema* Schema = SMGraph->GetSchema();
						if (Schema)
						{
							Schema->TryCreateConnection(EntryOutput, StateInput);
						}
					}
					break;
				}
			}
		}
	}

	// Optionally set up animation in the state's graph
	FString AnimInfo = TEXT("none");
	if (!AnimAssetPath.IsEmpty())
	{
		FString AnimFullPath = EnsureObjectPath(AnimAssetPath);
		UObject* AnimObj = StaticLoadObject(UAnimSequenceBase::StaticClass(), nullptr, *AnimFullPath);
		UAnimSequenceBase* AnimAsset = Cast<UAnimSequenceBase>(AnimObj);

		if (AnimAsset)
		{
			UAnimationStateGraph* StateGraph = Cast<UAnimationStateGraph>(StateNode->BoundGraph);
			if (StateGraph)
			{
				// Create sequence player node
				UAnimGraphNode_SequencePlayer* SeqPlayer = NewObject<UAnimGraphNode_SequencePlayer>(StateGraph);
				SeqPlayer->CreateNewGuid();
				StateGraph->AddNode(SeqPlayer, false, false);
				SeqPlayer->AllocateDefaultPins();
				SeqPlayer->NodePosX = -300;
				SeqPlayer->NodePosY = 0;

				// Set the animation sequence
				SeqPlayer->Node.SetSequence(AnimAsset);

				// Connect to the state result node
				if (StateGraph->MyResultNode)
				{
					UEdGraphPin* SeqOutput = nullptr;
					for (UEdGraphPin* Pin : SeqPlayer->Pins)
					{
						if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
						{
							SeqOutput = Pin;
							break;
						}
					}

					UEdGraphPin* ResultInput = nullptr;
					for (UEdGraphPin* Pin : StateGraph->MyResultNode->Pins)
					{
						if (Pin->Direction == EGPD_Input)
						{
							ResultInput = Pin;
							break;
						}
					}

					if (SeqOutput && ResultInput)
					{
						const UEdGraphSchema* Schema = StateGraph->GetSchema();
						if (Schema)
						{
							Schema->TryCreateConnection(SeqOutput, ResultInput);
						}
					}
				}

				AnimInfo = AnimAsset->GetName();
			}
		}
		else
		{
			AnimInfo = FString::Printf(TEXT("WARNING: Could not load animation '%s'"), *AnimAssetPath);
		}
	}

	// Compile
	FKismetEditorUtilities::CompileBlueprint(AnimBP);
	AnimBP->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("state_name"), StateName);
	Data->SetNumberField(TEXT("pos_x"), PosX);
	Data->SetNumberField(TEXT("pos_y"), PosY);
	Data->SetStringField(TEXT("animation"), AnimInfo);
	return SuccessResponse(Data);
}

// ============================================================================
// add_anim_transition
// ============================================================================

TSharedPtr<FJsonObject> FMCPAddAnimTransitionCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString SourceStateName = Params->GetStringField(TEXT("source_state"));
	FString TargetStateName = Params->GetStringField(TEXT("target_state"));
	FString SMName = Params->GetStringField(TEXT("state_machine_name"));

	double Duration = 0.2;
	Params->TryGetNumberField(TEXT("duration"), Duration);

	FString BlendMode = TEXT("Linear");
	Params->TryGetStringField(TEXT("blend_mode"), BlendMode);

	if (AssetPath.IsEmpty() || SourceStateName.IsEmpty() || TargetStateName.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_path, source_state, and target_state are required"));
	}

	UAnimBlueprint* AnimBP = LoadAnimBP(AssetPath);
	if (!AnimBP)
	{
		return ErrorResponse(FString::Printf(TEXT("Animation Blueprint not found: %s"), *AssetPath));
	}

	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(AnimBP, SMName);
	if (!SMGraph)
	{
		return ErrorResponse(TEXT("No state machine found in the Animation Blueprint"));
	}

	UAnimStateNode* SourceState = FindStateByName(SMGraph, SourceStateName);
	if (!SourceState)
	{
		return ErrorResponse(FString::Printf(TEXT("Source state not found: %s"), *SourceStateName));
	}

	UAnimStateNode* TargetState = FindStateByName(SMGraph, TargetStateName);
	if (!TargetState)
	{
		return ErrorResponse(FString::Printf(TEXT("Target state not found: %s"), *TargetStateName));
	}

	// Check if transition already exists
	if (FindTransitionBetween(SMGraph, SourceState, TargetState))
	{
		return ErrorResponse(FString::Printf(TEXT("Transition from '%s' to '%s' already exists"),
			*SourceStateName, *TargetStateName));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Add Anim Transition")));
	AnimBP->Modify();

	// Create transition node
	UAnimStateTransitionNode* TransNode = NewObject<UAnimStateTransitionNode>(SMGraph);
	TransNode->CreateNewGuid();
	SMGraph->AddNode(TransNode, false, false);
	TransNode->PostPlacedNewNode();
	TransNode->AllocateDefaultPins();

	// Position between the two states
	TransNode->NodePosX = (SourceState->NodePosX + TargetState->NodePosX) / 2;
	TransNode->NodePosY = (SourceState->NodePosY + TargetState->NodePosY) / 2;

	// Connect: Source output -> Transition input, Transition output -> Target input
	const UEdGraphSchema* Schema = SMGraph->GetSchema();
	if (Schema)
	{
		UEdGraphPin* SourceOutput = SourceState->GetOutputPin();
		UEdGraphPin* TransInput = TransNode->GetInputPin();
		UEdGraphPin* TransOutput = TransNode->GetOutputPin();
		UEdGraphPin* TargetInput = TargetState->GetInputPin();

		if (SourceOutput && TransInput)
		{
			Schema->TryCreateConnection(SourceOutput, TransInput);
		}
		if (TransOutput && TargetInput)
		{
			Schema->TryCreateConnection(TransOutput, TargetInput);
		}
	}

	// Set crossfade duration
	TransNode->CrossfadeDuration = static_cast<float>(Duration);

	// Set blend mode
	if (BlendMode == TEXT("HermiteCubic"))
	{
		TransNode->BlendMode = EAlphaBlendOption::HermiteCubic;
	}
	else if (BlendMode == TEXT("Sinusoidal"))
	{
		TransNode->BlendMode = EAlphaBlendOption::Sinusoidal;
	}
	else if (BlendMode == TEXT("QuadraticInOut"))
	{
		TransNode->BlendMode = EAlphaBlendOption::QuadraticInOut;
	}
	else if (BlendMode == TEXT("CubicInOut"))
	{
		TransNode->BlendMode = EAlphaBlendOption::CubicInOut;
	}
	else if (BlendMode == TEXT("QuarticInOut"))
	{
		TransNode->BlendMode = EAlphaBlendOption::QuarticInOut;
	}
	else if (BlendMode == TEXT("QuinticInOut"))
	{
		TransNode->BlendMode = EAlphaBlendOption::QuinticInOut;
	}
	else if (BlendMode == TEXT("CircularIn"))
	{
		TransNode->BlendMode = EAlphaBlendOption::CircularIn;
	}
	else if (BlendMode == TEXT("CircularOut"))
	{
		TransNode->BlendMode = EAlphaBlendOption::CircularOut;
	}
	else if (BlendMode == TEXT("CircularInOut"))
	{
		TransNode->BlendMode = EAlphaBlendOption::CircularInOut;
	}
	else if (BlendMode == TEXT("ExpIn"))
	{
		TransNode->BlendMode = EAlphaBlendOption::ExpIn;
	}
	else if (BlendMode == TEXT("ExpOut"))
	{
		TransNode->BlendMode = EAlphaBlendOption::ExpOut;
	}
	else if (BlendMode == TEXT("ExpInOut"))
	{
		TransNode->BlendMode = EAlphaBlendOption::ExpInOut;
	}
	else
	{
		TransNode->BlendMode = EAlphaBlendOption::Linear;
	}

	// Compile
	FKismetEditorUtilities::CompileBlueprint(AnimBP);
	AnimBP->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("source_state"), SourceStateName);
	Data->SetStringField(TEXT("target_state"), TargetStateName);
	Data->SetNumberField(TEXT("duration"), Duration);
	Data->SetStringField(TEXT("blend_mode"), BlendMode);
	return SuccessResponse(Data);
}

// ============================================================================
// set_anim_transition_rule
// ============================================================================

TSharedPtr<FJsonObject> FMCPSetAnimTransitionRuleCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString SourceStateName = Params->GetStringField(TEXT("source_state"));
	FString TargetStateName = Params->GetStringField(TEXT("target_state"));
	FString RuleType = Params->GetStringField(TEXT("rule_type"));
	FString SMName = Params->GetStringField(TEXT("state_machine_name"));

	const TSharedPtr<FJsonObject>* RuleParamsPtr;
	TSharedPtr<FJsonObject> RuleParams;
	if (Params->TryGetObjectField(TEXT("rule_params"), RuleParamsPtr))
	{
		RuleParams = *RuleParamsPtr;
	}

	if (AssetPath.IsEmpty() || SourceStateName.IsEmpty() || TargetStateName.IsEmpty() || RuleType.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_path, source_state, target_state, and rule_type are required"));
	}

	UAnimBlueprint* AnimBP = LoadAnimBP(AssetPath);
	if (!AnimBP)
	{
		return ErrorResponse(FString::Printf(TEXT("Animation Blueprint not found: %s"), *AssetPath));
	}

	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(AnimBP, SMName);
	if (!SMGraph)
	{
		return ErrorResponse(TEXT("No state machine found in the Animation Blueprint"));
	}

	UAnimStateNode* SourceState = FindStateByName(SMGraph, SourceStateName);
	if (!SourceState)
	{
		return ErrorResponse(FString::Printf(TEXT("Source state not found: %s"), *SourceStateName));
	}

	UAnimStateNode* TargetState = FindStateByName(SMGraph, TargetStateName);
	if (!TargetState)
	{
		return ErrorResponse(FString::Printf(TEXT("Target state not found: %s"), *TargetStateName));
	}

	UAnimStateTransitionNode* TransNode = FindTransitionBetween(SMGraph, SourceState, TargetState);
	if (!TransNode)
	{
		return ErrorResponse(FString::Printf(TEXT("No transition found from '%s' to '%s'. Use add_anim_transition first."),
			*SourceStateName, *TargetStateName));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Set Transition Rule")));
	AnimBP->Modify();

	FString RuleInfo;

	if (RuleType == TEXT("auto_rule"))
	{
		// Automatic transition based on sequence player remaining time
		TransNode->bAutomaticRuleBasedOnSequencePlayerInState = true;
		RuleInfo = TEXT("auto_rule: transitions when animation finishes");
	}
	else if (RuleType == TEXT("time_remaining"))
	{
		// Transition when remaining time is below threshold
		TransNode->bAutomaticRuleBasedOnSequencePlayerInState = true;

		double Threshold = 0.25;
		if (RuleParams.IsValid())
		{
			RuleParams->TryGetNumberField(TEXT("threshold"), Threshold);
		}
		TransNode->AutomaticRuleTriggerTime = static_cast<float>(Threshold);
		RuleInfo = FString::Printf(TEXT("time_remaining: triggers when < %.2f seconds remain"), Threshold);
	}
	else if (RuleType == TEXT("bool_variable"))
	{
		// Transition based on a bool variable in the AnimInstance
		TransNode->bAutomaticRuleBasedOnSequencePlayerInState = false;

		FString VariableName;
		if (RuleParams.IsValid())
		{
			VariableName = RuleParams->GetStringField(TEXT("variable"));
		}

		if (VariableName.IsEmpty())
		{
			return ErrorResponse(TEXT("bool_variable rule_type requires rule_params.variable"));
		}

		// Ensure the bool variable exists on the AnimBP (auto-create if missing)
		FName VarFName(*VariableName);
		int32 VarIdx = FBlueprintEditorUtils::FindNewVariableIndex(AnimBP, VarFName);
		bool bVariableCreated = false;
		if (VarIdx == INDEX_NONE)
		{
			FEdGraphPinType PinType;
			PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
			FBlueprintEditorUtils::AddMemberVariable(AnimBP, VarFName, PinType);
			FKismetEditorUtilities::CompileBlueprint(AnimBP);
			bVariableCreated = true;
		}

		// Wire a variable getter node in the transition graph
		UAnimationTransitionGraph* TransGraph = Cast<UAnimationTransitionGraph>(TransNode->BoundGraph);
		if (TransGraph && TransGraph->MyResultNode)
		{
			// Find the "bCanEnterTransition" result input pin (boolean)
			UEdGraphPin* ResultPin = nullptr;
			for (UEdGraphPin* Pin : TransGraph->MyResultNode->Pins)
			{
				if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
				{
					ResultPin = Pin;
					break;
				}
			}

			if (ResultPin)
			{
				// Break any existing connections to the result pin
				ResultPin->BreakAllPinLinks();

				// Create a variable getter node
				UK2Node_VariableGet* GetterNode = NewObject<UK2Node_VariableGet>(TransGraph);
				GetterNode->VariableReference.SetSelfMember(VarFName);
				GetterNode->CreateNewGuid();
				TransGraph->AddNode(GetterNode, false, false);
				GetterNode->AllocateDefaultPins();
				GetterNode->NodePosX = TransGraph->MyResultNode->NodePosX - 250;
				GetterNode->NodePosY = TransGraph->MyResultNode->NodePosY;

				// Find the bool output pin on the getter
				UEdGraphPin* GetterOutput = GetterNode->GetValuePin();
				if (GetterOutput)
				{
					const UEdGraphSchema* Schema = TransGraph->GetSchema();
					if (Schema)
					{
						Schema->TryCreateConnection(GetterOutput, ResultPin);
					}
					RuleInfo = FString::Printf(TEXT("bool_variable: checks '%s' on AnimInstance%s (connected)"),
						*VariableName, bVariableCreated ? TEXT(" [variable auto-created]") : TEXT(""));
				}
				else
				{
					RuleInfo = FString::Printf(
						TEXT("bool_variable: created getter for '%s' but could not find output pin"), *VariableName);
				}
			}
			else
			{
				RuleInfo = FString::Printf(
					TEXT("bool_variable: could not find boolean result pin. Variable '%s' not connected."), *VariableName);
			}
		}
		else
		{
			RuleInfo = TEXT("bool_variable: transition has no bound graph or result node");
		}
	}
	else
	{
		return ErrorResponse(FString::Printf(
			TEXT("Unknown rule_type: '%s'. Supported: auto_rule, time_remaining, bool_variable"), *RuleType));
	}

	// Compile
	FKismetEditorUtilities::CompileBlueprint(AnimBP);
	AnimBP->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("source_state"), SourceStateName);
	Data->SetStringField(TEXT("target_state"), TargetStateName);
	Data->SetStringField(TEXT("rule_type"), RuleType);
	Data->SetStringField(TEXT("rule_info"), RuleInfo);
	return SuccessResponse(Data);
}

// ============================================================================
// add_blend_space
// ============================================================================

TSharedPtr<FJsonObject> FMCPAddBlendSpaceCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Name = Params->GetStringField(TEXT("name"));
	FString Path = Params->GetStringField(TEXT("path"));
	FString SkeletonPath = Params->GetStringField(TEXT("skeleton_path"));
	FString Type = Params->GetStringField(TEXT("type"));

	FString AxisXName = Params->GetStringField(TEXT("axis_x_name"));
	FString AxisYName = Params->GetStringField(TEXT("axis_y_name"));

	if (Name.IsEmpty() || SkeletonPath.IsEmpty())
	{
		return ErrorResponse(TEXT("name and skeleton_path are required"));
	}
	if (Path.IsEmpty())
	{
		Path = TEXT("/Game/Animations");
	}
	if (Type.IsEmpty())
	{
		Type = TEXT("1D");
	}

	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return ErrorResponse(FString::Printf(TEXT("Could not load skeleton from '%s'"), *SkeletonPath));
	}

	// Parse axis ranges
	double AxisXMin = 0.0, AxisXMax = 100.0;
	const TArray<TSharedPtr<FJsonValue>>* AxisXRange;
	if (Params->TryGetArrayField(TEXT("axis_x_range"), AxisXRange) && AxisXRange->Num() >= 2)
	{
		AxisXMin = (*AxisXRange)[0]->AsNumber();
		AxisXMax = (*AxisXRange)[1]->AsNumber();
	}

	double AxisYMin = 0.0, AxisYMax = 100.0;
	const TArray<TSharedPtr<FJsonValue>>* AxisYRange;
	if (Params->TryGetArrayField(TEXT("axis_y_range"), AxisYRange) && AxisYRange->Num() >= 2)
	{
		AxisYMin = (*AxisYRange)[0]->AsNumber();
		AxisYMax = (*AxisYRange)[1]->AsNumber();
	}

	FString PackagePath = Path / Name;
	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to create package at '%s'"), *PackagePath));
	}

	UBlendSpace* BlendSpaceAsset = nullptr;

	if (Type == TEXT("1D"))
	{
		UBlendSpaceFactory1D* Factory = NewObject<UBlendSpaceFactory1D>();
		Factory->TargetSkeleton = Skeleton;

		UObject* NewAsset = Factory->FactoryCreateNew(
			UBlendSpace1D::StaticClass(),
			Package,
			FName(*Name),
			RF_Public | RF_Standalone,
			nullptr,
			GWarn);

		UBlendSpace1D* BS1D = Cast<UBlendSpace1D>(NewAsset);
		if (!BS1D)
		{
			return ErrorResponse(TEXT("Failed to create BlendSpace1D"));
		}

		// Configure axis via FProperty (BlendParameters is protected, const getter only)
		{
			FProperty* BPProp = BS1D->GetClass()->FindPropertyByName(TEXT("BlendParameters"));
			if (BPProp)
			{
				FBlendParameter* Param0 = BPProp->ContainerPtrToValuePtr<FBlendParameter>(BS1D, 0);
				Param0->DisplayName = AxisXName.IsEmpty() ? TEXT("Speed") : AxisXName;
				Param0->Min = static_cast<float>(AxisXMin);
				Param0->Max = static_cast<float>(AxisXMax);
			}
		}

		BlendSpaceAsset = BS1D;
	}
	else
	{
		UBlendSpaceFactoryNew* Factory = NewObject<UBlendSpaceFactoryNew>();
		Factory->TargetSkeleton = Skeleton;

		UObject* NewAsset = Factory->FactoryCreateNew(
			UBlendSpace::StaticClass(),
			Package,
			FName(*Name),
			RF_Public | RF_Standalone,
			nullptr,
			GWarn);

		UBlendSpace* BS2D = Cast<UBlendSpace>(NewAsset);
		if (!BS2D)
		{
			return ErrorResponse(TEXT("Failed to create BlendSpace"));
		}

		// Configure axes via FProperty (BlendParameters is protected, const getter only)
		{
			FProperty* BPProp = BS2D->GetClass()->FindPropertyByName(TEXT("BlendParameters"));
			if (BPProp)
			{
				FBlendParameter* Param0 = BPProp->ContainerPtrToValuePtr<FBlendParameter>(BS2D, 0);
				Param0->DisplayName = AxisXName.IsEmpty() ? TEXT("Speed") : AxisXName;
				Param0->Min = static_cast<float>(AxisXMin);
				Param0->Max = static_cast<float>(AxisXMax);

				FBlendParameter* Param1 = BPProp->ContainerPtrToValuePtr<FBlendParameter>(BS2D, 1);
				Param1->DisplayName = AxisYName.IsEmpty() ? TEXT("Direction") : AxisYName;
				Param1->Min = static_cast<float>(AxisYMin);
				Param1->Max = static_cast<float>(AxisYMax);
			}
		}

		BlendSpaceAsset = BS2D;
	}

	// Add samples if provided (SampleData is protected, access via FProperty)
	int32 SamplesAdded = 0;
	const TArray<TSharedPtr<FJsonValue>>* SamplesArr;
	if (Params->TryGetArrayField(TEXT("samples"), SamplesArr))
	{
		FProperty* SampleProp = BlendSpaceAsset->GetClass()->FindPropertyByName(TEXT("SampleData"));
		TArray<FBlendSample>* SampleDataPtr = SampleProp
			? SampleProp->ContainerPtrToValuePtr<TArray<FBlendSample>>(BlendSpaceAsset)
			: nullptr;

		for (const TSharedPtr<FJsonValue>& SampleVal : *SamplesArr)
		{
			const TSharedPtr<FJsonObject>& SampleObj = SampleVal->AsObject();
			if (!SampleObj.IsValid()) continue;

			FString AnimPath = SampleObj->GetStringField(TEXT("animation"));
			double XVal = 0.0, YVal = 0.0;
			SampleObj->TryGetNumberField(TEXT("x"), XVal);
			SampleObj->TryGetNumberField(TEXT("y"), YVal);

			FString AnimFullPath = EnsureObjectPath(AnimPath);
			UObject* AnimObj = StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *AnimFullPath);
			UAnimSequence* AnimSeq = Cast<UAnimSequence>(AnimObj);

			if (AnimSeq && SampleDataPtr)
			{
				FBlendSample NewSample;
				NewSample.Animation = AnimSeq;
				NewSample.SampleValue = FVector(XVal, YVal, 0.0f);
				NewSample.RateScale = 1.0f;
				SampleDataPtr->Add(NewSample);
				SamplesAdded++;
			}
		}

		BlendSpaceAsset->ValidateSampleData();
	}

	SaveAssetPackage(BlendSpaceAsset);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Name);
	Data->SetStringField(TEXT("asset_path"), BlendSpaceAsset->GetPathName());
	Data->SetStringField(TEXT("type"), Type);
	Data->SetStringField(TEXT("skeleton"), Skeleton->GetPathName());
	Data->SetNumberField(TEXT("samples_added"), SamplesAdded);
	return SuccessResponse(Data);
}

// ============================================================================
// add_anim_montage
// ============================================================================

TSharedPtr<FJsonObject> FMCPAddAnimMontageCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Name = Params->GetStringField(TEXT("name"));
	FString Path = Params->GetStringField(TEXT("path"));
	FString SkeletonPath = Params->GetStringField(TEXT("skeleton_path"));
	FString AnimationPath = Params->GetStringField(TEXT("animation_path"));
	FString SlotName = Params->GetStringField(TEXT("slot_name"));

	if (Name.IsEmpty() || AnimationPath.IsEmpty())
	{
		return ErrorResponse(TEXT("name and animation_path are required"));
	}
	if (Path.IsEmpty())
	{
		Path = TEXT("/Game/Animations");
	}
	if (SlotName.IsEmpty())
	{
		SlotName = TEXT("DefaultSlot");
	}

	// Load the source animation
	FString AnimFullPath = EnsureObjectPath(AnimationPath);
	UObject* AnimObj = StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *AnimFullPath);
	UAnimSequence* SourceAnim = Cast<UAnimSequence>(AnimObj);
	if (!SourceAnim)
	{
		return ErrorResponse(FString::Printf(TEXT("Could not load animation sequence: %s"), *AnimationPath));
	}

	// Load skeleton (optional - can get from animation)
	USkeleton* Skeleton = nullptr;
	if (!SkeletonPath.IsEmpty())
	{
		Skeleton = LoadSkeleton(SkeletonPath);
	}
	if (!Skeleton)
	{
		Skeleton = SourceAnim->GetSkeleton();
	}
	if (!Skeleton)
	{
		return ErrorResponse(TEXT("Could not determine skeleton for the montage"));
	}

	FString PackagePath = Path / Name;
	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to create package at '%s'"), *PackagePath));
	}

	// Create montage using factory
	UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
	Factory->SourceAnimation = SourceAnim;

	UObject* NewAsset = Factory->FactoryCreateNew(
		UAnimMontage::StaticClass(),
		Package,
		FName(*Name),
		RF_Public | RF_Standalone,
		nullptr,
		GWarn);

	UAnimMontage* Montage = Cast<UAnimMontage>(NewAsset);
	if (!Montage)
	{
		return ErrorResponse(TEXT("Failed to create Animation Montage"));
	}

	// Set slot name
	if (Montage->SlotAnimTracks.Num() > 0)
	{
		Montage->SlotAnimTracks[0].SlotName = FName(*SlotName);
	}

	// Add custom sections if provided
	int32 SectionsAdded = 0;
	const TArray<TSharedPtr<FJsonValue>>* SectionsArr;
	if (Params->TryGetArrayField(TEXT("sections"), SectionsArr))
	{
		for (const TSharedPtr<FJsonValue>& SectionVal : *SectionsArr)
		{
			const TSharedPtr<FJsonObject>& SectionObj = SectionVal->AsObject();
			if (!SectionObj.IsValid()) continue;

			FString SectionName = SectionObj->GetStringField(TEXT("name"));
			double StartTime = 0.0;
			SectionObj->TryGetNumberField(TEXT("start_time"), StartTime);

			if (!SectionName.IsEmpty())
			{
				Montage->AddAnimCompositeSection(FName(*SectionName), static_cast<float>(StartTime));
				SectionsAdded++;
			}
		}
	}

	SaveAssetPackage(Montage);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Name);
	Data->SetStringField(TEXT("asset_path"), Montage->GetPathName());
	Data->SetStringField(TEXT("skeleton"), Skeleton->GetPathName());
	Data->SetStringField(TEXT("source_animation"), SourceAnim->GetName());
	Data->SetStringField(TEXT("slot_name"), SlotName);
	Data->SetNumberField(TEXT("sections_added"), SectionsAdded);
	return SuccessResponse(Data);
}

// ============================================================================
// get_anim_graph
// ============================================================================

TSharedPtr<FJsonObject> FMCPGetAnimGraphCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	if (AssetPath.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_path is required"));
	}

	UAnimBlueprint* AnimBP = LoadAnimBP(AssetPath);
	if (!AnimBP)
	{
		return ErrorResponse(FString::Printf(TEXT("Animation Blueprint not found: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), AnimBP->GetPathName());
	Data->SetStringField(TEXT("name"), AnimBP->GetName());

	if (AnimBP->TargetSkeleton)
	{
		Data->SetStringField(TEXT("skeleton"), AnimBP->TargetSkeleton->GetPathName());
	}

	// Find all state machines
	TArray<TSharedPtr<FJsonValue>> StateMachines;

	TArray<UEdGraph*> AllGraphs;
	AnimBP->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node);
			if (!SMNode || !SMNode->EditorStateMachineGraph)
			{
				continue;
			}

			UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;

			TSharedPtr<FJsonObject> SMObj = MakeShared<FJsonObject>();
			SMObj->SetStringField(TEXT("name"), SMNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

			// Find entry state
			FString EntryStateName = TEXT("none");
			for (UEdGraphNode* SMNodeInner : SMGraph->Nodes)
			{
				UAnimStateEntryNode* EntryNode = Cast<UAnimStateEntryNode>(SMNodeInner);
				if (EntryNode)
				{
					// UAnimStateEntryNode inherits from UEdGraphNode, not UAnimStateNodeBase
					// so it doesn't have GetOutputPin(). Search Pins array instead.
					UEdGraphPin* EntryOutput = nullptr;
					for (UEdGraphPin* Pin : EntryNode->Pins)
					{
						if (Pin->Direction == EGPD_Output)
						{
							EntryOutput = Pin;
							break;
						}
					}
					if (EntryOutput)
					{
						for (UEdGraphPin* Link : EntryOutput->LinkedTo)
						{
							UAnimStateNode* ConnectedState = Cast<UAnimStateNode>(Link->GetOwningNode());
							if (ConnectedState && ConnectedState->BoundGraph)
							{
								EntryStateName = ConnectedState->BoundGraph->GetName();
							}
						}
					}
					break;
				}
			}
			SMObj->SetStringField(TEXT("entry_state"), EntryStateName);

			// Serialize all states
			TArray<TSharedPtr<FJsonValue>> StatesArr;
			int32 StateCount = 0;
			for (UEdGraphNode* SMNodeInner : SMGraph->Nodes)
			{
				UAnimStateNode* StateNode = Cast<UAnimStateNode>(SMNodeInner);
				if (StateNode)
				{
					TSharedPtr<FJsonObject> StateJson = StateToJson(StateNode);
					if (StateJson.IsValid())
					{
						StatesArr.Add(MakeShared<FJsonValueObject>(StateJson));
						StateCount++;
					}
				}
			}
			SMObj->SetNumberField(TEXT("state_count"), StateCount);
			SMObj->SetArrayField(TEXT("states"), StatesArr);

			StateMachines.Add(MakeShared<FJsonValueObject>(SMObj));
		}
	}

	Data->SetNumberField(TEXT("state_machine_count"), StateMachines.Num());
	Data->SetArrayField(TEXT("state_machines"), StateMachines);

	return SuccessResponse(Data);
}
