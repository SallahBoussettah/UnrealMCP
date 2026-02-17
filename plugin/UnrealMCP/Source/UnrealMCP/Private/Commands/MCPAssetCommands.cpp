#include "Commands/MCPAssetCommands.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetImportTask.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Particles/ParticleSystem.h"
#include "Factories/Factory.h"

// Type name to UClass mapping for search_assets filtering
static UClass* ResolveAssetClass(const FString& TypeName)
{
	if (TypeName.IsEmpty()) return nullptr;

	static TMap<FString, UClass*> TypeMap;
	if (TypeMap.Num() == 0)
	{
		TypeMap.Add(TEXT("StaticMesh"), UStaticMesh::StaticClass());
		TypeMap.Add(TEXT("SkeletalMesh"), USkeletalMesh::StaticClass());
		TypeMap.Add(TEXT("Texture2D"), UTexture2D::StaticClass());
		TypeMap.Add(TEXT("Material"), UMaterial::StaticClass());
		TypeMap.Add(TEXT("MaterialInstanceConstant"), UMaterialInstanceConstant::StaticClass());
		TypeMap.Add(TEXT("SoundWave"), USoundWave::StaticClass());
		TypeMap.Add(TEXT("Blueprint"), UBlueprint::StaticClass());
		TypeMap.Add(TEXT("World"), UWorld::StaticClass());
		TypeMap.Add(TEXT("AnimSequence"), UAnimSequence::StaticClass());
		TypeMap.Add(TEXT("ParticleSystem"), UParticleSystem::StaticClass());
	}

	if (UClass** Found = TypeMap.Find(TypeName))
	{
		return *Found;
	}

	// Try FindObject as fallback for arbitrary class names
	FString ClassPath = FString::Printf(TEXT("/Script/Engine.%s"), *TypeName);
	UClass* FoundClass = FindObject<UClass>(nullptr, *ClassPath);
	if (!FoundClass)
	{
		ClassPath = FString::Printf(TEXT("/Script/CoreUObject.%s"), *TypeName);
		FoundClass = FindObject<UClass>(nullptr, *ClassPath);
	}
	return FoundClass;
}

// Resolve an asset path to FAssetData. Handles both full object paths
// (/Game/Foo.Foo) and short package paths (/Game/Foo). Skips Package-class
// results when the actual asset (StaticMesh, Texture2D, etc.) is available.
static FAssetData ResolveAssetData(IAssetRegistry& AssetRegistry, const FString& AssetPath)
{
	// 1. Try as-is (handles full object paths like /Game/Foo.Foo)
	FAssetData Result = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
	if (Result.IsValid() && Result.AssetClassPath.GetAssetName() != TEXT("Package"))
	{
		return Result;
	}

	// 2. Auto-append asset name: /Game/Foo -> /Game/Foo.Foo
	if (!AssetPath.Contains(TEXT(".")))
	{
		FString ShortName = FPackageName::GetShortName(AssetPath);
		FString FullPath = AssetPath + TEXT(".") + ShortName;
		FAssetData FullResult = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(FullPath));
		if (FullResult.IsValid())
		{
			return FullResult;
		}
	}

	// 3. Search by package name, pick first non-Package asset
	TArray<FAssetData> InPackage;
	AssetRegistry.GetAssetsByPackageName(FName(*AssetPath), InPackage);
	for (const FAssetData& AD : InPackage)
	{
		if (AD.AssetClassPath.GetAssetName() != TEXT("Package"))
		{
			return AD;
		}
	}

	// 4. Return whatever we have (may be Package or invalid)
	if (InPackage.Num() > 0) return InPackage[0];
	return Result;
}

// Find a legacy (non-Interchange) factory for a file extension.
// Interchange uses async TaskGraph tasks internally, which crashes when called
// from our game-thread AsyncTask dispatch (TaskGraph recursion guard).
// By selecting a legacy factory explicitly, we bypass Interchange entirely.
static UFactory* FindLegacyFactoryForExtension(const FString& Extension)
{
	UClass* BestClass = nullptr;
	int32 BestPriority = -1;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(UFactory::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
			continue;

		// Skip Interchange factories to avoid TaskGraph recursion crash
		FString ClassName = Class->GetName();
		if (ClassName.Contains(TEXT("Interchange")))
			continue;

		UFactory* CDO = Class->GetDefaultObject<UFactory>();
		if (!CDO || !CDO->bEditorImport)
			continue;

		for (const FString& Format : CDO->Formats)
		{
			FString Ext;
			if (Format.Split(TEXT(";"), &Ext, nullptr) && Ext.Equals(Extension, ESearchCase::IgnoreCase))
			{
				if (CDO->ImportPriority > BestPriority)
				{
					BestClass = Class;
					BestPriority = CDO->ImportPriority;
				}
				break;
			}
		}
	}

	if (!BestClass) return nullptr;
	return NewObject<UFactory>(GetTransientPackage(), BestClass);
}

// --- Import Asset ---
TSharedPtr<FJsonObject> FMCPImportAssetCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString FilePath = Params->GetStringField(TEXT("file_path"));
	FString DestinationPath = Params->GetStringField(TEXT("destination_path"));
	FString AssetName = Params->GetStringField(TEXT("asset_name"));

	if (FilePath.IsEmpty())
	{
		return ErrorResponse(TEXT("file_path is required"));
	}
	if (DestinationPath.IsEmpty())
	{
		return ErrorResponse(TEXT("destination_path is required (e.g., '/Game/Textures')"));
	}

	// Normalize and validate file path
	FPaths::NormalizeFilename(FilePath);
	if (!FPaths::FileExists(FilePath))
	{
		return ErrorResponse(FString::Printf(TEXT("File does not exist: %s"), *FilePath));
	}

	// Parse options
	bool bReplaceExisting = true;

	const TSharedPtr<FJsonObject>* OptionsPtr = nullptr;
	if (Params->TryGetObjectField(TEXT("options"), OptionsPtr) && OptionsPtr)
	{
		(*OptionsPtr)->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);
	}

	// Find a legacy factory for this file type (bypasses Interchange TaskGraph crash)
	FString Extension = FPaths::GetExtension(FilePath);
	UFactory* Factory = FindLegacyFactoryForExtension(Extension);
	if (!Factory)
	{
		return ErrorResponse(FString::Printf(
			TEXT("No legacy import factory found for file type: .%s. Supported: png, jpg, bmp, tga, hdr, exr, fbx, obj, wav, etc."),
			*Extension));
	}

	// Use UAssetImportTask with explicit factory to avoid Interchange
	UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
	ImportTask->Filename = FilePath;
	ImportTask->DestinationPath = DestinationPath;
	ImportTask->DestinationName = AssetName.IsEmpty() ? FPaths::GetBaseFilename(FilePath) : AssetName;
	ImportTask->bReplaceExisting = bReplaceExisting;
	ImportTask->bAutomated = true;
	ImportTask->bSave = false;
	ImportTask->Factory = Factory;

	TArray<UAssetImportTask*> Tasks;
	Tasks.Add(ImportTask);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.ImportAssetTasks(Tasks);

	TArray<UObject*> ImportedObjects = ImportTask->GetObjects();

	if (ImportedObjects.Num() == 0)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to import asset from: %s"), *FilePath));
	}

	// Build response with imported asset info
	TArray<TSharedPtr<FJsonValue>> ImportedArr;
	for (UObject* Obj : ImportedObjects)
	{
		if (!Obj) continue;

		TSharedPtr<FJsonObject> AssetInfo = MakeShared<FJsonObject>();
		AssetInfo->SetStringField(TEXT("name"), Obj->GetName());
		AssetInfo->SetStringField(TEXT("path"), Obj->GetOutermost()->GetName());
		AssetInfo->SetStringField(TEXT("class"), Obj->GetClass()->GetName());
		ImportedArr.Add(MakeShared<FJsonValueObject>(AssetInfo));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("source_file"), FilePath);
	Data->SetArrayField(TEXT("imported_assets"), ImportedArr);
	Data->SetNumberField(TEXT("count"), ImportedObjects.Num());
	return SuccessResponse(Data);
}

// --- Search Assets ---
TSharedPtr<FJsonObject> FMCPSearchAssetsCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Path = Params->GetStringField(TEXT("path"));
	FString TypeStr = Params->GetStringField(TEXT("type"));
	FString NamePattern = Params->GetStringField(TEXT("name_pattern"));
	bool bRecursive = true;
	Params->TryGetBoolField(TEXT("recursive"), bRecursive);
	int32 MaxResults = 100;
	if (Params->HasField(TEXT("max_results")))
	{
		MaxResults = static_cast<int32>(Params->GetNumberField(TEXT("max_results")));
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	FARFilter Filter;

	// Path filter
	if (!Path.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*Path));
		Filter.bRecursivePaths = bRecursive;
	}

	// Type filter
	if (!TypeStr.IsEmpty())
	{
		UClass* AssetClass = ResolveAssetClass(TypeStr);
		if (AssetClass)
		{
			Filter.ClassPaths.Add(AssetClass->GetClassPathName());
			Filter.bRecursiveClasses = true;
		}
		else
		{
			return ErrorResponse(FString::Printf(TEXT("Unknown asset type: %s"), *TypeStr));
		}
	}

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	// Post-filter by name pattern (case-insensitive substring match)
	if (!NamePattern.IsEmpty())
	{
		AssetList.RemoveAll([&NamePattern](const FAssetData& Asset)
		{
			return !Asset.AssetName.ToString().Contains(NamePattern, ESearchCase::IgnoreCase);
		});
	}

	// Limit results
	if (MaxResults > 0 && AssetList.Num() > MaxResults)
	{
		AssetList.SetNum(MaxResults);
	}

	// Build response
	TArray<TSharedPtr<FJsonValue>> ResultArr;
	for (const FAssetData& Asset : AssetList)
	{
		TSharedPtr<FJsonObject> AssetInfo = MakeShared<FJsonObject>();
		AssetInfo->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		AssetInfo->SetStringField(TEXT("path"), Asset.GetObjectPathString());
		AssetInfo->SetStringField(TEXT("package_path"), Asset.PackagePath.ToString());
		AssetInfo->SetStringField(TEXT("class"), Asset.AssetClassPath.GetAssetName().ToString());
		ResultArr.Add(MakeShared<FJsonValueObject>(AssetInfo));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("assets"), ResultArr);
	Data->SetNumberField(TEXT("count"), ResultArr.Num());
	Data->SetNumberField(TEXT("total_found"), AssetList.Num());
	return SuccessResponse(Data);
}

// --- Get Asset Info ---
TSharedPtr<FJsonObject> FMCPGetAssetInfoCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_path is required"));
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	FAssetData AssetData = ResolveAssetData(AssetRegistry, AssetPath);

	if (!AssetData.IsValid())
	{
		return ErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
	Data->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
	Data->SetStringField(TEXT("package_name"), AssetData.PackageName.ToString());
	Data->SetStringField(TEXT("package_path"), AssetData.PackagePath.ToString());
	Data->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());

	// Disk size
	FString PackageFilename;
	if (FPackageName::DoesPackageExist(AssetData.PackageName.ToString(), &PackageFilename))
	{
		int64 FileSize = IFileManager::Get().FileSize(*PackageFilename);
		Data->SetNumberField(TEXT("disk_size_bytes"), static_cast<double>(FileSize));
	}

	// Dirty state - check if the package is loaded and dirty
	UPackage* Package = FindPackage(nullptr, *AssetData.PackageName.ToString());
	Data->SetBoolField(TEXT("is_dirty"), Package ? Package->IsDirty() : false);
	Data->SetBoolField(TEXT("is_loaded"), Package != nullptr);

	// References (assets that reference this one)
	TArray<FName> Referencers;
	AssetRegistry.GetReferencers(AssetData.PackageName, Referencers);
	TArray<TSharedPtr<FJsonValue>> ReferencerArr;
	for (const FName& Ref : Referencers)
	{
		ReferencerArr.Add(MakeShared<FJsonValueString>(Ref.ToString()));
	}
	Data->SetArrayField(TEXT("referencers"), ReferencerArr);
	Data->SetNumberField(TEXT("referencer_count"), ReferencerArr.Num());

	// Dependencies (assets this one depends on)
	TArray<FName> Dependencies;
	AssetRegistry.GetDependencies(AssetData.PackageName, Dependencies);
	TArray<TSharedPtr<FJsonValue>> DependencyArr;
	for (const FName& Dep : Dependencies)
	{
		DependencyArr.Add(MakeShared<FJsonValueString>(Dep.ToString()));
	}
	Data->SetArrayField(TEXT("dependencies"), DependencyArr);
	Data->SetNumberField(TEXT("dependency_count"), DependencyArr.Num());

	return SuccessResponse(Data);
}

// --- Delete Asset ---
TSharedPtr<FJsonObject> FMCPDeleteAssetCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_path is required"));
	}

	bool bForce = false;
	Params->TryGetBoolField(TEXT("force"), bForce);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	// Verify asset exists
	FAssetData AssetData = ResolveAssetData(AssetRegistry, AssetPath);
	if (!AssetData.IsValid())
	{
		return ErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
	}

	// Check for references unless force is true
	if (!bForce)
	{
		TArray<FName> Referencers;
		AssetRegistry.GetReferencers(AssetData.PackageName, Referencers);

		// Filter out self-references
		Referencers.RemoveAll([&AssetData](const FName& Ref)
		{
			return Ref == AssetData.PackageName;
		});

		if (Referencers.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> RefArr;
			for (const FName& Ref : Referencers)
			{
				RefArr.Add(MakeShared<FJsonValueString>(Ref.ToString()));
			}

			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetBoolField(TEXT("has_referencers"), true);
			Data->SetArrayField(TEXT("referencers"), RefArr);
			Data->SetNumberField(TEXT("referencer_count"), RefArr.Num());
			Data->SetStringField(TEXT("message"),
				FString::Printf(TEXT("Asset '%s' is referenced by %d other asset(s). Use force=true to delete anyway."),
					*AssetData.AssetName.ToString(), RefArr.Num()));

			// Return success=false but with data about references
			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), false);
			Response->SetStringField(TEXT("error"),
				FString::Printf(TEXT("Asset has %d referencer(s). Use force=true to delete."), RefArr.Num()));
			Response->SetObjectField(TEXT("data"), Data);
			return Response;
		}
	}

	// Delete the asset using the package path
	FString PackagePath = AssetData.PackageName.ToString();
	bool bDeleted = UEditorAssetLibrary::DeleteAsset(PackagePath);

	if (!bDeleted)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to delete asset: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("deleted_asset"), AssetData.AssetName.ToString());
	Data->SetStringField(TEXT("deleted_path"), PackagePath);
	Data->SetBoolField(TEXT("forced"), bForce);
	return SuccessResponse(Data);
}

// --- Rename Asset ---
TSharedPtr<FJsonObject> FMCPRenameAssetCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString NewPath = Params->GetStringField(TEXT("new_path"));

	if (AssetPath.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_path is required"));
	}
	if (NewPath.IsEmpty())
	{
		return ErrorResponse(TEXT("new_path is required"));
	}

	// Verify source asset exists
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	FAssetData AssetData = ResolveAssetData(AssetRegistry, AssetPath);
	if (!AssetData.IsValid())
	{
		return ErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
	}

	FString SourcePath = AssetData.PackageName.ToString();
	bool bRenamed = UEditorAssetLibrary::RenameAsset(SourcePath, NewPath);

	if (!bRenamed)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to rename asset from '%s' to '%s'"), *SourcePath, *NewPath));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("old_path"), SourcePath);
	Data->SetStringField(TEXT("new_path"), NewPath);
	Data->SetStringField(TEXT("old_name"), AssetData.AssetName.ToString());
	Data->SetStringField(TEXT("new_name"), FPackageName::GetShortName(NewPath));
	return SuccessResponse(Data);
}
