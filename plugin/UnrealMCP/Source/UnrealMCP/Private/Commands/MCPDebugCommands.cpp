#include "Commands/MCPDebugCommands.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/Breakpoint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Editor.h"
#include "Engine/BlueprintGeneratedClass.h"

namespace DebugCommandsLocal
{

static UBlueprint* LoadBlueprintForDebug(const FString& AssetPath)
{
	return Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *AssetPath));
}

static UEdGraphNode* FindNodeInBlueprint(UBlueprint* BP, const FString& NodeId, const FString& GraphName = TEXT(""))
{
	TArray<UEdGraph*> AllGraphs;
	BP->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!GraphName.IsEmpty() && Graph->GetName() != GraphName)
			continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node->NodeGuid.ToString() == NodeId)
				return Node;
		}
	}
	return nullptr;
}

} // namespace DebugCommandsLocal

using namespace DebugCommandsLocal;

// --- Set Breakpoint ---
TSharedPtr<FJsonObject> FMCPSetBreakpointCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString NodeId = Params->GetStringField(TEXT("node_id"));
	FString GraphName = Params->GetStringField(TEXT("graph_name"));
	bool bEnabled = true;
	Params->TryGetBoolField(TEXT("enabled"), bEnabled);

	if (NodeId.IsEmpty())
	{
		return ErrorResponse(TEXT("node_id is required"));
	}

	UBlueprint* BP = LoadBlueprintForDebug(AssetPath);
	if (!BP)
	{
		return ErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	UEdGraphNode* TargetNode = FindNodeInBlueprint(BP, NodeId, GraphName);
	if (!TargetNode)
	{
		return ErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeId));
	}

	// Check if breakpoint already exists on this node
	FBlueprintBreakpoint* Existing = FKismetDebugUtilities::FindBreakpointForNode(TargetNode, BP);
	if (Existing)
	{
		// Toggle or update existing breakpoint
		FKismetDebugUtilities::SetBreakpointEnabled(*Existing, bEnabled);
	}
	else if (bEnabled)
	{
		// Create new breakpoint
		FKismetDebugUtilities::CreateBreakpoint(BP, TargetNode, bEnabled);
	}
	else
	{
		// Trying to disable a non-existent breakpoint — nothing to do
		return SuccessResponse(TEXT("No breakpoint exists on this node to disable"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("node_id"), NodeId);
	Data->SetStringField(TEXT("node_title"), TargetNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Data->SetBoolField(TEXT("enabled"), bEnabled);
	Data->SetBoolField(TEXT("is_valid"), Existing ? FKismetDebugUtilities::IsBreakpointValid(*Existing) : true);
	return SuccessResponse(Data);
}

// --- Get Breakpoints ---
TSharedPtr<FJsonObject> FMCPGetBreakpointsCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UBlueprint* BP = LoadBlueprintForDebug(AssetPath);
	if (!BP)
	{
		return ErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	TArray<TSharedPtr<FJsonValue>> BreakpointArray;

	FKismetDebugUtilities::ForeachBreakpoint(BP, [&BreakpointArray](FBlueprintBreakpoint& Breakpoint)
	{
		UEdGraphNode* Node = Breakpoint.GetLocation();
		if (!Node) return;

		TSharedPtr<FJsonObject> BPInfo = MakeShared<FJsonObject>();
		BPInfo->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
		BPInfo->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		BPInfo->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
		BPInfo->SetBoolField(TEXT("enabled"), Breakpoint.IsEnabled());
		BPInfo->SetBoolField(TEXT("is_valid"), FKismetDebugUtilities::IsBreakpointValid(Breakpoint));

		// Find which graph this node belongs to
		if (UEdGraph* Graph = Node->GetGraph())
		{
			BPInfo->SetStringField(TEXT("graph_name"), Graph->GetName());
		}

		BPInfo->SetNumberField(TEXT("pos_x"), Node->NodePosX);
		BPInfo->SetNumberField(TEXT("pos_y"), Node->NodePosY);

		BreakpointArray.Add(MakeShared<FJsonValueObject>(BPInfo));
	});

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("breakpoints"), BreakpointArray);
	Data->SetNumberField(TEXT("count"), BreakpointArray.Num());
	Data->SetBoolField(TEXT("has_breakpoints"), FKismetDebugUtilities::BlueprintHasBreakpoints(BP));
	return SuccessResponse(Data);
}

// --- Get Watch Values ---
TSharedPtr<FJsonObject> FMCPGetWatchValuesCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	// Check if PIE is active and paused
	if (!GEditor || !GEditor->PlayWorld)
	{
		return ErrorResponse(TEXT("No PIE session active. Start PIE and hit a breakpoint first."));
	}

	if (!GEditor->PlayWorld->bDebugPauseExecution)
	{
		return ErrorResponse(TEXT("PIE is running but not paused at a breakpoint. Pause at a breakpoint to inspect values."));
	}

	UBlueprint* BP = nullptr;
	if (!AssetPath.IsEmpty())
	{
		BP = LoadBlueprintForDebug(AssetPath);
		if (!BP)
		{
			return ErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
		}
	}

	TArray<TSharedPtr<FJsonValue>> WatchArray;

	// If a specific blueprint is requested, get its watches
	if (BP)
	{
		// Get the debug object for this blueprint
		UObject* DebugObj = BP->GetObjectBeingDebugged();

		FKismetDebugUtilities::ForeachPinWatch(BP, [&WatchArray, BP, DebugObj](UEdGraphPin* Pin)
		{
			if (!Pin) return;

			TSharedPtr<FJsonObject> WatchInfo = MakeShared<FJsonObject>();
			WatchInfo->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
			WatchInfo->SetStringField(TEXT("pin_type"), Pin->PinType.PinCategory.ToString());

			if (UEdGraphNode* Node = Pin->GetOwningNode())
			{
				WatchInfo->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
				WatchInfo->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
				if (UEdGraph* Graph = Node->GetGraph())
				{
					WatchInfo->SetStringField(TEXT("graph_name"), Graph->GetName());
				}
			}

			// Try to get the actual value
			if (DebugObj)
			{
				FString WatchText;
				FKismetDebugUtilities::EWatchTextResult WatchResult = FKismetDebugUtilities::GetWatchText(WatchText, BP, DebugObj, Pin);
				if (WatchResult == FKismetDebugUtilities::EWTR_Valid)
				{
					WatchInfo->SetStringField(TEXT("value"), WatchText);
					WatchInfo->SetStringField(TEXT("status"), TEXT("valid"));
				}
				else if (WatchResult == FKismetDebugUtilities::EWTR_NotInScope)
				{
					WatchInfo->SetStringField(TEXT("status"), TEXT("not_in_scope"));
				}
				else if (WatchResult == FKismetDebugUtilities::EWTR_NoDebugObject)
				{
					WatchInfo->SetStringField(TEXT("status"), TEXT("no_debug_object"));
				}
				else
				{
					WatchInfo->SetStringField(TEXT("status"), TEXT("no_property"));
				}
			}
			else
			{
				WatchInfo->SetStringField(TEXT("status"), TEXT("no_debug_object"));
			}

			WatchArray.Add(MakeShared<FJsonValueObject>(WatchInfo));
		});
	}

	// Also get current instruction info
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("watches"), WatchArray);
	Data->SetNumberField(TEXT("count"), WatchArray.Num());

	UEdGraphNode* CurrentNode = FKismetDebugUtilities::GetCurrentInstruction();
	if (CurrentNode)
	{
		Data->SetStringField(TEXT("current_node_id"), CurrentNode->NodeGuid.ToString());
		Data->SetStringField(TEXT("current_node_title"), CurrentNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	}

	return SuccessResponse(Data);
}

// --- Step Execution ---
TSharedPtr<FJsonObject> FMCPStepExecutionCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString StepType = Params->GetStringField(TEXT("step_type"));

	// Check if PIE is active
	if (!GEditor || !GEditor->PlayWorld)
	{
		return ErrorResponse(TEXT("No PIE session active."));
	}

	if (StepType == TEXT("into"))
	{
		FKismetDebugUtilities::RequestSingleStepIn();
	}
	else if (StepType == TEXT("over"))
	{
		FKismetDebugUtilities::RequestStepOver();
	}
	else if (StepType == TEXT("out"))
	{
		FKismetDebugUtilities::RequestStepOut();
	}
	else if (StepType == TEXT("resume"))
	{
		// Resume execution by unpausing
		if (GEditor->PlayWorld->bDebugPauseExecution)
		{
			GEditor->PlayWorld->bDebugPauseExecution = false;
		}
	}
	else
	{
		return ErrorResponse(FString::Printf(TEXT("Unknown step_type: '%s'. Use 'into', 'over', 'out', or 'resume'."), *StepType));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("step_type"), StepType);
	Data->SetBoolField(TEXT("is_single_stepping"), FKismetDebugUtilities::IsSingleStepping());
	return SuccessResponse(Data);
}

// --- Get Call Stack ---
TSharedPtr<FJsonObject> FMCPGetCallStackCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Check if PIE is active and paused
	if (!GEditor || !GEditor->PlayWorld)
	{
		return ErrorResponse(TEXT("No PIE session active."));
	}

	if (!GEditor->PlayWorld->bDebugPauseExecution)
	{
		return ErrorResponse(TEXT("PIE is running but not paused at a breakpoint."));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

	// Get current instruction
	UEdGraphNode* CurrentNode = FKismetDebugUtilities::GetCurrentInstruction();
	if (CurrentNode)
	{
		TSharedPtr<FJsonObject> CurrentInfo = MakeShared<FJsonObject>();
		CurrentInfo->SetStringField(TEXT("node_id"), CurrentNode->NodeGuid.ToString());
		CurrentInfo->SetStringField(TEXT("node_title"), CurrentNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		CurrentInfo->SetStringField(TEXT("node_class"), CurrentNode->GetClass()->GetName());
		if (UEdGraph* Graph = CurrentNode->GetGraph())
		{
			CurrentInfo->SetStringField(TEXT("graph_name"), Graph->GetName());
		}
		CurrentInfo->SetNumberField(TEXT("pos_x"), CurrentNode->NodePosX);
		CurrentInfo->SetNumberField(TEXT("pos_y"), CurrentNode->NodePosY);
		Data->SetObjectField(TEXT("current_instruction"), CurrentInfo);
	}

	// Get most recent breakpoint hit
	UEdGraphNode* BreakpointNode = FKismetDebugUtilities::GetMostRecentBreakpointHit();
	if (BreakpointNode)
	{
		TSharedPtr<FJsonObject> BPInfo = MakeShared<FJsonObject>();
		BPInfo->SetStringField(TEXT("node_id"), BreakpointNode->NodeGuid.ToString());
		BPInfo->SetStringField(TEXT("node_title"), BreakpointNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		if (UEdGraph* Graph = BreakpointNode->GetGraph())
		{
			BPInfo->SetStringField(TEXT("graph_name"), Graph->GetName());
		}
		Data->SetObjectField(TEXT("breakpoint_hit"), BPInfo);
	}

	// Get trace stack — recent execution history
	// TSimpleRingBuffer uses operator() — element 0 is most recent
	const auto& TraceStack = FKismetDebugUtilities::GetTraceStack();
	TArray<TSharedPtr<FJsonValue>> StackFrames;

	const int32 MaxFrames = 50;
	const int32 TraceNum = TraceStack.Num();

	for (int32 i = 0; i < TraceNum && i < MaxFrames; ++i)
	{
		const FKismetTraceSample& Sample = TraceStack(i);
		UObject* Context = Sample.Context.Get();
		UFunction* Function = Sample.Function.Get();

		if (!Context || !Function) continue;

		// Find the source node for this trace entry
		UEdGraphNode* SourceNode = FKismetDebugUtilities::FindSourceNodeForCodeLocation(
			Context, Function, Sample.Offset, true);

		if (!SourceNode) continue;

		TSharedPtr<FJsonObject> FrameInfo = MakeShared<FJsonObject>();
		FrameInfo->SetNumberField(TEXT("index"), StackFrames.Num());
		FrameInfo->SetStringField(TEXT("node_id"), SourceNode->NodeGuid.ToString());
		FrameInfo->SetStringField(TEXT("node_title"), SourceNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		FrameInfo->SetStringField(TEXT("function"), Function->GetName());
		FrameInfo->SetStringField(TEXT("context"), Context->GetName());
		FrameInfo->SetNumberField(TEXT("time"), Sample.ObservationTime);

		if (UEdGraph* Graph = SourceNode->GetGraph())
		{
			FrameInfo->SetStringField(TEXT("graph_name"), Graph->GetName());
		}

		StackFrames.Add(MakeShared<FJsonValueObject>(FrameInfo));
	}

	Data->SetArrayField(TEXT("trace_stack"), StackFrames);
	Data->SetNumberField(TEXT("frame_count"), StackFrames.Num());
	Data->SetBoolField(TEXT("is_single_stepping"), FKismetDebugUtilities::IsSingleStepping());

	return SuccessResponse(Data);
}
