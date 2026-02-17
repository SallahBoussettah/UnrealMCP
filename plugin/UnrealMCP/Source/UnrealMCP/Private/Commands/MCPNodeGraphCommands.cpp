#include "Commands/MCPNodeGraphCommands.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "ScopedTransaction.h"

static UBlueprint* LoadBP(const FString& AssetPath)
{
	return Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *AssetPath));
}

static UEdGraph* FindGraph(UBlueprint* BP, const FString& GraphName)
{
	for (UEdGraph* Graph : BP->UbergraphPages)
	{
		if (Graph->GetName() == GraphName)
			return Graph;
	}
	for (UEdGraph* Graph : BP->FunctionGraphs)
	{
		if (Graph->GetName() == GraphName)
			return Graph;
	}
	return nullptr;
}

static UEdGraphNode* FindNodeById(UEdGraph* Graph, const FString& NodeId)
{
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node->NodeGuid.ToString() == NodeId)
			return Node;
	}
	return nullptr;
}

static TSharedPtr<FJsonObject> PinToJson(UEdGraphPin* Pin)
{
	TSharedPtr<FJsonObject> PinInfo = MakeShared<FJsonObject>();
	PinInfo->SetStringField(TEXT("name"), Pin->PinName.ToString());
	PinInfo->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
	PinInfo->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
	PinInfo->SetStringField(TEXT("default_value"), Pin->DefaultValue);

	TArray<TSharedPtr<FJsonValue>> Connections;
	for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
	{
		TSharedPtr<FJsonObject> Link = MakeShared<FJsonObject>();
		Link->SetStringField(TEXT("node_id"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
		Link->SetStringField(TEXT("pin_name"), LinkedPin->PinName.ToString());
		Connections.Add(MakeShared<FJsonValueObject>(Link));
	}
	PinInfo->SetArrayField(TEXT("connections"), Connections);

	return PinInfo;
}

static TSharedPtr<FJsonObject> NodeToJson(UEdGraphNode* Node)
{
	TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
	Info->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
	Info->SetStringField(TEXT("class"), Node->GetClass()->GetName());
	Info->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Info->SetNumberField(TEXT("pos_x"), Node->NodePosX);
	Info->SetNumberField(TEXT("pos_y"), Node->NodePosY);

	TArray<TSharedPtr<FJsonValue>> Pins;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin->bHidden)
		{
			Pins.Add(MakeShared<FJsonValueObject>(PinToJson(Pin)));
		}
	}
	Info->SetArrayField(TEXT("pins"), Pins);

	return Info;
}

// --- Add Node ---
TSharedPtr<FJsonObject> FMCPAddNodeCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString GraphName = Params->GetStringField(TEXT("graph_name"));
	FString NodeType = Params->GetStringField(TEXT("node_type"));

	UBlueprint* BP = LoadBP(AssetPath);
	if (!BP) return ErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));

	UEdGraph* Graph = FindGraph(BP, GraphName);
	if (!Graph) return ErrorResponse(FString::Printf(TEXT("Graph not found: %s"), *GraphName));

	const TArray<TSharedPtr<FJsonValue>>* PosArr;
	FVector2D Position(0, 0);
	if (Params->TryGetArrayField(TEXT("node_position"), PosArr) && PosArr->Num() >= 2)
	{
		Position = FVector2D((*PosArr)[0]->AsNumber(), (*PosArr)[1]->AsNumber());
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Add Node")));

	UEdGraphNode* NewNode = nullptr;

	if (NodeType == TEXT("CallFunction"))
	{
		FString FunctionName = Params->GetStringField(TEXT("function_name"));
		FString TargetClassName = Params->GetStringField(TEXT("target_class"));

		UClass* TargetClass = nullptr;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->GetName() == TargetClassName || It->GetName() == FString::Printf(TEXT("U%s"), *TargetClassName))
			{
				TargetClass = *It;
				break;
			}
		}

		if (!TargetClass) return ErrorResponse(FString::Printf(TEXT("Class not found: %s"), *TargetClassName));

		UFunction* Function = TargetClass->FindFunctionByName(FName(*FunctionName));
		if (!Function) return ErrorResponse(FString::Printf(TEXT("Function '%s' not found on %s"), *FunctionName, *TargetClassName));

		UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
		CallNode->FunctionReference.SetExternalMember(FName(*FunctionName), TargetClass);
		CallNode->NodePosX = Position.X;
		CallNode->NodePosY = Position.Y;
		Graph->AddNode(CallNode, false, false);
		CallNode->AllocateDefaultPins();
		NewNode = CallNode;
	}
	else if (NodeType == TEXT("Event"))
	{
		const TSharedPtr<FJsonObject>* ExtraParams;
		FString EventName = TEXT("ReceiveBeginPlay");
		if (Params->TryGetObjectField(TEXT("params"), ExtraParams))
		{
			(*ExtraParams)->TryGetStringField(TEXT("event_name"), EventName);
		}

		UK2Node_Event* EventNode = NewObject<UK2Node_Event>(Graph);
		EventNode->EventReference.SetExternalMember(FName(*EventName), AActor::StaticClass());
		EventNode->NodePosX = Position.X;
		EventNode->NodePosY = Position.Y;
		Graph->AddNode(EventNode, false, false);
		EventNode->AllocateDefaultPins();
		NewNode = EventNode;
	}
	else if (NodeType == TEXT("CustomEvent"))
	{
		const TSharedPtr<FJsonObject>* ExtraParams;
		FString EventName = TEXT("MyCustomEvent");
		if (Params->TryGetObjectField(TEXT("params"), ExtraParams))
		{
			(*ExtraParams)->TryGetStringField(TEXT("event_name"), EventName);
		}

		UK2Node_CustomEvent* CustomEvent = NewObject<UK2Node_CustomEvent>(Graph);
		CustomEvent->CustomFunctionName = FName(*EventName);
		CustomEvent->NodePosX = Position.X;
		CustomEvent->NodePosY = Position.Y;
		Graph->AddNode(CustomEvent, false, false);
		CustomEvent->AllocateDefaultPins();
		NewNode = CustomEvent;
	}
	else if (NodeType == TEXT("Branch"))
	{
		UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(Graph);
		BranchNode->NodePosX = Position.X;
		BranchNode->NodePosY = Position.Y;
		Graph->AddNode(BranchNode, false, false);
		BranchNode->AllocateDefaultPins();
		NewNode = BranchNode;
	}
	else if (NodeType == TEXT("Sequence"))
	{
		UK2Node_ExecutionSequence* SeqNode = NewObject<UK2Node_ExecutionSequence>(Graph);
		SeqNode->NodePosX = Position.X;
		SeqNode->NodePosY = Position.Y;
		Graph->AddNode(SeqNode, false, false);
		SeqNode->AllocateDefaultPins();
		NewNode = SeqNode;
	}
	else if (NodeType == TEXT("VariableGet"))
	{
		const TSharedPtr<FJsonObject>* ExtraParams;
		FString VarName;
		if (Params->TryGetObjectField(TEXT("params"), ExtraParams))
		{
			(*ExtraParams)->TryGetStringField(TEXT("variable_name"), VarName);
		}
		if (VarName.IsEmpty()) return ErrorResponse(TEXT("variable_name is required for VariableGet"));

		UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(Graph);
		GetNode->VariableReference.SetSelfMember(FName(*VarName));
		GetNode->NodePosX = Position.X;
		GetNode->NodePosY = Position.Y;
		Graph->AddNode(GetNode, false, false);
		GetNode->AllocateDefaultPins();
		NewNode = GetNode;
	}
	else if (NodeType == TEXT("VariableSet"))
	{
		const TSharedPtr<FJsonObject>* ExtraParams;
		FString VarName;
		if (Params->TryGetObjectField(TEXT("params"), ExtraParams))
		{
			(*ExtraParams)->TryGetStringField(TEXT("variable_name"), VarName);
		}
		if (VarName.IsEmpty()) return ErrorResponse(TEXT("variable_name is required for VariableSet"));

		UK2Node_VariableSet* SetNode = NewObject<UK2Node_VariableSet>(Graph);
		SetNode->VariableReference.SetSelfMember(FName(*VarName));
		SetNode->NodePosX = Position.X;
		SetNode->NodePosY = Position.Y;
		Graph->AddNode(SetNode, false, false);
		SetNode->AllocateDefaultPins();
		NewNode = SetNode;
	}
	else if (NodeType == TEXT("Self"))
	{
		UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(Graph);
		SelfNode->NodePosX = Position.X;
		SelfNode->NodePosY = Position.Y;
		Graph->AddNode(SelfNode, false, false);
		SelfNode->AllocateDefaultPins();
		NewNode = SelfNode;
	}
	else
	{
		return ErrorResponse(FString::Printf(TEXT("Unsupported node type: %s"), *NodeType));
	}

	if (!NewNode) return ErrorResponse(TEXT("Failed to create node"));

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return SuccessResponse(NodeToJson(NewNode));
}

// --- Connect Pins ---
TSharedPtr<FJsonObject> FMCPConnectPinsCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString GraphName = Params->GetStringField(TEXT("graph_name"));
	FString SourceNodeId = Params->GetStringField(TEXT("source_node_id"));
	FString SourcePinName = Params->GetStringField(TEXT("source_pin_name"));
	FString TargetNodeId = Params->GetStringField(TEXT("target_node_id"));
	FString TargetPinName = Params->GetStringField(TEXT("target_pin_name"));

	UBlueprint* BP = LoadBP(AssetPath);
	if (!BP) return ErrorResponse(TEXT("Blueprint not found"));

	UEdGraph* Graph = FindGraph(BP, GraphName);
	if (!Graph) return ErrorResponse(TEXT("Graph not found"));

	UEdGraphNode* SourceNode = FindNodeById(Graph, SourceNodeId);
	if (!SourceNode) return ErrorResponse(FString::Printf(TEXT("Source node not found: %s"), *SourceNodeId));

	UEdGraphNode* TargetNode = FindNodeById(Graph, TargetNodeId);
	if (!TargetNode) return ErrorResponse(FString::Printf(TEXT("Target node not found: %s"), *TargetNodeId));

	UEdGraphPin* SourcePin = SourceNode->FindPin(FName(*SourcePinName));
	if (!SourcePin) return ErrorResponse(FString::Printf(TEXT("Source pin not found: %s"), *SourcePinName));

	UEdGraphPin* TargetPin = TargetNode->FindPin(FName(*TargetPinName));
	if (!TargetPin) return ErrorResponse(FString::Printf(TEXT("Target pin not found: %s"), *TargetPinName));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Connect Pins")));

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	bool bSuccess = Schema->TryCreateConnection(SourcePin, TargetPin);

	if (!bSuccess) return ErrorResponse(TEXT("Failed to create connection - pins may be incompatible"));

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return SuccessResponse(TEXT("Pins connected successfully"));
}

// --- Disconnect Pins ---
TSharedPtr<FJsonObject> FMCPDisconnectPinsCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString GraphName = Params->GetStringField(TEXT("graph_name"));
	FString NodeId = Params->GetStringField(TEXT("node_id"));
	FString PinName = Params->GetStringField(TEXT("pin_name"));

	UBlueprint* BP = LoadBP(AssetPath);
	if (!BP) return ErrorResponse(TEXT("Blueprint not found"));

	UEdGraph* Graph = FindGraph(BP, GraphName);
	if (!Graph) return ErrorResponse(TEXT("Graph not found"));

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node) return ErrorResponse(TEXT("Node not found"));

	UEdGraphPin* Pin = Node->FindPin(FName(*PinName));
	if (!Pin) return ErrorResponse(TEXT("Pin not found"));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Disconnect Pins")));
	Pin->BreakAllPinLinks();

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return SuccessResponse(TEXT("Pin disconnected"));
}

// --- Delete Node ---
TSharedPtr<FJsonObject> FMCPDeleteNodeCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString GraphName = Params->GetStringField(TEXT("graph_name"));
	FString NodeId = Params->GetStringField(TEXT("node_id"));

	UBlueprint* BP = LoadBP(AssetPath);
	if (!BP) return ErrorResponse(TEXT("Blueprint not found"));

	UEdGraph* Graph = FindGraph(BP, GraphName);
	if (!Graph) return ErrorResponse(TEXT("Graph not found"));

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node) return ErrorResponse(TEXT("Node not found"));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Delete Node")));
	FBlueprintEditorUtils::RemoveNode(BP, Node);

	return SuccessResponse(TEXT("Node deleted"));
}

// --- Get Graph Nodes ---
TSharedPtr<FJsonObject> FMCPGetGraphNodesCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString GraphName = Params->GetStringField(TEXT("graph_name"));

	UBlueprint* BP = LoadBP(AssetPath);
	if (!BP) return ErrorResponse(TEXT("Blueprint not found"));

	UEdGraph* Graph = FindGraph(BP, GraphName);
	if (!Graph) return ErrorResponse(TEXT("Graph not found"));

	TArray<TSharedPtr<FJsonValue>> Nodes;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		Nodes.Add(MakeShared<FJsonValueObject>(NodeToJson(Node)));
	}

	return SuccessResponse(Nodes);
}

// --- Set Pin Value ---
TSharedPtr<FJsonObject> FMCPSetPinValueCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString GraphName = Params->GetStringField(TEXT("graph_name"));
	FString NodeId = Params->GetStringField(TEXT("node_id"));
	FString PinName = Params->GetStringField(TEXT("pin_name"));
	FString Value = Params->GetStringField(TEXT("value"));

	UBlueprint* BP = LoadBP(AssetPath);
	if (!BP) return ErrorResponse(TEXT("Blueprint not found"));

	UEdGraph* Graph = FindGraph(BP, GraphName);
	if (!Graph) return ErrorResponse(TEXT("Graph not found"));

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node) return ErrorResponse(TEXT("Node not found"));

	UEdGraphPin* Pin = Node->FindPin(FName(*PinName));
	if (!Pin) return ErrorResponse(TEXT("Pin not found"));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Set Pin Value")));

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	Schema->TrySetDefaultValue(*Pin, Value);

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return SuccessResponse(FString::Printf(TEXT("Set pin '%s' = '%s'"), *PinName, *Value));
}

// --- Create Function ---
TSharedPtr<FJsonObject> FMCPCreateFunctionCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString FunctionName = Params->GetStringField(TEXT("function_name"));

	UBlueprint* BP = LoadBP(AssetPath);
	if (!BP) return ErrorResponse(TEXT("Blueprint not found"));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Create Function")));

	UEdGraph* FuncGraph = FBlueprintEditorUtils::CreateNewGraph(
		BP,
		FName(*FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	FBlueprintEditorUtils::AddFunctionGraph(BP, FuncGraph, true, nullptr);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("function_name"), FunctionName);
	Data->SetStringField(TEXT("graph_name"), FuncGraph->GetName());

	// Find entry and result nodes
	for (UEdGraphNode* Node : FuncGraph->Nodes)
	{
		if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
		{
			Data->SetStringField(TEXT("entry_node_id"), Entry->NodeGuid.ToString());
		}
		else if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Node))
		{
			Data->SetStringField(TEXT("result_node_id"), Result->NodeGuid.ToString());
		}
	}

	return SuccessResponse(Data);
}

// --- Delete Function ---
TSharedPtr<FJsonObject> FMCPDeleteFunctionCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString FunctionName = Params->GetStringField(TEXT("function_name"));

	UBlueprint* BP = LoadBP(AssetPath);
	if (!BP) return ErrorResponse(TEXT("Blueprint not found"));

	UEdGraph* FuncGraph = FindGraph(BP, FunctionName);
	if (!FuncGraph) return ErrorResponse(FString::Printf(TEXT("Function graph not found: %s"), *FunctionName));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Delete Function")));
	FBlueprintEditorUtils::RemoveGraph(BP, FuncGraph);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

	return SuccessResponse(FString::Printf(TEXT("Deleted function: %s"), *FunctionName));
}
