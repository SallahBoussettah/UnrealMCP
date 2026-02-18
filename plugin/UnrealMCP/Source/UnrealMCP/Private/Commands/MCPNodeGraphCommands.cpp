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

// New node type includes - Flow Control
#include "K2Node_MacroInstance.h"
#include "K2Node_MultiGate.h"
#include "K2Node_Select.h"
#include "K2Node_DoOnceMultiInput.h"
#include "K2Node_ForEachElementInEnum.h"

// New node type includes - Switch
#include "K2Node_SwitchInteger.h"
#include "K2Node_SwitchString.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_SwitchName.h"

// New node type includes - Casting
#include "K2Node_ClassDynamicCast.h"

// New node type includes - Structs
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_SetFieldsInStruct.h"

// New node type includes - Containers
#include "K2Node_MakeArray.h"
#include "K2Node_MakeMap.h"
#include "K2Node_MakeSet.h"
#include "K2Node_GetArrayItem.h"

// New node type includes - Spawning & Objects
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_GenericCreateObject.h"
#include "K2Node_AddComponentByClass.h"

// New node type includes - Delegates
#include "K2Node_CreateDelegate.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_ClearDelegate.h"

// New node type includes - Text & Enums
#include "K2Node_FormatText.h"
#include "K2Node_EnumLiteral.h"

// New node type includes - Misc
#include "K2Node_Timeline.h"
#include "K2Node_Knot.h"
#include "K2Node_LoadAsset.h"
#include "K2Node_EaseFunction.h"
#include "K2Node_GetClassDefaults.h"
#include "K2Node_GetDataTableRow.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"

// Enhanced Input
#include "K2Node_EnhancedInputAction.h"
#include "InputAction.h"

// For Timeline template
#include "Engine/TimelineTemplate.h"

namespace NodeGraphCommandsLocal
{
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

// Helper: Get a string from the "params" sub-object
static FString GetParamString(const TSharedPtr<FJsonObject>& Params, const FString& Key, const FString& Default = TEXT(""))
{
	const TSharedPtr<FJsonObject>* ExtraParams;
	if (Params->TryGetObjectField(TEXT("params"), ExtraParams))
	{
		FString Value;
		if ((*ExtraParams)->TryGetStringField(Key, Value))
			return Value;
	}
	return Default;
}

// Helper: Get an int from the "params" sub-object
static int32 GetParamInt(const TSharedPtr<FJsonObject>& Params, const FString& Key, int32 Default = 0)
{
	const TSharedPtr<FJsonObject>* ExtraParams;
	if (Params->TryGetObjectField(TEXT("params"), ExtraParams))
	{
		double Value;
		if ((*ExtraParams)->TryGetNumberField(Key, Value))
			return static_cast<int32>(Value);
	}
	return Default;
}

// Helper: Find a UClass by short name (tries with/without A/U prefix)
static UClass* FindClassByName(const FString& ClassName)
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->GetName() == ClassName
			|| It->GetName() == FString::Printf(TEXT("U%s"), *ClassName)
			|| It->GetName() == FString::Printf(TEXT("A%s"), *ClassName))
		{
			return *It;
		}
	}
	return nullptr;
}

// Helper: Find a UScriptStruct by short name or full path
static UScriptStruct* FindStructByName(const FString& Name)
{
	// Try full path first
	UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *Name);
	if (Struct) return Struct;

	// Search by short name
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		if (It->GetName() == Name)
			return *It;
	}
	return nullptr;
}

// Helper: Find a UEnum by short name or full path
static UEnum* FindEnumByName(const FString& Name)
{
	UEnum* Enum = FindObject<UEnum>(nullptr, *Name);
	if (Enum) return Enum;

	for (TObjectIterator<UEnum> It; It; ++It)
	{
		if (It->GetName() == Name)
			return *It;
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
} // namespace NodeGraphCommandsLocal

using namespace NodeGraphCommandsLocal;

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

	// ========================================================================
	// EXISTING NODE TYPES (8)
	// ========================================================================

	if (NodeType == TEXT("CallFunction"))
	{
		FString FunctionName = Params->GetStringField(TEXT("function_name"));
		FString TargetClassName = Params->GetStringField(TEXT("target_class"));

		UClass* TargetClass = FindClassByName(TargetClassName);
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
		FString EventName = GetParamString(Params, TEXT("event_name"), TEXT("ReceiveBeginPlay"));

		// Also accept short names like "Tick" → "ReceiveTick", "BeginPlay" → "ReceiveBeginPlay"
		if (!EventName.StartsWith(TEXT("Receive")))
		{
			EventName = TEXT("Receive") + EventName;
		}

		// Find the declaring class for this event function
		UClass* EventOwnerClass = AActor::StaticClass();
		UFunction* EventFunc = BP->GeneratedClass ? BP->GeneratedClass->FindFunctionByName(FName(*EventName)) : nullptr;
		if (EventFunc)
		{
			EventOwnerClass = EventFunc->GetOwnerClass();
		}

		UK2Node_Event* EventNode = NewObject<UK2Node_Event>(Graph);
		EventNode->EventReference.SetExternalMember(FName(*EventName), EventOwnerClass);
		EventNode->bOverrideFunction = true;
		EventNode->NodePosX = Position.X;
		EventNode->NodePosY = Position.Y;
		Graph->AddNode(EventNode, false, false);
		EventNode->PostPlacedNewNode();
		EventNode->AllocateDefaultPins();
		NewNode = EventNode;
	}
	else if (NodeType == TEXT("CustomEvent"))
	{
		FString EventName = GetParamString(Params, TEXT("event_name"), TEXT("MyCustomEvent"));

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
		FString VarName = GetParamString(Params, TEXT("variable_name"));
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
		FString VarName = GetParamString(Params, TEXT("variable_name"));
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

	// ========================================================================
	// FLOW CONTROL
	// ========================================================================

	else if (NodeType == TEXT("MacroInstance"))
	{
		FString MacroName = GetParamString(Params, TEXT("macro_name"));
		if (MacroName.IsEmpty()) return ErrorResponse(TEXT("macro_name is required for MacroInstance (e.g. ForLoop, DoOnce, WhileLoop, ForEachLoop, Gate, FlipFlop, DoN, IsValid)"));

		FString CustomMacroPath = GetParamString(Params, TEXT("macro_path"));

		UEdGraph* MacroGraph = nullptr;

		// Lambda to search a Blueprint's macro graphs for the named macro
		auto FindMacroInBP = [&MacroName, &MacroGraph](UBlueprint* MacroLib) -> bool
		{
			if (!MacroLib) return false;
			for (UEdGraph* MG : MacroLib->MacroGraphs)
			{
				if (MG->GetName() == MacroName)
				{
					MacroGraph = MG;
					return true;
				}
			}
			return false;
		};

		// 1. If user specified a custom macro path, try that first
		if (!CustomMacroPath.IsEmpty())
		{
			UBlueprint* CustomLib = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *CustomMacroPath));
			FindMacroInBP(CustomLib);
		}

		// 2. Try standard macro library paths (UE 5.6 moved them from EditorResources)
		if (!MacroGraph)
		{
			static const TCHAR* StandardPaths[] = {
				TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"),
				TEXT("/Engine/EditorBlueprintResources/StandardMacros"),
				TEXT("/Engine/EditorKismetResources/StandardMacros.StandardMacros"),
				TEXT("/Engine/EditorKismetResources/StandardMacros"),
				TEXT("/Engine/EditorResources/StandardMacros.StandardMacros"),
				TEXT("/Engine/EditorResources/StandardMacros"),
			};
			for (const TCHAR* Path : StandardPaths)
			{
				UBlueprint* StdLib = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, Path));
				if (FindMacroInBP(StdLib)) break;
			}
		}

		// 3. Fallback: search ALL loaded UBlueprint objects (no type filter)
		if (!MacroGraph)
		{
			for (TObjectIterator<UBlueprint> It; It; ++It)
			{
				if (FindMacroInBP(*It)) break;
			}
		}

		// 4. Last resort: search all UEdGraph objects directly
		if (!MacroGraph)
		{
			for (TObjectIterator<UEdGraph> It; It; ++It)
			{
				if (It->GetName() == MacroName)
				{
					// Verify this graph's schema is K2 (Blueprint graph)
					if (It->GetSchema() && It->GetSchema()->IsA(UEdGraphSchema_K2::StaticClass()))
					{
						MacroGraph = *It;
						break;
					}
				}
			}
		}

		if (!MacroGraph)
		{
			// Build diagnostic: count loaded BPs and their macro graphs
			int32 BPCount = 0;
			int32 MacroLibCount = 0;
			TArray<FString> MacroLibNames;
			for (TObjectIterator<UBlueprint> It; It; ++It)
			{
				BPCount++;
				if (It->MacroGraphs.Num() > 0)
				{
					MacroLibCount++;
					MacroLibNames.Add(FString::Printf(TEXT("%s(%d macros)"), *It->GetName(), It->MacroGraphs.Num()));
				}
			}
			FString DiagMsg = FString::Printf(
				TEXT("Macro '%s' not found. Scanned %d Blueprints, %d had macro graphs: [%s]. Try opening a Blueprint editor first to load standard macros."),
				*MacroName, BPCount, MacroLibCount, *FString::Join(MacroLibNames, TEXT(", ")));
			return ErrorResponse(DiagMsg);
		}

		UK2Node_MacroInstance* MacroNode = NewObject<UK2Node_MacroInstance>(Graph);
		MacroNode->SetMacroGraph(MacroGraph);
		MacroNode->NodePosX = Position.X;
		MacroNode->NodePosY = Position.Y;
		Graph->AddNode(MacroNode, false, false);
		MacroNode->AllocateDefaultPins();
		NewNode = MacroNode;
	}
	else if (NodeType == TEXT("MultiGate"))
	{
		UK2Node_MultiGate* MultiGateNode = NewObject<UK2Node_MultiGate>(Graph);
		MultiGateNode->NodePosX = Position.X;
		MultiGateNode->NodePosY = Position.Y;
		Graph->AddNode(MultiGateNode, false, false);
		MultiGateNode->AllocateDefaultPins();
		NewNode = MultiGateNode;
	}
	else if (NodeType == TEXT("Select"))
	{
		UK2Node_Select* SelectNode = NewObject<UK2Node_Select>(Graph);
		SelectNode->NodePosX = Position.X;
		SelectNode->NodePosY = Position.Y;
		Graph->AddNode(SelectNode, false, false);
		SelectNode->AllocateDefaultPins();
		NewNode = SelectNode;
	}
	else if (NodeType == TEXT("DoOnceMultiInput"))
	{
		UK2Node_DoOnceMultiInput* DoOnceNode = NewObject<UK2Node_DoOnceMultiInput>(Graph);
		DoOnceNode->NodePosX = Position.X;
		DoOnceNode->NodePosY = Position.Y;
		Graph->AddNode(DoOnceNode, false, false);
		DoOnceNode->AllocateDefaultPins();
		NewNode = DoOnceNode;
	}
	else if (NodeType == TEXT("ForEachElementInEnum"))
	{
		FString EnumName = GetParamString(Params, TEXT("enum_name"));
		if (EnumName.IsEmpty()) return ErrorResponse(TEXT("enum_name is required for ForEachElementInEnum"));

		UEnum* Enum = FindEnumByName(EnumName);
		if (!Enum) return ErrorResponse(FString::Printf(TEXT("Enum not found: %s"), *EnumName));

		UK2Node_ForEachElementInEnum* EnumLoopNode = NewObject<UK2Node_ForEachElementInEnum>(Graph);
		EnumLoopNode->Enum = Enum;
		EnumLoopNode->NodePosX = Position.X;
		EnumLoopNode->NodePosY = Position.Y;
		Graph->AddNode(EnumLoopNode, false, false);
		EnumLoopNode->AllocateDefaultPins();
		NewNode = EnumLoopNode;
	}

	// ========================================================================
	// SWITCH NODES
	// ========================================================================

	else if (NodeType == TEXT("SwitchInteger"))
	{
		UK2Node_SwitchInteger* SwitchNode = NewObject<UK2Node_SwitchInteger>(Graph);
		SwitchNode->NodePosX = Position.X;
		SwitchNode->NodePosY = Position.Y;
		Graph->AddNode(SwitchNode, false, false);
		SwitchNode->AllocateDefaultPins();
		NewNode = SwitchNode;
	}
	else if (NodeType == TEXT("SwitchString"))
	{
		UK2Node_SwitchString* SwitchNode = NewObject<UK2Node_SwitchString>(Graph);
		SwitchNode->NodePosX = Position.X;
		SwitchNode->NodePosY = Position.Y;
		Graph->AddNode(SwitchNode, false, false);
		SwitchNode->AllocateDefaultPins();
		NewNode = SwitchNode;
	}
	else if (NodeType == TEXT("SwitchEnum"))
	{
		FString EnumName = GetParamString(Params, TEXT("enum_name"));
		if (EnumName.IsEmpty()) return ErrorResponse(TEXT("enum_name is required for SwitchEnum"));

		UEnum* Enum = FindEnumByName(EnumName);
		if (!Enum) return ErrorResponse(FString::Printf(TEXT("Enum not found: %s"), *EnumName));

		UK2Node_SwitchEnum* SwitchNode = NewObject<UK2Node_SwitchEnum>(Graph);
		SwitchNode->SetEnum(Enum);
		SwitchNode->NodePosX = Position.X;
		SwitchNode->NodePosY = Position.Y;
		Graph->AddNode(SwitchNode, false, false);
		SwitchNode->AllocateDefaultPins();
		NewNode = SwitchNode;
	}
	else if (NodeType == TEXT("SwitchName"))
	{
		UK2Node_SwitchName* SwitchNode = NewObject<UK2Node_SwitchName>(Graph);
		SwitchNode->NodePosX = Position.X;
		SwitchNode->NodePosY = Position.Y;
		Graph->AddNode(SwitchNode, false, false);
		SwitchNode->AllocateDefaultPins();
		NewNode = SwitchNode;
	}

	// ========================================================================
	// CASTING
	// ========================================================================

	else if (NodeType == TEXT("DynamicCast"))
	{
		FString TargetClassName = GetParamString(Params, TEXT("target_class"));
		if (TargetClassName.IsEmpty()) return ErrorResponse(TEXT("target_class is required for DynamicCast"));

		UClass* TargetClass = FindClassByName(TargetClassName);
		if (!TargetClass) return ErrorResponse(FString::Printf(TEXT("Target class not found: %s"), *TargetClassName));

		UK2Node_DynamicCast* CastNode = NewObject<UK2Node_DynamicCast>(Graph);
		CastNode->TargetType = TargetClass;
		CastNode->NodePosX = Position.X;
		CastNode->NodePosY = Position.Y;
		Graph->AddNode(CastNode, false, false);
		CastNode->AllocateDefaultPins();
		NewNode = CastNode;
	}
	else if (NodeType == TEXT("ClassDynamicCast"))
	{
		FString TargetClassName = GetParamString(Params, TEXT("target_class"));
		if (TargetClassName.IsEmpty()) return ErrorResponse(TEXT("target_class is required for ClassDynamicCast"));

		UClass* TargetClass = FindClassByName(TargetClassName);
		if (!TargetClass) return ErrorResponse(FString::Printf(TEXT("Target class not found: %s"), *TargetClassName));

		UK2Node_ClassDynamicCast* CastNode = NewObject<UK2Node_ClassDynamicCast>(Graph);
		CastNode->TargetType = TargetClass;
		CastNode->NodePosX = Position.X;
		CastNode->NodePosY = Position.Y;
		Graph->AddNode(CastNode, false, false);
		CastNode->AllocateDefaultPins();
		NewNode = CastNode;
	}

	// ========================================================================
	// STRUCTS
	// ========================================================================

	else if (NodeType == TEXT("MakeStruct"))
	{
		FString StructName = GetParamString(Params, TEXT("struct_type"));
		if (StructName.IsEmpty()) return ErrorResponse(TEXT("struct_type is required for MakeStruct (e.g. Vector, Rotator, Transform, LinearColor)"));

		UScriptStruct* Struct = FindStructByName(StructName);
		if (!Struct) return ErrorResponse(FString::Printf(TEXT("Struct not found: %s"), *StructName));

		UK2Node_MakeStruct* MakeNode = NewObject<UK2Node_MakeStruct>(Graph);
		MakeNode->StructType = Struct;
		MakeNode->NodePosX = Position.X;
		MakeNode->NodePosY = Position.Y;
		Graph->AddNode(MakeNode, false, false);
		MakeNode->AllocateDefaultPins();
		NewNode = MakeNode;
	}
	else if (NodeType == TEXT("BreakStruct"))
	{
		FString StructName = GetParamString(Params, TEXT("struct_type"));
		if (StructName.IsEmpty()) return ErrorResponse(TEXT("struct_type is required for BreakStruct (e.g. Vector, Rotator, Transform, LinearColor)"));

		UScriptStruct* Struct = FindStructByName(StructName);
		if (!Struct) return ErrorResponse(FString::Printf(TEXT("Struct not found: %s"), *StructName));

		UK2Node_BreakStruct* BreakNode = NewObject<UK2Node_BreakStruct>(Graph);
		BreakNode->StructType = Struct;
		BreakNode->NodePosX = Position.X;
		BreakNode->NodePosY = Position.Y;
		Graph->AddNode(BreakNode, false, false);
		BreakNode->AllocateDefaultPins();
		NewNode = BreakNode;
	}
	else if (NodeType == TEXT("SetFieldsInStruct"))
	{
		FString StructName = GetParamString(Params, TEXT("struct_type"));
		if (StructName.IsEmpty()) return ErrorResponse(TEXT("struct_type is required for SetFieldsInStruct"));

		UScriptStruct* Struct = FindStructByName(StructName);
		if (!Struct) return ErrorResponse(FString::Printf(TEXT("Struct not found: %s"), *StructName));

		UK2Node_SetFieldsInStruct* SetFieldsNode = NewObject<UK2Node_SetFieldsInStruct>(Graph);
		SetFieldsNode->StructType = Struct;
		SetFieldsNode->NodePosX = Position.X;
		SetFieldsNode->NodePosY = Position.Y;
		Graph->AddNode(SetFieldsNode, false, false);
		SetFieldsNode->AllocateDefaultPins();
		NewNode = SetFieldsNode;
	}

	// ========================================================================
	// CONTAINERS
	// ========================================================================

	else if (NodeType == TEXT("MakeArray"))
	{
		UK2Node_MakeArray* ArrayNode = NewObject<UK2Node_MakeArray>(Graph);
		ArrayNode->NodePosX = Position.X;
		ArrayNode->NodePosY = Position.Y;
		Graph->AddNode(ArrayNode, false, false);
		ArrayNode->AllocateDefaultPins();

		// Add extra input pins if requested
		int32 NumInputs = GetParamInt(Params, TEXT("num_inputs"), 1);
		for (int32 i = 1; i < NumInputs; ++i)
		{
			ArrayNode->AddInputPin();
		}

		NewNode = ArrayNode;
	}
	else if (NodeType == TEXT("MakeMap"))
	{
		UK2Node_MakeMap* MapNode = NewObject<UK2Node_MakeMap>(Graph);
		MapNode->NodePosX = Position.X;
		MapNode->NodePosY = Position.Y;
		Graph->AddNode(MapNode, false, false);
		MapNode->AllocateDefaultPins();
		NewNode = MapNode;
	}
	else if (NodeType == TEXT("MakeSet"))
	{
		UK2Node_MakeSet* SetNode = NewObject<UK2Node_MakeSet>(Graph);
		SetNode->NodePosX = Position.X;
		SetNode->NodePosY = Position.Y;
		Graph->AddNode(SetNode, false, false);
		SetNode->AllocateDefaultPins();
		NewNode = SetNode;
	}
	else if (NodeType == TEXT("GetArrayItem"))
	{
		UK2Node_GetArrayItem* GetItemNode = NewObject<UK2Node_GetArrayItem>(Graph);
		GetItemNode->NodePosX = Position.X;
		GetItemNode->NodePosY = Position.Y;
		Graph->AddNode(GetItemNode, false, false);
		GetItemNode->AllocateDefaultPins();
		NewNode = GetItemNode;
	}

	// ========================================================================
	// SPAWNING & OBJECTS
	// ========================================================================

	else if (NodeType == TEXT("SpawnActorFromClass"))
	{
		UK2Node_SpawnActorFromClass* SpawnNode = NewObject<UK2Node_SpawnActorFromClass>(Graph);
		SpawnNode->NodePosX = Position.X;
		SpawnNode->NodePosY = Position.Y;
		Graph->AddNode(SpawnNode, false, false);
		SpawnNode->AllocateDefaultPins();
		NewNode = SpawnNode;
	}
	else if (NodeType == TEXT("GenericCreateObject"))
	{
		UK2Node_GenericCreateObject* CreateNode = NewObject<UK2Node_GenericCreateObject>(Graph);
		CreateNode->NodePosX = Position.X;
		CreateNode->NodePosY = Position.Y;
		Graph->AddNode(CreateNode, false, false);
		CreateNode->AllocateDefaultPins();
		NewNode = CreateNode;
	}
	else if (NodeType == TEXT("AddComponentByClass"))
	{
		UK2Node_AddComponentByClass* AddCompNode = NewObject<UK2Node_AddComponentByClass>(Graph);
		AddCompNode->NodePosX = Position.X;
		AddCompNode->NodePosY = Position.Y;
		Graph->AddNode(AddCompNode, false, false);
		AddCompNode->AllocateDefaultPins();
		NewNode = AddCompNode;
	}

	// ========================================================================
	// DELEGATES
	// ========================================================================

	else if (NodeType == TEXT("CreateDelegate"))
	{
		UK2Node_CreateDelegate* CreateDelegateNode = NewObject<UK2Node_CreateDelegate>(Graph);
		CreateDelegateNode->NodePosX = Position.X;
		CreateDelegateNode->NodePosY = Position.Y;
		Graph->AddNode(CreateDelegateNode, false, false);
		CreateDelegateNode->AllocateDefaultPins();
		NewNode = CreateDelegateNode;
	}
	else if (NodeType == TEXT("AddDelegate"))
	{
		FString DelegateName = GetParamString(Params, TEXT("delegate_name"));
		if (DelegateName.IsEmpty()) return ErrorResponse(TEXT("delegate_name is required for AddDelegate (name of the event dispatcher)"));

		UK2Node_AddDelegate* AddDelegateNode = NewObject<UK2Node_AddDelegate>(Graph);
		AddDelegateNode->DelegateReference.SetSelfMember(FName(*DelegateName));
		AddDelegateNode->NodePosX = Position.X;
		AddDelegateNode->NodePosY = Position.Y;
		Graph->AddNode(AddDelegateNode, false, false);
		AddDelegateNode->AllocateDefaultPins();
		NewNode = AddDelegateNode;
	}
	else if (NodeType == TEXT("RemoveDelegate"))
	{
		FString DelegateName = GetParamString(Params, TEXT("delegate_name"));
		if (DelegateName.IsEmpty()) return ErrorResponse(TEXT("delegate_name is required for RemoveDelegate"));

		UK2Node_RemoveDelegate* RemoveDelegateNode = NewObject<UK2Node_RemoveDelegate>(Graph);
		RemoveDelegateNode->DelegateReference.SetSelfMember(FName(*DelegateName));
		RemoveDelegateNode->NodePosX = Position.X;
		RemoveDelegateNode->NodePosY = Position.Y;
		Graph->AddNode(RemoveDelegateNode, false, false);
		RemoveDelegateNode->AllocateDefaultPins();
		NewNode = RemoveDelegateNode;
	}
	else if (NodeType == TEXT("CallDelegate"))
	{
		FString DelegateName = GetParamString(Params, TEXT("delegate_name"));
		if (DelegateName.IsEmpty()) return ErrorResponse(TEXT("delegate_name is required for CallDelegate"));

		UK2Node_CallDelegate* CallDelegateNode = NewObject<UK2Node_CallDelegate>(Graph);
		CallDelegateNode->DelegateReference.SetSelfMember(FName(*DelegateName));
		CallDelegateNode->NodePosX = Position.X;
		CallDelegateNode->NodePosY = Position.Y;
		Graph->AddNode(CallDelegateNode, false, false);
		CallDelegateNode->AllocateDefaultPins();
		NewNode = CallDelegateNode;
	}
	else if (NodeType == TEXT("ClearDelegate"))
	{
		FString DelegateName = GetParamString(Params, TEXT("delegate_name"));
		if (DelegateName.IsEmpty()) return ErrorResponse(TEXT("delegate_name is required for ClearDelegate"));

		UK2Node_ClearDelegate* ClearDelegateNode = NewObject<UK2Node_ClearDelegate>(Graph);
		ClearDelegateNode->DelegateReference.SetSelfMember(FName(*DelegateName));
		ClearDelegateNode->NodePosX = Position.X;
		ClearDelegateNode->NodePosY = Position.Y;
		Graph->AddNode(ClearDelegateNode, false, false);
		ClearDelegateNode->AllocateDefaultPins();
		NewNode = ClearDelegateNode;
	}

	// ========================================================================
	// TEXT & ENUMS
	// ========================================================================

	else if (NodeType == TEXT("FormatText"))
	{
		UK2Node_FormatText* FormatNode = NewObject<UK2Node_FormatText>(Graph);
		FormatNode->NodePosX = Position.X;
		FormatNode->NodePosY = Position.Y;
		Graph->AddNode(FormatNode, false, false);
		FormatNode->AllocateDefaultPins();
		NewNode = FormatNode;
	}
	else if (NodeType == TEXT("EnumLiteral"))
	{
		FString EnumName = GetParamString(Params, TEXT("enum_name"));
		if (EnumName.IsEmpty()) return ErrorResponse(TEXT("enum_name is required for EnumLiteral"));

		UEnum* Enum = FindEnumByName(EnumName);
		if (!Enum) return ErrorResponse(FString::Printf(TEXT("Enum not found: %s"), *EnumName));

		UK2Node_EnumLiteral* EnumNode = NewObject<UK2Node_EnumLiteral>(Graph);
		EnumNode->Enum = Enum;
		EnumNode->NodePosX = Position.X;
		EnumNode->NodePosY = Position.Y;
		Graph->AddNode(EnumNode, false, false);
		EnumNode->AllocateDefaultPins();
		NewNode = EnumNode;
	}

	// ========================================================================
	// MISC
	// ========================================================================

	else if (NodeType == TEXT("Timeline"))
	{
		FString TimelineName = GetParamString(Params, TEXT("timeline_name"));
		if (TimelineName.IsEmpty()) return ErrorResponse(TEXT("timeline_name is required for Timeline"));

		FName TLName = FName(*TimelineName);

		// Check if timeline template already exists, create if not
		bool bFoundExisting = false;
		for (UTimelineTemplate* TL : BP->Timelines)
		{
			if (TL->GetFName() == TLName)
			{
				bFoundExisting = true;
				break;
			}
		}
		if (!bFoundExisting)
		{
			UTimelineTemplate* NewTimeline = NewObject<UTimelineTemplate>(BP, TLName);
			NewTimeline->SetFlags(RF_Transactional);
			BP->Timelines.Add(NewTimeline);
		}

		UK2Node_Timeline* TimelineNode = NewObject<UK2Node_Timeline>(Graph);
		TimelineNode->TimelineName = TLName;
		TimelineNode->NodePosX = Position.X;
		TimelineNode->NodePosY = Position.Y;
		Graph->AddNode(TimelineNode, false, false);
		TimelineNode->AllocateDefaultPins();
		NewNode = TimelineNode;
	}
	else if (NodeType == TEXT("Knot"))
	{
		UK2Node_Knot* KnotNode = NewObject<UK2Node_Knot>(Graph);
		KnotNode->NodePosX = Position.X;
		KnotNode->NodePosY = Position.Y;
		Graph->AddNode(KnotNode, false, false);
		KnotNode->AllocateDefaultPins();
		NewNode = KnotNode;
	}
	else if (NodeType == TEXT("LoadAsset"))
	{
		UK2Node_LoadAsset* LoadNode = NewObject<UK2Node_LoadAsset>(Graph);
		LoadNode->NodePosX = Position.X;
		LoadNode->NodePosY = Position.Y;
		Graph->AddNode(LoadNode, false, false);
		LoadNode->AllocateDefaultPins();
		NewNode = LoadNode;
	}
	else if (NodeType == TEXT("EaseFunction"))
	{
		UK2Node_EaseFunction* EaseNode = NewObject<UK2Node_EaseFunction>(Graph);
		EaseNode->NodePosX = Position.X;
		EaseNode->NodePosY = Position.Y;
		Graph->AddNode(EaseNode, false, false);
		EaseNode->AllocateDefaultPins();
		NewNode = EaseNode;
	}
	else if (NodeType == TEXT("GetClassDefaults"))
	{
		UK2Node_GetClassDefaults* DefaultsNode = NewObject<UK2Node_GetClassDefaults>(Graph);
		DefaultsNode->NodePosX = Position.X;
		DefaultsNode->NodePosY = Position.Y;
		Graph->AddNode(DefaultsNode, false, false);
		DefaultsNode->AllocateDefaultPins();
		NewNode = DefaultsNode;
	}
	else if (NodeType == TEXT("GetDataTableRow"))
	{
		UK2Node_GetDataTableRow* DataTableNode = NewObject<UK2Node_GetDataTableRow>(Graph);
		DataTableNode->NodePosX = Position.X;
		DataTableNode->NodePosY = Position.Y;
		Graph->AddNode(DataTableNode, false, false);
		DataTableNode->AllocateDefaultPins();
		NewNode = DataTableNode;
	}
	else if (NodeType == TEXT("CommutativeAssociativeBinaryOperator"))
	{
		FString FunctionName = Params->GetStringField(TEXT("function_name"));
		FString TargetClassName = Params->GetStringField(TEXT("target_class"));

		UClass* TargetClass = FindClassByName(TargetClassName);
		if (!TargetClass) return ErrorResponse(FString::Printf(TEXT("Class not found: %s"), *TargetClassName));

		UFunction* Function = TargetClass->FindFunctionByName(FName(*FunctionName));
		if (!Function) return ErrorResponse(FString::Printf(TEXT("Function '%s' not found on %s"), *FunctionName, *TargetClassName));

		UK2Node_CommutativeAssociativeBinaryOperator* MathNode = NewObject<UK2Node_CommutativeAssociativeBinaryOperator>(Graph);
		MathNode->FunctionReference.SetExternalMember(FName(*FunctionName), TargetClass);
		MathNode->NodePosX = Position.X;
		MathNode->NodePosY = Position.Y;
		Graph->AddNode(MathNode, false, false);
		MathNode->AllocateDefaultPins();
		NewNode = MathNode;
	}

	// ========================================================================
	// ENHANCED INPUT
	// ========================================================================

	else if (NodeType == TEXT("EnhancedInputAction"))
	{
		FString InputActionPath = GetParamString(Params, TEXT("input_action_path"));
		if (InputActionPath.IsEmpty()) return ErrorResponse(TEXT("input_action_path is required for EnhancedInputAction (e.g. '/Game/Input/Actions/IA_Interact')"));

		UInputAction* Action = LoadObject<UInputAction>(nullptr, *InputActionPath);
		if (!Action)
		{
			// Try appending asset name if not provided (e.g. '/Game/Input/Actions/IA_Interact' -> '/Game/Input/Actions/IA_Interact.IA_Interact')
			FString AssetName = FPaths::GetCleanFilename(InputActionPath);
			FString FullPath = InputActionPath + TEXT(".") + AssetName;
			Action = LoadObject<UInputAction>(nullptr, *FullPath);
		}
		if (!Action) return ErrorResponse(FString::Printf(TEXT("InputAction not found: %s"), *InputActionPath));

		UK2Node_EnhancedInputAction* InputNode = NewObject<UK2Node_EnhancedInputAction>(Graph);
		InputNode->InputAction = Action;
		InputNode->NodePosX = Position.X;
		InputNode->NodePosY = Position.Y;
		Graph->AddNode(InputNode, false, false);
		InputNode->AllocateDefaultPins();
		NewNode = InputNode;
	}
	else
	{
		return ErrorResponse(FString::Printf(TEXT("Unsupported node type: %s. Supported types: CallFunction, Event, CustomEvent, Branch, Sequence, VariableGet, VariableSet, Self, MacroInstance, MultiGate, Select, DoOnceMultiInput, ForEachElementInEnum, SwitchInteger, SwitchString, SwitchEnum, SwitchName, DynamicCast, ClassDynamicCast, MakeStruct, BreakStruct, SetFieldsInStruct, MakeArray, MakeMap, MakeSet, GetArrayItem, SpawnActorFromClass, GenericCreateObject, AddComponentByClass, CreateDelegate, AddDelegate, RemoveDelegate, CallDelegate, ClearDelegate, FormatText, EnumLiteral, Timeline, Knot, LoadAsset, EaseFunction, GetClassDefaults, GetDataTableRow, CommutativeAssociativeBinaryOperator, EnhancedInputAction"), *NodeType));
	}

	if (!NewNode) return ErrorResponse(TEXT("Failed to create node"));

	// Ensure a valid GUID exists. CreateNewGuid() is called AFTER AddNode and
	// AllocateDefaultPins to prevent either of them from resetting the GUID.
	// If PostInitProperties already generated a valid one, we keep it;
	// otherwise we force-assign a new one.
	if (!NewNode->NodeGuid.IsValid())
	{
		NewNode->NodeGuid = FGuid::NewGuid();
	}

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

// Helper: Convert a type string to FEdGraphPinType (shared mapping for function params and variables)
namespace NodeGraphCommandsLocal
{
static FEdGraphPinType StringToPinType(const FString& TypeStr)
{
	FEdGraphPinType PinType;

	if (TypeStr == TEXT("Boolean")) PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	else if (TypeStr == TEXT("Byte")) PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
	else if (TypeStr == TEXT("Integer")) PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	else if (TypeStr == TEXT("Integer64")) PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
	else if (TypeStr == TEXT("Float"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	}
	else if (TypeStr == TEXT("Double"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (TypeStr == TEXT("String")) PinType.PinCategory = UEdGraphSchema_K2::PC_String;
	else if (TypeStr == TEXT("Text")) PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
	else if (TypeStr == TEXT("Name")) PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
	else if (TypeStr == TEXT("Vector"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (TypeStr == TEXT("Rotator"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else if (TypeStr == TEXT("Transform"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	}
	else if (TypeStr == TEXT("Object") || TypeStr.StartsWith(TEXT("Object:")))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		if (TypeStr.Contains(TEXT(":")))
		{
			FString ClassName = TypeStr.RightChop(TypeStr.Find(TEXT(":")) + 1);
			UClass* ObjClass = FindClassByName(ClassName);
			if (ObjClass) PinType.PinSubCategoryObject = ObjClass;
		}
		else
		{
			PinType.PinSubCategoryObject = UObject::StaticClass();
		}
	}
	else if (TypeStr == TEXT("Class") || TypeStr.StartsWith(TEXT("Class:")))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Class;
		if (TypeStr.Contains(TEXT(":")))
		{
			FString ClassName = TypeStr.RightChop(TypeStr.Find(TEXT(":")) + 1);
			UClass* ObjClass = FindClassByName(ClassName);
			if (ObjClass) PinType.PinSubCategoryObject = ObjClass;
		}
		else
		{
			PinType.PinSubCategoryObject = UObject::StaticClass();
		}
	}
	else
	{
		// Default fallback to wildcard — caller should validate
		PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
	}

	return PinType;
}
} // namespace NodeGraphCommandsLocal (StringToPinType)

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

	FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP, FuncGraph, true, nullptr);

	// Process input parameters
	const TArray<TSharedPtr<FJsonValue>>* InputsArr;
	if (Params->TryGetArrayField(TEXT("inputs"), InputsArr) && InputsArr->Num() > 0)
	{
		UK2Node_FunctionEntry* EntryNode = nullptr;
		for (UEdGraphNode* Node : FuncGraph->Nodes)
		{
			EntryNode = Cast<UK2Node_FunctionEntry>(Node);
			if (EntryNode) break;
		}
		if (EntryNode)
		{
			for (const auto& InputVal : *InputsArr)
			{
				auto InputObj = InputVal->AsObject();
				if (!InputObj.IsValid()) continue;
				FString ParamName = InputObj->GetStringField(TEXT("name"));
				FString ParamType = InputObj->GetStringField(TEXT("type"));
				if (ParamName.IsEmpty() || ParamType.IsEmpty()) continue;

				FEdGraphPinType ParamPinType = StringToPinType(ParamType);

				TSharedPtr<FUserPinInfo> PinInfo = MakeShared<FUserPinInfo>();
				PinInfo->PinName = FName(*ParamName);
				PinInfo->PinType = ParamPinType;
				PinInfo->DesiredPinDirection = EGPD_Output; // Entry outputs = function inputs
				EntryNode->UserDefinedPins.Add(PinInfo);
			}
			EntryNode->ReconstructNode();
		}
	}

	// Process output parameters
	const TArray<TSharedPtr<FJsonValue>>* OutputsArr;
	if (Params->TryGetArrayField(TEXT("outputs"), OutputsArr) && OutputsArr->Num() > 0)
	{
		// Find or create result node
		UK2Node_FunctionResult* ResultNode = nullptr;
		for (UEdGraphNode* Node : FuncGraph->Nodes)
		{
			ResultNode = Cast<UK2Node_FunctionResult>(Node);
			if (ResultNode) break;
		}
		if (!ResultNode)
		{
			// Create a result node if one doesn't exist
			ResultNode = NewObject<UK2Node_FunctionResult>(FuncGraph);
			ResultNode->NodePosX = 600;
			ResultNode->NodePosY = 0;
			FuncGraph->AddNode(ResultNode, false, false);
			ResultNode->AllocateDefaultPins();
		}

		for (const auto& OutputVal : *OutputsArr)
		{
			auto OutputObj = OutputVal->AsObject();
			if (!OutputObj.IsValid()) continue;
			FString ParamName = OutputObj->GetStringField(TEXT("name"));
			FString ParamType = OutputObj->GetStringField(TEXT("type"));
			if (ParamName.IsEmpty() || ParamType.IsEmpty()) continue;

			FEdGraphPinType ParamPinType = StringToPinType(ParamType);

			TSharedPtr<FUserPinInfo> PinInfo = MakeShared<FUserPinInfo>();
			PinInfo->PinName = FName(*ParamName);
			PinInfo->PinType = ParamPinType;
			PinInfo->DesiredPinDirection = EGPD_Input; // Result inputs = function outputs
			ResultNode->UserDefinedPins.Add(PinInfo);
		}
		ResultNode->ReconstructNode();
	}

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

// --- Arrange Nodes (auto-layout) ---

namespace NodeGraphCommandsLocal
{
// Helper: check if a node has any exec pins (input or output)
static bool NodeHasExecPins(UEdGraphNode* Node)
{
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			return true;
	}
	return false;
}

// Helper: get all exec output pins for a node
static TArray<UEdGraphPin*> GetExecOutputPins(UEdGraphNode* Node)
{
	TArray<UEdGraphPin*> Result;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->Direction == EGPD_Output)
			Result.Add(Pin);
	}
	return Result;
}

// Helper: get all exec input pins for a node
static TArray<UEdGraphPin*> GetExecInputPins(UEdGraphNode* Node)
{
	TArray<UEdGraphPin*> Result;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->Direction == EGPD_Input)
			Result.Add(Pin);
	}
	return Result;
}

// Helper: check if an exec input pin has any incoming connections
static bool HasExecInputConnections(UEdGraphNode* Node)
{
	for (UEdGraphPin* Pin : GetExecInputPins(Node))
	{
		if (Pin->LinkedTo.Num() > 0)
			return true;
	}
	return false;
}
} // namespace NodeGraphCommandsLocal (arrange helpers)

TSharedPtr<FJsonObject> FMCPArrangeNodesCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString GraphName = Params->GetStringField(TEXT("graph_name"));

	int32 HSpacing = 350;
	int32 VSpacing = 200;
	int32 SubgraphSpacing = 400;

	double TempVal;
	if (Params->TryGetNumberField(TEXT("horizontal_spacing"), TempVal)) HSpacing = static_cast<int32>(TempVal);
	if (Params->TryGetNumberField(TEXT("vertical_spacing"), TempVal)) VSpacing = static_cast<int32>(TempVal);
	if (Params->TryGetNumberField(TEXT("subgraph_spacing"), TempVal)) SubgraphSpacing = static_cast<int32>(TempVal);

	UBlueprint* BP = LoadBP(AssetPath);
	if (!BP) return ErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));

	UEdGraph* Graph = FindGraph(BP, GraphName);
	if (!Graph) return ErrorResponse(FString::Printf(TEXT("Graph not found: %s"), *GraphName));

	if (Graph->Nodes.Num() == 0) return SuccessResponse(TEXT("Graph has no nodes"));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Arrange Nodes")));

	// Phase 0: Classify nodes as exec-flow or data-only
	TArray<UEdGraphNode*> ExecNodes;
	TArray<UEdGraphNode*> DataOnlyNodes;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (NodeHasExecPins(Node))
			ExecNodes.Add(Node);
		else
			DataOnlyNodes.Add(Node);
	}

	// Phase 1: Build adjacency via exec output pins (exec node → exec node)
	// Maps each exec node to its exec successors
	TMap<UEdGraphNode*, TArray<UEdGraphNode*>> ExecSuccessors;
	TMap<UEdGraphNode*, TArray<UEdGraphNode*>> ExecPredecessors;
	for (UEdGraphNode* Node : ExecNodes)
	{
		ExecSuccessors.FindOrAdd(Node);
		ExecPredecessors.FindOrAdd(Node);
	}

	for (UEdGraphNode* Node : ExecNodes)
	{
		for (UEdGraphPin* OutPin : GetExecOutputPins(Node))
		{
			for (UEdGraphPin* LinkedPin : OutPin->LinkedTo)
			{
				UEdGraphNode* Successor = LinkedPin->GetOwningNode();
				if (ExecSuccessors.Contains(Successor))
				{
					ExecSuccessors[Node].AddUnique(Successor);
					ExecPredecessors[Successor].AddUnique(Node);
				}
			}
		}
	}

	// Phase 2: Find root nodes (exec nodes with no exec input connections)
	TArray<UEdGraphNode*> Roots;
	for (UEdGraphNode* Node : ExecNodes)
	{
		if (!HasExecInputConnections(Node))
			Roots.Add(Node);
	}

	// If no roots found (cycle), pick the first exec node as root
	if (Roots.Num() == 0 && ExecNodes.Num() > 0)
		Roots.Add(ExecNodes[0]);

	// Phase 3: BFS from roots, assign layers using longest-path
	TMap<UEdGraphNode*, int32> NodeLayer;
	for (UEdGraphNode* Node : ExecNodes)
		NodeLayer.Add(Node, -1);

	// Process each root separately to identify subgraphs
	TArray<TArray<UEdGraphNode*>> Subgraphs; // Each subgraph is a list of nodes in BFS order
	TSet<UEdGraphNode*> Visited;

	for (UEdGraphNode* Root : Roots)
	{
		if (Visited.Contains(Root)) continue;

		TArray<UEdGraphNode*> SubgraphNodes;
		TQueue<UEdGraphNode*> Queue;
		Queue.Enqueue(Root);
		Visited.Add(Root);
		NodeLayer[Root] = 0;

		while (!Queue.IsEmpty())
		{
			UEdGraphNode* Current;
			Queue.Dequeue(Current);
			SubgraphNodes.Add(Current);

			int32 CurrentLayer = NodeLayer[Current];
			for (UEdGraphNode* Successor : ExecSuccessors[Current])
			{
				// Longest-path: always try to push successor further right
				int32 NewLayer = CurrentLayer + 1;
				if (NewLayer > NodeLayer[Successor])
				{
					NodeLayer[Successor] = NewLayer;
				}

				if (!Visited.Contains(Successor))
				{
					Visited.Add(Successor);
					Queue.Enqueue(Successor);
				}
			}
		}

		Subgraphs.Add(SubgraphNodes);
	}

	// Handle orphaned exec nodes (not reachable from any root)
	for (UEdGraphNode* Node : ExecNodes)
	{
		if (!Visited.Contains(Node))
		{
			NodeLayer[Node] = 0;
			TArray<UEdGraphNode*> OrphanSubgraph;
			OrphanSubgraph.Add(Node);
			Subgraphs.Add(OrphanSubgraph);
			Visited.Add(Node);
		}
	}

	// Helper: estimate node height from pin count
	auto EstimateNodeHeight = [](UEdGraphNode* Node) -> int32
	{
		int32 NumPins = Node ? Node->Pins.Num() : 0;
		// Header ~50px + ~26px per pin, minimum 80px
		return FMath::Max(80, 50 + NumPins * 26);
	};

	// Phase 4: Group nodes by subgraph + layer
	// Phase 5: Assign positions — layers = X columns, vertical stacking within layer
	int32 GlobalYOffset = 0;

	// Track which data-only nodes we place (to avoid placing them twice)
	TSet<UEdGraphNode*> PlacedDataNodes;

	for (int32 SubIdx = 0; SubIdx < Subgraphs.Num(); ++SubIdx)
	{
		const TArray<UEdGraphNode*>& SubgraphNodes = Subgraphs[SubIdx];

		// Group by layer
		TMap<int32, TArray<UEdGraphNode*>> LayerToNodes;
		int32 MaxLayer = 0;
		for (UEdGraphNode* Node : SubgraphNodes)
		{
			int32 Layer = NodeLayer[Node];
			LayerToNodes.FindOrAdd(Layer).Add(Node);
			if (Layer > MaxLayer) MaxLayer = Layer;
		}

		// Place exec nodes — track per-column Y cursor for accurate stacking
		int32 SubgraphMaxY = GlobalYOffset;
		for (int32 Layer = 0; Layer <= MaxLayer; ++Layer)
		{
			TArray<UEdGraphNode*>* NodesInLayer = LayerToNodes.Find(Layer);
			if (!NodesInLayer) continue;

			int32 LayerY = GlobalYOffset;
			for (int32 Idx = 0; Idx < NodesInLayer->Num(); ++Idx)
			{
				UEdGraphNode* Node = (*NodesInLayer)[Idx];
				Node->NodePosX = Layer * HSpacing;
				Node->NodePosY = LayerY;

				int32 NodeHeight = EstimateNodeHeight(Node);
				LayerY += NodeHeight + VSpacing;
			}
			if (LayerY > SubgraphMaxY) SubgraphMaxY = LayerY;
		}

		// Phase 6: Place data-only nodes near their consumers within this subgraph
		for (UEdGraphNode* Node : SubgraphNodes)
		{
			for (UEdGraphPin* InputPin : Node->Pins)
			{
				if (InputPin->Direction != EGPD_Input) continue;
				if (InputPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;

				for (UEdGraphPin* LinkedPin : InputPin->LinkedTo)
				{
					UEdGraphNode* DataNode = LinkedPin->GetOwningNode();
					if (!DataNode) continue;
					if (PlacedDataNodes.Contains(DataNode)) continue;
					if (!DataOnlyNodes.Contains(DataNode)) continue;

					// Place data node to the left and below its consumer
					DataNode->NodePosX = Node->NodePosX - (HSpacing / 2);
					DataNode->NodePosY = SubgraphMaxY;
					SubgraphMaxY += EstimateNodeHeight(DataNode) + VSpacing;
					PlacedDataNodes.Add(DataNode);
				}
			}
		}

		GlobalYOffset = SubgraphMaxY + SubgraphSpacing;
	}

	// Place remaining data-only nodes that aren't connected to any exec node
	for (UEdGraphNode* DataNode : DataOnlyNodes)
	{
		if (PlacedDataNodes.Contains(DataNode)) continue;

		// Try to place near the first consumer of any kind
		bool bPlaced = false;
		for (UEdGraphPin* OutputPin : DataNode->Pins)
		{
			if (OutputPin->Direction != EGPD_Output) continue;
			for (UEdGraphPin* LinkedPin : OutputPin->LinkedTo)
			{
				UEdGraphNode* Consumer = LinkedPin->GetOwningNode();
				if (Consumer)
				{
					DataNode->NodePosX = Consumer->NodePosX - (HSpacing / 2);
					DataNode->NodePosY = GlobalYOffset;
					GlobalYOffset += EstimateNodeHeight(DataNode) + VSpacing;
					bPlaced = true;
					break;
				}
			}
			if (bPlaced) break;
		}

		if (!bPlaced)
		{
			DataNode->NodePosX = 0;
			DataNode->NodePosY = GlobalYOffset;
			GlobalYOffset += EstimateNodeHeight(DataNode) + VSpacing;
		}
		PlacedDataNodes.Add(DataNode);
	}

	// Phase 7: Build result JSON with new positions
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("nodes_arranged"), Graph->Nodes.Num());
	Data->SetNumberField(TEXT("exec_nodes"), ExecNodes.Num());
	Data->SetNumberField(TEXT("data_nodes"), DataOnlyNodes.Num());
	Data->SetNumberField(TEXT("subgraphs"), Subgraphs.Num());

	TArray<TSharedPtr<FJsonValue>> Positions;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		TSharedPtr<FJsonObject> PosObj = MakeShared<FJsonObject>();
		PosObj->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
		PosObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		PosObj->SetNumberField(TEXT("pos_x"), Node->NodePosX);
		PosObj->SetNumberField(TEXT("pos_y"), Node->NodePosY);
		Positions.Add(MakeShared<FJsonValueObject>(PosObj));
	}
	Data->SetArrayField(TEXT("positions"), Positions);

	return SuccessResponse(Data);
}
