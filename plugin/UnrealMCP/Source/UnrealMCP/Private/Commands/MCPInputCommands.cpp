#include "Commands/MCPInputCommands.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace InputCommandsLocal
{
// ---------------------------------------------------------------------------
// Helper: Create a modifier UObject from a JSON descriptor
// ---------------------------------------------------------------------------
static UInputModifier* CreateModifierFromJson(UObject* Outer, const TSharedPtr<FJsonObject>& Json, FString& OutError)
{
	FString Type;
	if (!Json->TryGetStringField(TEXT("type"), Type))
	{
		OutError = TEXT("Modifier missing 'type' field");
		return nullptr;
	}

	if (Type == TEXT("Negate"))
	{
		UInputModifierNegate* Mod = NewObject<UInputModifierNegate>(Outer);
		bool bX = true, bY = true, bZ = true;
		Json->TryGetBoolField(TEXT("bX"), bX);
		Json->TryGetBoolField(TEXT("bY"), bY);
		Json->TryGetBoolField(TEXT("bZ"), bZ);
		Mod->bX = bX;
		Mod->bY = bY;
		Mod->bZ = bZ;
		return Mod;
	}
	else if (Type == TEXT("SwizzleAxis"))
	{
		UInputModifierSwizzleAxis* Mod = NewObject<UInputModifierSwizzleAxis>(Outer);
		FString Order;
		if (Json->TryGetStringField(TEXT("order"), Order))
		{
			if (Order == TEXT("YXZ"))
			{
				Mod->Order = EInputAxisSwizzle::YXZ;
			}
			else if (Order == TEXT("ZYX"))
			{
				Mod->Order = EInputAxisSwizzle::ZYX;
			}
			else if (Order == TEXT("XZY"))
			{
				Mod->Order = EInputAxisSwizzle::XZY;
			}
			else if (Order == TEXT("YZX"))
			{
				Mod->Order = EInputAxisSwizzle::YZX;
			}
			else if (Order == TEXT("ZXY"))
			{
				Mod->Order = EInputAxisSwizzle::ZXY;
			}
		}
		return Mod;
	}
	else if (Type == TEXT("DeadZone"))
	{
		UInputModifierDeadZone* Mod = NewObject<UInputModifierDeadZone>(Outer);
		double LowerThreshold = 0.2, UpperThreshold = 1.0;
		Json->TryGetNumberField(TEXT("lower_threshold"), LowerThreshold);
		Json->TryGetNumberField(TEXT("upper_threshold"), UpperThreshold);
		Mod->LowerThreshold = (float)LowerThreshold;
		Mod->UpperThreshold = (float)UpperThreshold;

		FString DZType;
		if (Json->TryGetStringField(TEXT("dead_zone_type"), DZType))
		{
			if (DZType == TEXT("Axial"))
			{
				Mod->Type = EDeadZoneType::Axial;
			}
			else if (DZType == TEXT("Radial"))
			{
				Mod->Type = EDeadZoneType::Radial;
			}
		}
		return Mod;
	}
	else if (Type == TEXT("Scalar"))
	{
		UInputModifierScalar* Mod = NewObject<UInputModifierScalar>(Outer);
		double X = 1.0, Y = 1.0, Z = 1.0;
		Json->TryGetNumberField(TEXT("x"), X);
		Json->TryGetNumberField(TEXT("y"), Y);
		Json->TryGetNumberField(TEXT("z"), Z);
		Mod->Scalar = FVector(X, Y, Z);
		return Mod;
	}

	OutError = FString::Printf(TEXT("Unknown modifier type: %s"), *Type);
	return nullptr;
}

// ---------------------------------------------------------------------------
// Helper: Create a trigger UObject from a JSON descriptor
// ---------------------------------------------------------------------------
static UInputTrigger* CreateTriggerFromJson(UObject* Outer, const TSharedPtr<FJsonObject>& Json, FString& OutError)
{
	FString Type;
	if (!Json->TryGetStringField(TEXT("type"), Type))
	{
		OutError = TEXT("Trigger missing 'type' field");
		return nullptr;
	}

	if (Type == TEXT("Down"))
	{
		return NewObject<UInputTriggerDown>(Outer);
	}
	else if (Type == TEXT("Pressed"))
	{
		return NewObject<UInputTriggerPressed>(Outer);
	}
	else if (Type == TEXT("Released"))
	{
		return NewObject<UInputTriggerReleased>(Outer);
	}
	else if (Type == TEXT("Hold"))
	{
		UInputTriggerHold* Trig = NewObject<UInputTriggerHold>(Outer);
		double HoldTime = 0.5;
		Json->TryGetNumberField(TEXT("hold_time_threshold"), HoldTime);
		Trig->HoldTimeThreshold = (float)HoldTime;
		bool bIsOneShot = false;
		Json->TryGetBoolField(TEXT("is_one_shot"), bIsOneShot);
		Trig->bIsOneShot = bIsOneShot;
		return Trig;
	}
	else if (Type == TEXT("Tap"))
	{
		UInputTriggerTap* Trig = NewObject<UInputTriggerTap>(Outer);
		double TapTime = 0.2;
		Json->TryGetNumberField(TEXT("tap_release_time_threshold"), TapTime);
		Trig->TapReleaseTimeThreshold = (float)TapTime;
		return Trig;
	}

	OutError = FString::Printf(TEXT("Unknown trigger type: %s"), *Type);
	return nullptr;
}

// ---------------------------------------------------------------------------
// Helper: Serialize a modifier to JSON
// ---------------------------------------------------------------------------
static TSharedPtr<FJsonObject> ModifierToJson(UInputModifier* Mod)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	if (UInputModifierNegate* Negate = Cast<UInputModifierNegate>(Mod))
	{
		Obj->SetStringField(TEXT("type"), TEXT("Negate"));
		Obj->SetBoolField(TEXT("bX"), Negate->bX);
		Obj->SetBoolField(TEXT("bY"), Negate->bY);
		Obj->SetBoolField(TEXT("bZ"), Negate->bZ);
	}
	else if (UInputModifierSwizzleAxis* Swizzle = Cast<UInputModifierSwizzleAxis>(Mod))
	{
		Obj->SetStringField(TEXT("type"), TEXT("SwizzleAxis"));
		switch (Swizzle->Order)
		{
		case EInputAxisSwizzle::YXZ: Obj->SetStringField(TEXT("order"), TEXT("YXZ")); break;
		case EInputAxisSwizzle::ZYX: Obj->SetStringField(TEXT("order"), TEXT("ZYX")); break;
		case EInputAxisSwizzle::XZY: Obj->SetStringField(TEXT("order"), TEXT("XZY")); break;
		case EInputAxisSwizzle::YZX: Obj->SetStringField(TEXT("order"), TEXT("YZX")); break;
		case EInputAxisSwizzle::ZXY: Obj->SetStringField(TEXT("order"), TEXT("ZXY")); break;
		default: Obj->SetStringField(TEXT("order"), TEXT("YXZ")); break;
		}
	}
	else if (UInputModifierDeadZone* DZ = Cast<UInputModifierDeadZone>(Mod))
	{
		Obj->SetStringField(TEXT("type"), TEXT("DeadZone"));
		Obj->SetNumberField(TEXT("lower_threshold"), DZ->LowerThreshold);
		Obj->SetNumberField(TEXT("upper_threshold"), DZ->UpperThreshold);
		Obj->SetStringField(TEXT("dead_zone_type"), DZ->Type == EDeadZoneType::Axial ? TEXT("Axial") : TEXT("Radial"));
	}
	else if (UInputModifierScalar* Scalar = Cast<UInputModifierScalar>(Mod))
	{
		Obj->SetStringField(TEXT("type"), TEXT("Scalar"));
		Obj->SetNumberField(TEXT("x"), Scalar->Scalar.X);
		Obj->SetNumberField(TEXT("y"), Scalar->Scalar.Y);
		Obj->SetNumberField(TEXT("z"), Scalar->Scalar.Z);
	}
	else
	{
		Obj->SetStringField(TEXT("type"), Mod->GetClass()->GetName());
	}

	return Obj;
}

// ---------------------------------------------------------------------------
// Helper: Serialize a trigger to JSON
// ---------------------------------------------------------------------------
static TSharedPtr<FJsonObject> TriggerToJson(UInputTrigger* Trig)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	if (Cast<UInputTriggerDown>(Trig))
	{
		Obj->SetStringField(TEXT("type"), TEXT("Down"));
	}
	else if (Cast<UInputTriggerPressed>(Trig))
	{
		Obj->SetStringField(TEXT("type"), TEXT("Pressed"));
	}
	else if (Cast<UInputTriggerReleased>(Trig))
	{
		Obj->SetStringField(TEXT("type"), TEXT("Released"));
	}
	else if (UInputTriggerHold* Hold = Cast<UInputTriggerHold>(Trig))
	{
		Obj->SetStringField(TEXT("type"), TEXT("Hold"));
		Obj->SetNumberField(TEXT("hold_time_threshold"), Hold->HoldTimeThreshold);
		Obj->SetBoolField(TEXT("is_one_shot"), Hold->bIsOneShot);
	}
	else if (UInputTriggerTap* Tap = Cast<UInputTriggerTap>(Trig))
	{
		Obj->SetStringField(TEXT("type"), TEXT("Tap"));
		Obj->SetNumberField(TEXT("tap_release_time_threshold"), Tap->TapReleaseTimeThreshold);
	}
	else
	{
		Obj->SetStringField(TEXT("type"), Trig->GetClass()->GetName());
	}

	return Obj;
}

// ---------------------------------------------------------------------------
// Helper: Convert EInputActionValueType to string
// ---------------------------------------------------------------------------
static FString ValueTypeToString(EInputActionValueType Type)
{
	switch (Type)
	{
	case EInputActionValueType::Boolean: return TEXT("Boolean");
	case EInputActionValueType::Axis1D: return TEXT("Axis1D");
	case EInputActionValueType::Axis2D: return TEXT("Axis2D");
	case EInputActionValueType::Axis3D: return TEXT("Axis3D");
	default: return TEXT("Boolean");
	}
}

// ---------------------------------------------------------------------------
// Helper: Parse string to EInputActionValueType
// ---------------------------------------------------------------------------
static EInputActionValueType StringToValueType(const FString& Str)
{
	if (Str == TEXT("Axis1D")) return EInputActionValueType::Axis1D;
	if (Str == TEXT("Axis2D")) return EInputActionValueType::Axis2D;
	if (Str == TEXT("Axis3D")) return EInputActionValueType::Axis3D;
	return EInputActionValueType::Boolean;
}
} // namespace InputCommandsLocal

using namespace InputCommandsLocal;

// ===================================================================
// create_input_action
// ===================================================================
TSharedPtr<FJsonObject> FMCPCreateInputActionCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetName = Params->GetStringField(TEXT("asset_name"));
	FString PackagePath = Params->GetStringField(TEXT("package_path"));

	if (AssetName.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_name is required"));
	}
	if (PackagePath.IsEmpty())
	{
		PackagePath = TEXT("/Game/Input");
	}

	FString FullPath = PackagePath / AssetName;
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to create package at '%s'"), *FullPath));
	}

	UInputAction* NewAction = NewObject<UInputAction>(Package, *AssetName, RF_Standalone | RF_Public);
	if (!NewAction)
	{
		return ErrorResponse(TEXT("Failed to create InputAction object"));
	}

	// Set value type
	FString ValueTypeStr;
	if (Params->TryGetStringField(TEXT("value_type"), ValueTypeStr))
	{
		NewAction->ValueType = StringToValueType(ValueTypeStr);
	}

	// Set optional properties
	bool bConsumeInput = true;
	if (Params->TryGetBoolField(TEXT("consume_input"), bConsumeInput))
	{
		NewAction->bConsumeInput = bConsumeInput;
	}

	bool bTriggerWhenPaused = false;
	if (Params->TryGetBoolField(TEXT("trigger_when_paused"), bTriggerWhenPaused))
	{
		NewAction->bTriggerWhenPaused = bTriggerWhenPaused;
	}

	FAssetRegistryModule::AssetCreated(NewAction);
	Package->FullyLoad();
	Package->SetDirtyFlag(true);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), NewAction->GetPathName());
	Data->SetStringField(TEXT("asset_name"), AssetName);
	Data->SetStringField(TEXT("value_type"), ValueTypeToString(NewAction->ValueType));
	Data->SetBoolField(TEXT("consume_input"), NewAction->bConsumeInput);
	Data->SetBoolField(TEXT("trigger_when_paused"), NewAction->bTriggerWhenPaused);
	return SuccessResponse(Data);
}

// ===================================================================
// create_input_mapping_context
// ===================================================================
TSharedPtr<FJsonObject> FMCPCreateInputMappingContextCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetName = Params->GetStringField(TEXT("asset_name"));
	FString PackagePath = Params->GetStringField(TEXT("package_path"));

	if (AssetName.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_name is required"));
	}
	if (PackagePath.IsEmpty())
	{
		PackagePath = TEXT("/Game/Input");
	}

	FString FullPath = PackagePath / AssetName;
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to create package at '%s'"), *FullPath));
	}

	UInputMappingContext* NewContext = NewObject<UInputMappingContext>(Package, *AssetName, RF_Standalone | RF_Public);
	if (!NewContext)
	{
		return ErrorResponse(TEXT("Failed to create InputMappingContext object"));
	}

	FAssetRegistryModule::AssetCreated(NewContext);
	Package->FullyLoad();
	Package->SetDirtyFlag(true);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), NewContext->GetPathName());
	Data->SetStringField(TEXT("asset_name"), AssetName);
	Data->SetNumberField(TEXT("mappings_count"), 0);
	return SuccessResponse(Data);
}

// ===================================================================
// add_input_mapping
// ===================================================================
TSharedPtr<FJsonObject> FMCPAddInputMappingCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ContextPath = Params->GetStringField(TEXT("context_path"));
	FString ActionPath = Params->GetStringField(TEXT("action_path"));
	FString KeyName = Params->GetStringField(TEXT("key"));

	if (ContextPath.IsEmpty())
	{
		return ErrorResponse(TEXT("context_path is required"));
	}
	if (ActionPath.IsEmpty())
	{
		return ErrorResponse(TEXT("action_path is required"));
	}
	if (KeyName.IsEmpty())
	{
		return ErrorResponse(TEXT("key is required"));
	}

	// Load context
	UInputMappingContext* Context = Cast<UInputMappingContext>(
		StaticLoadObject(UInputMappingContext::StaticClass(), nullptr, *ContextPath));
	if (!Context)
	{
		return ErrorResponse(FString::Printf(TEXT("InputMappingContext not found: %s"), *ContextPath));
	}

	// Load action
	UInputAction* Action = Cast<UInputAction>(
		StaticLoadObject(UInputAction::StaticClass(), nullptr, *ActionPath));
	if (!Action)
	{
		return ErrorResponse(FString::Printf(TEXT("InputAction not found: %s"), *ActionPath));
	}

	// Validate key
	FKey Key(*KeyName);
	if (!Key.IsValid())
	{
		return ErrorResponse(FString::Printf(TEXT("Invalid key name: %s"), *KeyName));
	}

	// Map key to action
	FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);

	// Process modifiers
	int32 ModifiersCount = 0;
	const TArray<TSharedPtr<FJsonValue>>* ModifiersArr = nullptr;
	if (Params->TryGetArrayField(TEXT("modifiers"), ModifiersArr) && ModifiersArr)
	{
		for (const TSharedPtr<FJsonValue>& ModVal : *ModifiersArr)
		{
			const TSharedPtr<FJsonObject>* ModObj = nullptr;
			if (ModVal->TryGetObject(ModObj) && ModObj)
			{
				FString Error;
				UInputModifier* Mod = CreateModifierFromJson(Context, *ModObj, Error);
				if (Mod)
				{
					Mapping.Modifiers.Add(Mod);
					ModifiersCount++;
				}
				else
				{
					return ErrorResponse(FString::Printf(TEXT("Failed to create modifier: %s"), *Error));
				}
			}
		}
	}

	// Process triggers
	int32 TriggersCount = 0;
	const TArray<TSharedPtr<FJsonValue>>* TriggersArr = nullptr;
	if (Params->TryGetArrayField(TEXT("triggers"), TriggersArr) && TriggersArr)
	{
		for (const TSharedPtr<FJsonValue>& TrigVal : *TriggersArr)
		{
			const TSharedPtr<FJsonObject>* TrigObj = nullptr;
			if (TrigVal->TryGetObject(TrigObj) && TrigObj)
			{
				FString Error;
				UInputTrigger* Trig = CreateTriggerFromJson(Context, *TrigObj, Error);
				if (Trig)
				{
					Mapping.Triggers.Add(Trig);
					TriggersCount++;
				}
				else
				{
					return ErrorResponse(FString::Printf(TEXT("Failed to create trigger: %s"), *Error));
				}
			}
		}
	}

	Context->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("context_path"), Context->GetPathName());
	Data->SetStringField(TEXT("action"), Action->GetName());
	Data->SetStringField(TEXT("key"), KeyName);
	Data->SetNumberField(TEXT("modifiers_count"), ModifiersCount);
	Data->SetNumberField(TEXT("triggers_count"), TriggersCount);
	Data->SetNumberField(TEXT("total_mappings"), Context->GetMappings().Num());
	return SuccessResponse(Data);
}

// ===================================================================
// remove_input_mapping
// ===================================================================
TSharedPtr<FJsonObject> FMCPRemoveInputMappingCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ContextPath = Params->GetStringField(TEXT("context_path"));
	FString ActionPath = Params->GetStringField(TEXT("action_path"));
	FString KeyName = Params->GetStringField(TEXT("key"));

	if (ContextPath.IsEmpty())
	{
		return ErrorResponse(TEXT("context_path is required"));
	}

	// Load context
	UInputMappingContext* Context = Cast<UInputMappingContext>(
		StaticLoadObject(UInputMappingContext::StaticClass(), nullptr, *ContextPath));
	if (!Context)
	{
		return ErrorResponse(FString::Printf(TEXT("InputMappingContext not found: %s"), *ContextPath));
	}

	int32 BeforeCount = Context->GetMappings().Num();

	if (!ActionPath.IsEmpty() && !KeyName.IsEmpty())
	{
		// Remove specific key+action mapping
		UInputAction* Action = Cast<UInputAction>(
			StaticLoadObject(UInputAction::StaticClass(), nullptr, *ActionPath));
		if (!Action)
		{
			return ErrorResponse(FString::Printf(TEXT("InputAction not found: %s"), *ActionPath));
		}

		FKey Key(*KeyName);
		if (!Key.IsValid())
		{
			return ErrorResponse(FString::Printf(TEXT("Invalid key name: %s"), *KeyName));
		}

		Context->UnmapKey(Action, Key);
	}
	else if (!ActionPath.IsEmpty())
	{
		// Remove all mappings for this action
		UInputAction* Action = Cast<UInputAction>(
			StaticLoadObject(UInputAction::StaticClass(), nullptr, *ActionPath));
		if (!Action)
		{
			return ErrorResponse(FString::Printf(TEXT("InputAction not found: %s"), *ActionPath));
		}

		Context->UnmapAllKeysFromAction(Action);
	}
	else if (!KeyName.IsEmpty())
	{
		// Remove all mappings for this key (iterate backwards)
		FKey Key(*KeyName);
		if (!Key.IsValid())
		{
			return ErrorResponse(FString::Printf(TEXT("Invalid key name: %s"), *KeyName));
		}

		// UnmapKey needs an action, so iterate mappings to find all actions for this key
		TArray<const UInputAction*> ActionsToUnmap;
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (Mapping.Key == Key)
			{
				ActionsToUnmap.Add(Mapping.Action);
			}
		}
		for (const UInputAction* Action : ActionsToUnmap)
		{
			Context->UnmapKey(Action, Key);
		}
	}
	else
	{
		return ErrorResponse(TEXT("At least one of action_path or key is required"));
	}

	int32 AfterCount = Context->GetMappings().Num();
	Context->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("context_path"), Context->GetPathName());
	Data->SetNumberField(TEXT("removed_count"), BeforeCount - AfterCount);
	Data->SetNumberField(TEXT("remaining_mappings"), AfterCount);
	return SuccessResponse(Data);
}

// ===================================================================
// get_input_mapping_context
// ===================================================================
TSharedPtr<FJsonObject> FMCPGetInputMappingContextCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ContextPath = Params->GetStringField(TEXT("context_path"));

	if (ContextPath.IsEmpty())
	{
		return ErrorResponse(TEXT("context_path is required"));
	}

	UInputMappingContext* Context = Cast<UInputMappingContext>(
		StaticLoadObject(UInputMappingContext::StaticClass(), nullptr, *ContextPath));
	if (!Context)
	{
		return ErrorResponse(FString::Printf(TEXT("InputMappingContext not found: %s"), *ContextPath));
	}

	const TArray<FEnhancedActionKeyMapping>& Mappings = Context->GetMappings();

	TArray<TSharedPtr<FJsonValue>> MappingsArr;
	for (const FEnhancedActionKeyMapping& Mapping : Mappings)
	{
		TSharedPtr<FJsonObject> MappingObj = MakeShared<FJsonObject>();

		// Action info
		if (Mapping.Action)
		{
			MappingObj->SetStringField(TEXT("action"), Mapping.Action->GetName());
			MappingObj->SetStringField(TEXT("action_path"), Mapping.Action->GetPathName());
			MappingObj->SetStringField(TEXT("value_type"), ValueTypeToString(Mapping.Action->ValueType));
		}
		else
		{
			MappingObj->SetStringField(TEXT("action"), TEXT("None"));
			MappingObj->SetStringField(TEXT("action_path"), TEXT(""));
			MappingObj->SetStringField(TEXT("value_type"), TEXT("Boolean"));
		}

		// Key
		MappingObj->SetStringField(TEXT("key"), Mapping.Key.GetFName().ToString());

		// Modifiers
		TArray<TSharedPtr<FJsonValue>> ModArr;
		for (UInputModifier* Mod : Mapping.Modifiers)
		{
			if (Mod)
			{
				ModArr.Add(MakeShared<FJsonValueObject>(ModifierToJson(Mod)));
			}
		}
		MappingObj->SetArrayField(TEXT("modifiers"), ModArr);

		// Triggers
		TArray<TSharedPtr<FJsonValue>> TrigArr;
		for (UInputTrigger* Trig : Mapping.Triggers)
		{
			if (Trig)
			{
				TrigArr.Add(MakeShared<FJsonValueObject>(TriggerToJson(Trig)));
			}
		}
		MappingObj->SetArrayField(TEXT("triggers"), TrigArr);

		MappingsArr.Add(MakeShared<FJsonValueObject>(MappingObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("context_name"), Context->GetName());
	Data->SetArrayField(TEXT("mappings"), MappingsArr);
	Data->SetNumberField(TEXT("total_mappings"), Mappings.Num());
	return SuccessResponse(Data);
}
