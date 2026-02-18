#include "Commands/MCPMaterialCommands.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "UObject/SavePackage.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Factories/MaterialFactoryNew.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "MaterialEditingLibrary.h"

namespace MaterialCommandsLocal
{
static AActor* FindActorByNameInWorld(const FString& ActorName)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return nullptr;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
		{
			return *It;
		}
	}
	return nullptr;
}
} // namespace MaterialCommandsLocal

using namespace MaterialCommandsLocal;

// --- Create Material ---
TSharedPtr<FJsonObject> FMCPCreateMaterialCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Name = Params->GetStringField(TEXT("name"));
	FString Path = Params->GetStringField(TEXT("path"));

	if (Name.IsEmpty())
	{
		return ErrorResponse(TEXT("Material name is required"));
	}
	if (Path.IsEmpty())
	{
		Path = TEXT("/Game/Materials");
	}

	FString PackagePath = Path / Name;

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to create package at '%s'"), *PackagePath));
	}

	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	UMaterial* NewMaterial = Cast<UMaterial>(
		Factory->FactoryCreateNew(
			UMaterial::StaticClass(),
			Package,
			*Name,
			RF_Standalone | RF_Public,
			nullptr,
			GWarn
		)
	);

	if (!NewMaterial)
	{
		return ErrorResponse(TEXT("Failed to create material"));
	}

	FAssetRegistryModule::AssetCreated(NewMaterial);
	Package->FullyLoad();
	Package->SetDirtyFlag(true);

	// Read optional initial property values
	double BaseColorR = 0.8, BaseColorG = 0.8, BaseColorB = 0.8;
	double Roughness = 0.5, Metallic = 0.0;

	const TArray<TSharedPtr<FJsonValue>>* ColorArr;
	if (Params->TryGetArrayField(TEXT("base_color"), ColorArr) && ColorArr->Num() >= 3)
	{
		BaseColorR = (*ColorArr)[0]->AsNumber();
		BaseColorG = (*ColorArr)[1]->AsNumber();
		BaseColorB = (*ColorArr)[2]->AsNumber();
	}
	Params->TryGetNumberField(TEXT("roughness"), Roughness);
	Params->TryGetNumberField(TEXT("metallic"), Metallic);

	// Build expressions via UE5 editor-only data API
	UMaterialEditorOnlyData* EditorData = NewMaterial->GetEditorOnlyData();

	// Base color expression
	UMaterialExpressionConstant4Vector* ColorExpr =
		NewObject<UMaterialExpressionConstant4Vector>(NewMaterial);
	ColorExpr->Constant = FLinearColor(
		static_cast<float>(BaseColorR),
		static_cast<float>(BaseColorG),
		static_cast<float>(BaseColorB),
		1.0f
	);
	ColorExpr->MaterialExpressionEditorX = -400;
	ColorExpr->MaterialExpressionEditorY = -200;
	EditorData->ExpressionCollection.Expressions.Add(ColorExpr);
	EditorData->BaseColor.Connect(0, ColorExpr);

	// Roughness expression
	UMaterialExpressionConstant* RoughnessExpr =
		NewObject<UMaterialExpressionConstant>(NewMaterial);
	RoughnessExpr->R = static_cast<float>(Roughness);
	RoughnessExpr->MaterialExpressionEditorX = -400;
	RoughnessExpr->MaterialExpressionEditorY = 0;
	EditorData->ExpressionCollection.Expressions.Add(RoughnessExpr);
	EditorData->Roughness.Connect(0, RoughnessExpr);

	// Metallic expression
	UMaterialExpressionConstant* MetallicExpr =
		NewObject<UMaterialExpressionConstant>(NewMaterial);
	MetallicExpr->R = static_cast<float>(Metallic);
	MetallicExpr->MaterialExpressionEditorX = -400;
	MetallicExpr->MaterialExpressionEditorY = 200;
	EditorData->ExpressionCollection.Expressions.Add(MetallicExpr);
	EditorData->Metallic.Connect(0, MetallicExpr);

	// Compile the material
	NewMaterial->PreEditChange(nullptr);
	NewMaterial->PostEditChange();
	UMaterialEditingLibrary::RecompileMaterial(NewMaterial);

	// Save
	FString PackageFileName = FPackageName::LongPackageNameToFilename(
		PackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, NewMaterial, *PackageFileName, SaveArgs);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Name);
	Data->SetStringField(TEXT("asset_path"), NewMaterial->GetPathName());
	return SuccessResponse(Data);
}

// --- Assign Material ---
TSharedPtr<FJsonObject> FMCPAssignMaterialCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName = Params->GetStringField(TEXT("actor_name"));
	FString MaterialPath = Params->GetStringField(TEXT("material_path"));
	int32 MaterialSlot = 0;
	Params->TryGetNumberField(TEXT("slot"), MaterialSlot);

	AActor* TargetActor = FindActorByNameInWorld(ActorName);
	if (!TargetActor)
	{
		return ErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	UMaterialInterface* Material = Cast<UMaterialInterface>(
		StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialPath)
	);
	if (!Material)
	{
		return ErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	FString ComponentName = Params->GetStringField(TEXT("component_name"));

	// Find a mesh component on the actor
	UMeshComponent* MeshComp = nullptr;
	TInlineComponentArray<UMeshComponent*> MeshComps;
	TargetActor->GetComponents<UMeshComponent>(MeshComps);

	if (!ComponentName.IsEmpty())
	{
		for (UMeshComponent* C : MeshComps)
		{
			if (C->GetName() == ComponentName)
			{
				MeshComp = C;
				break;
			}
		}
	}
	else if (MeshComps.Num() > 0)
	{
		MeshComp = MeshComps[0];
	}

	if (!MeshComp)
	{
		return ErrorResponse(TEXT("No mesh component found on actor"));
	}

	if (MaterialSlot < 0 || MaterialSlot >= MeshComp->GetNumMaterials())
	{
		MaterialSlot = 0;
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Assign Material")));
	MeshComp->Modify();
	MeshComp->SetMaterial(MaterialSlot, Material);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor"), TargetActor->GetActorLabel());
	Data->SetStringField(TEXT("component"), MeshComp->GetName());
	Data->SetStringField(TEXT("material"), Material->GetPathName());
	Data->SetNumberField(TEXT("slot"), MaterialSlot);
	return SuccessResponse(Data);
}

// --- Modify Material ---
TSharedPtr<FJsonObject> FMCPModifyMaterialCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UMaterialInterface* MatInterface = Cast<UMaterialInterface>(
		StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *AssetPath)
	);
	if (!MatInterface)
	{
		return ErrorResponse(FString::Printf(TEXT("Material not found: %s"), *AssetPath));
	}

	// Try as MaterialInstanceConstant first (parameter overrides)
	UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(MatInterface);
	if (MIC)
	{
		FScopedTransaction Transaction(FText::FromString(TEXT("MCP Modify Material Instance")));
		MIC->Modify();
		MIC->PreEditChange(nullptr);

		// Set scalar parameters
		const TSharedPtr<FJsonObject>* ScalarParamsObj;
		if (Params->TryGetObjectField(TEXT("scalar_params"), ScalarParamsObj))
		{
			for (auto& Pair : (*ScalarParamsObj)->Values)
			{
				FName ParamName(*Pair.Key);
				float Value = static_cast<float>(Pair.Value->AsNumber());
				MIC->SetScalarParameterValueEditorOnly(ParamName, Value);
			}
		}

		// Set vector parameters
		const TSharedPtr<FJsonObject>* VectorParamsObj;
		if (Params->TryGetObjectField(TEXT("vector_params"), VectorParamsObj))
		{
			for (auto& Pair : (*VectorParamsObj)->Values)
			{
				FName ParamName(*Pair.Key);
				const TArray<TSharedPtr<FJsonValue>>* ValArr;
				if (Pair.Value->TryGetArray(ValArr) && ValArr->Num() >= 3)
				{
					FLinearColor Color(
						static_cast<float>((*ValArr)[0]->AsNumber()),
						static_cast<float>((*ValArr)[1]->AsNumber()),
						static_cast<float>((*ValArr)[2]->AsNumber()),
						ValArr->Num() >= 4 ? static_cast<float>((*ValArr)[3]->AsNumber()) : 1.0f
					);
					MIC->SetVectorParameterValueEditorOnly(ParamName, Color);
				}
			}
		}

		MIC->PostEditChange();
		MIC->MarkPackageDirty();

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("asset_path"), MIC->GetPathName());
		Data->SetStringField(TEXT("type"), TEXT("MaterialInstanceConstant"));
		return SuccessResponse(Data);
	}

	// For base UMaterial, modify the expression constants directly
	UMaterial* Mat = Cast<UMaterial>(MatInterface);
	if (Mat)
	{
		FScopedTransaction Transaction(FText::FromString(TEXT("MCP Modify Material")));
		UMaterialEditorOnlyData* EditorData = Mat->GetEditorOnlyData();
		if (!EditorData)
		{
			return ErrorResponse(TEXT("Cannot access material editor data"));
		}

		// Update base color if provided
		const TArray<TSharedPtr<FJsonValue>>* ColorArr;
		if (Params->TryGetArrayField(TEXT("base_color"), ColorArr) && ColorArr->Num() >= 3)
		{
			// Find existing Constant4Vector connected to BaseColor, or create one
			for (UMaterialExpression* Expr : EditorData->ExpressionCollection.Expressions)
			{
				if (UMaterialExpressionConstant4Vector* C4V = Cast<UMaterialExpressionConstant4Vector>(Expr))
				{
					C4V->Constant = FLinearColor(
						static_cast<float>((*ColorArr)[0]->AsNumber()),
						static_cast<float>((*ColorArr)[1]->AsNumber()),
						static_cast<float>((*ColorArr)[2]->AsNumber()),
						1.0f
					);
					break;
				}
			}
		}

		double Roughness = -1.0;
		if (Params->TryGetNumberField(TEXT("roughness"), Roughness))
		{
			// Find first constant connected to roughness
			for (UMaterialExpression* Expr : EditorData->ExpressionCollection.Expressions)
			{
				if (UMaterialExpressionConstant* C = Cast<UMaterialExpressionConstant>(Expr))
				{
					if (C->MaterialExpressionEditorY == 0) // Convention: roughness at Y=0
					{
						C->R = static_cast<float>(Roughness);
						break;
					}
				}
			}
		}

		double Metallic = -1.0;
		if (Params->TryGetNumberField(TEXT("metallic"), Metallic))
		{
			for (UMaterialExpression* Expr : EditorData->ExpressionCollection.Expressions)
			{
				if (UMaterialExpressionConstant* C = Cast<UMaterialExpressionConstant>(Expr))
				{
					if (C->MaterialExpressionEditorY == 200) // Convention: metallic at Y=200
					{
						C->R = static_cast<float>(Metallic);
						break;
					}
				}
			}
		}

		Mat->PreEditChange(nullptr);
		Mat->PostEditChange();
		UMaterialEditingLibrary::RecompileMaterial(Mat);

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("asset_path"), Mat->GetPathName());
		Data->SetStringField(TEXT("type"), TEXT("Material"));
		return SuccessResponse(Data);
	}

	return ErrorResponse(TEXT("Unsupported material type"));
}

// --- Get Material Info ---
TSharedPtr<FJsonObject> FMCPGetMaterialInfoCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UMaterialInterface* MatInterface = Cast<UMaterialInterface>(
		StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *AssetPath)
	);
	if (!MatInterface)
	{
		return ErrorResponse(FString::Printf(TEXT("Material not found: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), MatInterface->GetName());
	Data->SetStringField(TEXT("path"), MatInterface->GetPathName());
	Data->SetStringField(TEXT("class"), MatInterface->GetClass()->GetName());

	// Scalar parameters
	{
		TArray<FName> ScalarNames;
		UMaterialEditingLibrary::GetScalarParameterNames(MatInterface, ScalarNames);

		TArray<TSharedPtr<FJsonValue>> ScalarArr;
		for (const FName& PName : ScalarNames)
		{
			TSharedPtr<FJsonObject> ParamInfo = MakeShared<FJsonObject>();
			ParamInfo->SetStringField(TEXT("name"), PName.ToString());

			float ScalarValue = 0.0f;
			MatInterface->GetScalarParameterValue(FHashedMaterialParameterInfo(PName), ScalarValue);
			ParamInfo->SetNumberField(TEXT("value"), ScalarValue);

			ScalarArr.Add(MakeShared<FJsonValueObject>(ParamInfo));
		}
		Data->SetArrayField(TEXT("scalar_parameters"), ScalarArr);
	}

	// Vector parameters
	{
		TArray<FName> VectorNames;
		UMaterialEditingLibrary::GetVectorParameterNames(MatInterface, VectorNames);

		TArray<TSharedPtr<FJsonValue>> VectorArr;
		for (const FName& PName : VectorNames)
		{
			TSharedPtr<FJsonObject> ParamInfo = MakeShared<FJsonObject>();
			ParamInfo->SetStringField(TEXT("name"), PName.ToString());

			FLinearColor VectorValue = FLinearColor::Black;
			MatInterface->GetVectorParameterValue(FHashedMaterialParameterInfo(PName), VectorValue);

			TArray<TSharedPtr<FJsonValue>> RGBA = {
				MakeShared<FJsonValueNumber>(VectorValue.R),
				MakeShared<FJsonValueNumber>(VectorValue.G),
				MakeShared<FJsonValueNumber>(VectorValue.B),
				MakeShared<FJsonValueNumber>(VectorValue.A)
			};
			ParamInfo->SetArrayField(TEXT("value"), RGBA);
			VectorArr.Add(MakeShared<FJsonValueObject>(ParamInfo));
		}
		Data->SetArrayField(TEXT("vector_parameters"), VectorArr);
	}

	// Type-specific info
	if (UMaterial* Mat = Cast<UMaterial>(MatInterface))
	{
		UMaterialEditorOnlyData* EditorData = Mat->GetEditorOnlyData();
		if (EditorData)
		{
			Data->SetNumberField(TEXT("expression_count"),
				EditorData->ExpressionCollection.Expressions.Num());
		}
		Data->SetBoolField(TEXT("is_base_material"), true);
	}
	else if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(MatInterface))
	{
		if (MIC->Parent)
		{
			Data->SetStringField(TEXT("parent_material"), MIC->Parent->GetPathName());
		}
		Data->SetBoolField(TEXT("is_base_material"), false);
	}

	return SuccessResponse(Data);
}
