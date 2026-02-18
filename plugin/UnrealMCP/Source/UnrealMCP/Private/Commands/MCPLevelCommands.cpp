#include "Commands/MCPLevelCommands.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "EditorLevelUtils.h"
#include "FileHelpers.h"
#include "ScopedTransaction.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace LevelCommandsLocal
{

// Helper: save a world package directly without any UI dialogs
static bool SaveWorldPackage(UWorld* World, const FString& PackagePath)
{
	if (!World) return false;

	UPackage* Package = World->GetOutermost();
	FString CurrentName = Package->GetName();

	// If a different path is requested, rename the package first
	if (!PackagePath.IsEmpty() && CurrentName != PackagePath)
	{
		Package->Rename(*PackagePath, nullptr, REN_DontCreateRedirectors | REN_NonTransactional);
	}

	FString FinalPath = Package->GetName();
	FString PackageFilename = FPackageName::LongPackageNameToFilename(
		FinalPath, FPackageName::GetMapPackageExtension());

	// Ensure the directory exists
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);

	Package->MarkPackageDirty();

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	return UPackage::SavePackage(Package, World, *PackageFilename, SaveArgs);
}

// Helper: clean up orphan worlds before LoadMap to prevent "World Memory Leaks"
// fatal error. Finds ALL worlds not actively used by the editor or its streaming
// levels, destroys them and their packages, then runs GC.
static void CleanUpOrphanWorlds()
{
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!EditorWorld) return;

	// Build set of worlds actively in use
	TSet<UWorld*> ActiveWorlds;
	ActiveWorlds.Add(EditorWorld);
	for (ULevelStreaming* SL : EditorWorld->GetStreamingLevels())
	{
		if (SL)
		{
			ULevel* LoadedLevel = SL->GetLoadedLevel();
			if (LoadedLevel)
			{
				UWorld* LW = Cast<UWorld>(LoadedLevel->GetOuter());
				if (LW) ActiveWorlds.Add(LW);
			}
		}
	}

	// Find orphan worlds (skip Game/PIE/Preview worlds that belong to the engine)
	TArray<UWorld*> OrphanWorlds;
	for (TObjectIterator<UWorld> It; It; ++It)
	{
		UWorld* W = *It;
		if (!W || ActiveWorlds.Contains(W)) continue;
		if (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE ||
			W->WorldType == EWorldType::EditorPreview || W->WorldType == EWorldType::GamePreview)
			continue;

		OrphanWorlds.Add(W);
	}

	// Destroy orphan worlds and mark their packages for GC
	for (UWorld* W : OrphanWorlds)
	{
		UPackage* Pkg = W->GetOutermost();
		W->DestroyWorld(false);
		W->ClearFlags(RF_Standalone | RF_Public);
		W->MarkAsGarbage();
		if (Pkg)
		{
			Pkg->ClearFlags(RF_Standalone | RF_Public);
			Pkg->MarkAsGarbage();
		}
	}

	if (OrphanWorlds.Num() > 0)
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
}

// Helper to find a streaming level by package name (full path or short name)
static ULevelStreaming* FindStreamingLevelByName(UWorld* World, const FString& PackageName)
{
	if (!World) return nullptr;

	for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (!StreamingLevel) continue;

		FString LevelPackageName = StreamingLevel->GetWorldAssetPackageName();
		if (LevelPackageName == PackageName ||
			LevelPackageName.EndsWith(TEXT("/") + PackageName) ||
			FPackageName::GetShortName(LevelPackageName) == PackageName)
		{
			return StreamingLevel;
		}
	}
	return nullptr;
}

} // namespace LevelCommandsLocal

using namespace LevelCommandsLocal;

// --- Get Level Info ---
TSharedPtr<FJsonObject> FMCPGetLevelInfoCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return ErrorResponse(TEXT("No editor world available"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("world_name"), World->GetMapName());
	Data->SetStringField(TEXT("map_path"), World->GetOutermost()->GetName());

	// Persistent level info
	ULevel* PersistentLevel = World->PersistentLevel;
	if (PersistentLevel)
	{
		Data->SetStringField(TEXT("persistent_level"), PersistentLevel->GetOutermost()->GetName());
		Data->SetNumberField(TEXT("persistent_actor_count"), PersistentLevel->Actors.Num());
	}

	// Current level
	ULevel* CurrentLevel = World->GetCurrentLevel();
	if (CurrentLevel)
	{
		Data->SetStringField(TEXT("current_level"), CurrentLevel->GetOutermost()->GetName());
	}

	// Streaming sub-levels
	TArray<TSharedPtr<FJsonValue>> StreamingArr;
	for (ULevelStreaming* SL : World->GetStreamingLevels())
	{
		if (!SL) continue;

		TSharedPtr<FJsonObject> LevelInfo = MakeShared<FJsonObject>();
		LevelInfo->SetStringField(TEXT("package_name"), SL->GetWorldAssetPackageName());
		LevelInfo->SetStringField(TEXT("class"), SL->GetClass()->GetName());

		bool bIsVisible = SL->GetShouldBeVisibleInEditor();
		LevelInfo->SetBoolField(TEXT("visible"), bIsVisible);

		bool bIsLoaded = SL->GetLoadedLevel() != nullptr;
		LevelInfo->SetBoolField(TEXT("loaded"), bIsLoaded);

		bool bIsCurrent = CurrentLevel && SL->GetLoadedLevel() == CurrentLevel;
		LevelInfo->SetBoolField(TEXT("is_current"), bIsCurrent);

		if (bIsLoaded)
		{
			LevelInfo->SetNumberField(TEXT("actor_count"), SL->GetLoadedLevel()->Actors.Num());
		}

		// Transform
		FTransform LevelTransform = SL->LevelTransform;
		FVector Location = LevelTransform.GetLocation();
		FRotator Rotation = LevelTransform.Rotator();

		TArray<TSharedPtr<FJsonValue>> LocArr = {
			MakeShared<FJsonValueNumber>(Location.X),
			MakeShared<FJsonValueNumber>(Location.Y),
			MakeShared<FJsonValueNumber>(Location.Z)
		};
		LevelInfo->SetArrayField(TEXT("location"), LocArr);

		TArray<TSharedPtr<FJsonValue>> RotArr = {
			MakeShared<FJsonValueNumber>(Rotation.Pitch),
			MakeShared<FJsonValueNumber>(Rotation.Yaw),
			MakeShared<FJsonValueNumber>(Rotation.Roll)
		};
		LevelInfo->SetArrayField(TEXT("rotation"), RotArr);

		StreamingArr.Add(MakeShared<FJsonValueObject>(LevelInfo));
	}

	Data->SetArrayField(TEXT("streaming_levels"), StreamingArr);
	Data->SetNumberField(TEXT("streaming_level_count"), StreamingArr.Num());

	return SuccessResponse(Data);
}

// --- Create Level ---
TSharedPtr<FJsonObject> FMCPCreateLevelCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString TemplatePath = Params->GetStringField(TEXT("template_path"));
	FString SavePath = Params->GetStringField(TEXT("save_path"));
	bool bSaveExisting = true;
	Params->TryGetBoolField(TEXT("save_existing"), bSaveExisting);

	UWorld* NewWorld = nullptr;

	// Create blank or from template (this becomes the active editor world)
	if (TemplatePath.IsEmpty())
	{
		NewWorld = UEditorLoadingAndSavingUtils::NewBlankMap(bSaveExisting);
	}
	else
	{
		NewWorld = UEditorLoadingAndSavingUtils::NewMapFromTemplate(TemplatePath, bSaveExisting);
	}

	if (!NewWorld)
	{
		NewWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	if (!NewWorld)
	{
		return ErrorResponse(TEXT("Failed to create level"));
	}

	// If save_path given: save the file to the desired location, then reload
	// so the editor world gets the correct package name
	bool bSaved = false;
	if (!SavePath.IsEmpty())
	{
		// Save the new world's data to the desired file path (no package rename)
		UPackage* Package = NewWorld->GetOutermost();
		FString Filename = FPackageName::LongPackageNameToFilename(
			SavePath, FPackageName::GetMapPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		Package->MarkPackageDirty();

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		bSaved = UPackage::SavePackage(Package, NewWorld, *Filename, SaveArgs);

		if (bSaved)
		{
			// Notify asset registry so the Content Browser sees the new file
			IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
			FString MapDir = FPaths::GetPath(SavePath);
			AssetRegistry.ScanPathsSynchronous({MapDir}, true);

			// Clean up any orphan worlds before LoadMap to prevent memory leak crash
			CleanUpOrphanWorlds();

			// Reload from the saved path so the editor gets the correct package name
			NewWorld = UEditorLoadingAndSavingUtils::LoadMap(SavePath);
			if (!NewWorld)
			{
				NewWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
			}
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	if (NewWorld)
	{
		Data->SetStringField(TEXT("world_name"), NewWorld->GetMapName());
		Data->SetStringField(TEXT("map_path"), NewWorld->GetOutermost()->GetName());
	}
	if (!SavePath.IsEmpty())
	{
		Data->SetStringField(TEXT("save_path"), SavePath);
		Data->SetBoolField(TEXT("saved"), bSaved);
	}
	return SuccessResponse(Data);
}

// --- Save Level ---
TSharedPtr<FJsonObject> FMCPSaveLevelCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	bool bSaveAll = false;
	Params->TryGetBoolField(TEXT("save_all"), bSaveAll);
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	if (bSaveAll)
	{
		// Save all dirty map and content packages without prompting
		bool bResult = FEditorFileUtils::SaveDirtyPackages(
			/*bPromptUserToSave=*/ false,
			/*bSaveMapPackages=*/ true,
			/*bSaveContentPackages=*/ true
		);

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetBoolField(TEXT("saved_all"), true);
		Data->SetBoolField(TEXT("result"), bResult);
		return SuccessResponse(Data);
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return ErrorResponse(TEXT("No editor world available"));
	}

	if (!AssetPath.IsEmpty())
	{
		// Save As: save world data to new file path, then reload with correct name
		UPackage* Package = World->GetOutermost();
		FString Filename = FPackageName::LongPackageNameToFilename(
			AssetPath, FPackageName::GetMapPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		Package->MarkPackageDirty();

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		bool bResult = UPackage::SavePackage(Package, World, *Filename, SaveArgs);
		if (!bResult)
		{
			return ErrorResponse(FString::Printf(TEXT("Failed to save map to: %s"), *AssetPath));
		}

		// Notify asset registry so the Content Browser sees the new file
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		FString MapDir = FPaths::GetPath(AssetPath);
		AssetRegistry.ScanPathsSynchronous({MapDir}, true);

		// Clean up any orphan worlds before LoadMap to prevent memory leak crash
		CleanUpOrphanWorlds();

		// Reload so the editor picks up the correct package name
		UWorld* ReloadedWorld = UEditorLoadingAndSavingUtils::LoadMap(AssetPath);
		if (!ReloadedWorld)
		{
			ReloadedWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		}

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("asset_path"), AssetPath);
		if (ReloadedWorld)
		{
			Data->SetStringField(TEXT("world_name"), ReloadedWorld->GetMapName());
		}
		return SuccessResponse(Data);
	}

	// Default: save current level to its existing path (no UI dialogs)
	FString PackageName = World->GetOutermost()->GetName();
	if (PackageName.StartsWith(TEXT("/Temp/")))
	{
		return ErrorResponse(TEXT("Level has never been saved. Provide asset_path to specify where to save."));
	}

	bool bResult = SaveWorldPackage(World, TEXT(""));
	if (!bResult)
	{
		return ErrorResponse(TEXT("Failed to save current level"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("saved"), true);
	Data->SetStringField(TEXT("world_name"), World->GetMapName());
	Data->SetStringField(TEXT("map_path"), World->GetOutermost()->GetName());
	return SuccessResponse(Data);
}

// --- Load Level ---
TSharedPtr<FJsonObject> FMCPLoadLevelCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString MapPath = Params->GetStringField(TEXT("map_path"));
	if (MapPath.IsEmpty())
	{
		return ErrorResponse(TEXT("map_path is required"));
	}

	bool bSaveExisting = true;
	Params->TryGetBoolField(TEXT("save_existing"), bSaveExisting);

	// Save existing map if requested
	if (bSaveExisting)
	{
		// Use SaveDirtyPackages to safely save without renaming or dialogs
		FEditorFileUtils::SaveDirtyPackages(
			/*bPromptUserToSave=*/ false,
			/*bSaveMapPackages=*/ true,
			/*bSaveContentPackages=*/ false
		);
	}

	// Clean up any orphan worlds before LoadMap to prevent memory leak crash
	CleanUpOrphanWorlds();

	UWorld* LoadedWorld = UEditorLoadingAndSavingUtils::LoadMap(MapPath);
	if (!LoadedWorld)
	{
		// Fallback: check if the world was loaded as the current editor world
		LoadedWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	if (!LoadedWorld)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to load map: %s"), *MapPath));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("world_name"), LoadedWorld->GetMapName());
	Data->SetStringField(TEXT("map_path"), LoadedWorld->GetOutermost()->GetName());

	if (LoadedWorld->PersistentLevel)
	{
		Data->SetNumberField(TEXT("actor_count"), LoadedWorld->PersistentLevel->Actors.Num());
	}

	// Include streaming level count
	Data->SetNumberField(TEXT("streaming_level_count"), LoadedWorld->GetStreamingLevels().Num());

	return SuccessResponse(Data);
}

// --- Add Streaming Level ---
TSharedPtr<FJsonObject> FMCPAddStreamingLevelCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return ErrorResponse(TEXT("No editor world available"));
	}

	FString PackageName = Params->GetStringField(TEXT("package_name"));
	if (PackageName.IsEmpty())
	{
		return ErrorResponse(TEXT("package_name is required"));
	}

	FString StreamingClassStr = Params->GetStringField(TEXT("streaming_class"));
	bool bCreateNew = false;
	Params->TryGetBoolField(TEXT("create_new"), bCreateNew);

	// Determine streaming level class
	TSubclassOf<ULevelStreaming> StreamingClass = ULevelStreamingDynamic::StaticClass();
	if (StreamingClassStr.Equals(TEXT("AlwaysLoaded"), ESearchCase::IgnoreCase))
	{
		StreamingClass = ULevelStreamingAlwaysLoaded::StaticClass();
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Add Streaming Level")));

	ULevelStreaming* NewStreaming = nullptr;
	if (bCreateNew)
	{
		// Manually create and save a new empty level (no UI dialogs)
		FString MapName = FPackageName::GetShortName(PackageName);

		UPackage* LevelPackage = CreatePackage(*PackageName);
		if (!LevelPackage)
		{
			return ErrorResponse(FString::Printf(TEXT("Failed to create package: %s"), *PackageName));
		}

		UWorld* SubWorld = UWorld::CreateWorld(EWorldType::None, false, *MapName, LevelPackage);
		if (!SubWorld)
		{
			return ErrorResponse(TEXT("Failed to create sub-level world"));
		}

		SubWorld->SetFlags(RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(SubWorld);
		LevelPackage->MarkPackageDirty();

		// Save the new level to disk
		FString Filename = FPackageName::LongPackageNameToFilename(
			PackageName, FPackageName::GetMapPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		bool bSaved = UPackage::SavePackage(LevelPackage, SubWorld, *Filename, SaveArgs);
		if (!bSaved)
		{
			return ErrorResponse(FString::Printf(TEXT("Failed to save new level: %s"), *PackageName));
		}

		// Let AddLevelToWorld use the in-memory package directly.
		// Orphan cleanup happens later in load_level/create_level before LoadMap.
		NewStreaming = UEditorLevelUtils::AddLevelToWorld(
			World,
			*PackageName,
			StreamingClass
		);
	}
	else
	{
		NewStreaming = UEditorLevelUtils::AddLevelToWorld(
			World,
			*PackageName,
			StreamingClass
		);
	}

	if (!NewStreaming)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to add streaming level: %s"), *PackageName));
	}

	// Apply optional location
	const TArray<TSharedPtr<FJsonValue>>* LocationArr = nullptr;
	if (Params->TryGetArrayField(TEXT("location"), LocationArr) && LocationArr->Num() >= 3)
	{
		FVector Location(
			(*LocationArr)[0]->AsNumber(),
			(*LocationArr)[1]->AsNumber(),
			(*LocationArr)[2]->AsNumber()
		);
		FTransform LevelTransform = NewStreaming->LevelTransform;
		LevelTransform.SetLocation(Location);
		NewStreaming->LevelTransform = LevelTransform;
	}

	// Apply optional rotation
	const TArray<TSharedPtr<FJsonValue>>* RotationArr = nullptr;
	if (Params->TryGetArrayField(TEXT("rotation"), RotationArr) && RotationArr->Num() >= 3)
	{
		FRotator Rotation(
			(*RotationArr)[0]->AsNumber(),
			(*RotationArr)[1]->AsNumber(),
			(*RotationArr)[2]->AsNumber()
		);
		FTransform LevelTransform = NewStreaming->LevelTransform;
		LevelTransform.SetRotation(Rotation.Quaternion());
		NewStreaming->LevelTransform = LevelTransform;
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("package_name"), NewStreaming->GetWorldAssetPackageName());
	Data->SetStringField(TEXT("class"), NewStreaming->GetClass()->GetName());
	Data->SetBoolField(TEXT("created_new"), bCreateNew);
	return SuccessResponse(Data);
}

// --- Remove Streaming Level ---
TSharedPtr<FJsonObject> FMCPRemoveStreamingLevelCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return ErrorResponse(TEXT("No editor world available"));
	}

	FString PackageName = Params->GetStringField(TEXT("package_name"));
	if (PackageName.IsEmpty())
	{
		return ErrorResponse(TEXT("package_name is required"));
	}

	ULevelStreaming* StreamingLevel = FindStreamingLevelByName(World, PackageName);
	if (!StreamingLevel)
	{
		return ErrorResponse(FString::Printf(TEXT("Streaming level not found: %s"), *PackageName));
	}

	ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel();
	if (!LoadedLevel)
	{
		return ErrorResponse(FString::Printf(TEXT("Streaming level is not loaded: %s"), *PackageName));
	}

	FString ActualPackageName = StreamingLevel->GetWorldAssetPackageName();

	// If this level is the current editing level, switch to persistent first
	if (World->GetCurrentLevel() == LoadedLevel)
	{
		UEditorLevelUtils::MakeLevelCurrent(World->PersistentLevel);
	}

	// Fully detach and unload the streaming level before removing
	StreamingLevel->SetShouldBeVisibleInEditor(false);
	StreamingLevel->SetShouldBeVisible(false);
	StreamingLevel->SetShouldBeLoaded(false);

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Remove Streaming Level")));
	World->Modify();

	// Remove the streaming level reference from the world
	// (avoids RemoveLevelFromWorld/RemoveLevelsFromWorld which crash on rooted levels)
	World->RemoveStreamingLevel(StreamingLevel);
	World->RefreshStreamingLevels();
	World->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("package_name"), ActualPackageName);
	Data->SetBoolField(TEXT("removed"), true);
	return SuccessResponse(Data);
}

// --- Set Level Visibility ---
TSharedPtr<FJsonObject> FMCPSetLevelVisibilityCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return ErrorResponse(TEXT("No editor world available"));
	}

	FString PackageName = Params->GetStringField(TEXT("package_name"));
	if (PackageName.IsEmpty())
	{
		return ErrorResponse(TEXT("package_name is required"));
	}

	bool bVisible = true;
	Params->TryGetBoolField(TEXT("visible"), bVisible);

	bool bMakeCurrent = false;
	Params->TryGetBoolField(TEXT("make_current"), bMakeCurrent);

	ULevelStreaming* StreamingLevel = FindStreamingLevelByName(World, PackageName);
	if (!StreamingLevel)
	{
		return ErrorResponse(FString::Printf(TEXT("Streaming level not found: %s"), *PackageName));
	}

	ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel();
	if (!LoadedLevel)
	{
		return ErrorResponse(FString::Printf(TEXT("Streaming level is not loaded: %s"), *PackageName));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Set Level Visibility")));

	UEditorLevelUtils::SetLevelVisibility(LoadedLevel, bVisible, true);

	if (bMakeCurrent && bVisible)
	{
		UEditorLevelUtils::MakeLevelCurrent(LoadedLevel);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("package_name"), StreamingLevel->GetWorldAssetPackageName());
	Data->SetBoolField(TEXT("visible"), bVisible);
	Data->SetBoolField(TEXT("is_current"), World->GetCurrentLevel() == LoadedLevel);
	return SuccessResponse(Data);
}
