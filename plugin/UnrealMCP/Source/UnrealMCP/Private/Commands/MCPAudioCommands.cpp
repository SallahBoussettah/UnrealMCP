#include "Commands/MCPAudioCommands.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNode.h"
#include "Sound/SoundNodeModulator.h"
#include "Sound/SoundNodeRandom.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundNodeAttenuation.h"
#include "Factories/SoundCueFactoryNew.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EditorAssetLibrary.h"
#include "ScopedTransaction.h"

// --- Create Sound Cue ---
TSharedPtr<FJsonObject> FMCPCreateSoundCueCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Name = Params->GetStringField(TEXT("name"));
	FString Path = Params->GetStringField(TEXT("path"));

	if (Name.IsEmpty()) return ErrorResponse(TEXT("'name' is required (e.g. 'SC_Footstep_Wood')"));
	if (Path.IsEmpty()) return ErrorResponse(TEXT("'path' is required (e.g. '/Game/Audio/Footsteps')"));

	// Get wave paths array
	const TArray<TSharedPtr<FJsonValue>>* WavePathsArr = nullptr;
	if (!Params->TryGetArrayField(TEXT("wave_paths"), WavePathsArr) || !WavePathsArr || WavePathsArr->Num() == 0)
	{
		return ErrorResponse(TEXT("'wave_paths' array is required with at least one SoundWave path"));
	}

	// Load all SoundWave assets
	TArray<TWeakObjectPtr<USoundWave>> Waves;
	for (const auto& WaveVal : *WavePathsArr)
	{
		FString WavePath = WaveVal->AsString();
		USoundWave* Wave = LoadObject<USoundWave>(nullptr, *WavePath);
		if (!Wave)
		{
			// Try appending asset name
			FString AssetName = FPaths::GetCleanFilename(WavePath);
			FString FullPath = WavePath + TEXT(".") + AssetName;
			Wave = LoadObject<USoundWave>(nullptr, *FullPath);
		}
		if (!Wave)
		{
			return ErrorResponse(FString::Printf(TEXT("Could not load SoundWave: %s"), *WavePath));
		}
		Waves.Add(Wave);
	}

	// Check if asset already exists
	FString FullAssetPath = Path / Name;
	USoundCue* ExistingCue = LoadObject<USoundCue>(nullptr, *(FullAssetPath + TEXT(".") + Name));
	if (ExistingCue)
	{
		return ErrorResponse(FString::Printf(TEXT("Sound Cue already exists: %s. Delete it first or use a different name."), *FullAssetPath));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Create Sound Cue")));

	// Create using factory with initial waves (this properly builds the Random -> WavePlayer graph)
	USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>();
	Factory->InitialSoundWaves = Waves;

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(Name, Path, USoundCue::StaticClass(), Factory);

	USoundCue* SoundCue = Cast<USoundCue>(NewAsset);
	if (!SoundCue)
	{
		return ErrorResponse(TEXT("Failed to create Sound Cue asset"));
	}

	// Set Random node properties if present
	bool bRandomize = true;
	Params->TryGetBoolField(TEXT("randomize_without_replacement"), bRandomize);

	// Find the Random node created by the factory
	USoundNodeRandom* RandomNode = nullptr;
	for (USoundNode* Node : SoundCue->AllNodes)
	{
		RandomNode = Cast<USoundNodeRandom>(Node);
		if (RandomNode) break;
	}
	if (RandomNode)
	{
		RandomNode->bRandomizeWithoutReplacement = bRandomize;
	}

	// Optionally add a Modulator node (pitch/volume variation)
	double PitchMin = 1.0, PitchMax = 1.0, VolumeMin = 1.0, VolumeMax = 1.0;
	bool bHasModulator = false;
	if (Params->TryGetNumberField(TEXT("pitch_min"), PitchMin)) bHasModulator = true;
	if (Params->TryGetNumberField(TEXT("pitch_max"), PitchMax)) bHasModulator = true;
	if (Params->TryGetNumberField(TEXT("volume_min"), VolumeMin)) bHasModulator = true;
	if (Params->TryGetNumberField(TEXT("volume_max"), VolumeMax)) bHasModulator = true;

	if (bHasModulator)
	{
		USoundNodeModulator* Modulator = NewObject<USoundNodeModulator>(SoundCue);
		Modulator->PitchMin = static_cast<float>(PitchMin);
		Modulator->PitchMax = static_cast<float>(PitchMax);
		Modulator->VolumeMin = static_cast<float>(VolumeMin);
		Modulator->VolumeMax = static_cast<float>(VolumeMax);

		// Insert Modulator between Output and the existing first node (Random)
		USoundNode* ExistingFirst = SoundCue->FirstNode;
		if (ExistingFirst)
		{
			Modulator->ChildNodes.Add(ExistingFirst);
		}
		SoundCue->FirstNode = Modulator;
		SoundCue->AllNodes.Add(Modulator);
	}

	// Save the asset
	UEditorAssetLibrary::SaveAsset(FullAssetPath, false);

	// Build response
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), FullAssetPath);
	Data->SetStringField(TEXT("name"), Name);
	Data->SetNumberField(TEXT("wave_count"), Waves.Num());
	Data->SetNumberField(TEXT("total_nodes"), SoundCue->AllNodes.Num());
	Data->SetBoolField(TEXT("has_modulator"), bHasModulator);
	Data->SetBoolField(TEXT("has_random"), RandomNode != nullptr);

	return SuccessResponse(Data);
}

// --- Get Sound Cue Info ---
TSharedPtr<FJsonObject> FMCPGetSoundCueInfoCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	if (AssetPath.IsEmpty()) return ErrorResponse(TEXT("'asset_path' is required"));

	USoundCue* SoundCue = LoadObject<USoundCue>(nullptr, *AssetPath);
	if (!SoundCue)
	{
		// Try appending asset name
		FString AssetName = FPaths::GetCleanFilename(AssetPath);
		FString FullPath = AssetPath + TEXT(".") + AssetName;
		SoundCue = LoadObject<USoundCue>(nullptr, *FullPath);
	}
	if (!SoundCue)
	{
		return ErrorResponse(FString::Printf(TEXT("Sound Cue not found: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), SoundCue->GetName());
	Data->SetStringField(TEXT("asset_path"), SoundCue->GetPathName());
	Data->SetNumberField(TEXT("total_nodes"), SoundCue->AllNodes.Num());
	Data->SetNumberField(TEXT("duration"), SoundCue->Duration);
	Data->SetNumberField(TEXT("max_distance"), SoundCue->GetMaxDistance());

	// First node info
	if (SoundCue->FirstNode)
	{
		Data->SetStringField(TEXT("first_node_type"), SoundCue->FirstNode->GetClass()->GetName());
	}
	else
	{
		Data->SetStringField(TEXT("first_node_type"), TEXT("None"));
	}

	// All nodes info
	TArray<TSharedPtr<FJsonValue>> NodesArr;
	for (USoundNode* Node : SoundCue->AllNodes)
	{
		TSharedPtr<FJsonObject> NodeInfo = MakeShared<FJsonObject>();
		NodeInfo->SetStringField(TEXT("type"), Node->GetClass()->GetName());
		NodeInfo->SetStringField(TEXT("name"), Node->GetName());
		NodeInfo->SetNumberField(TEXT("child_count"), Node->ChildNodes.Num());

		// Type-specific info
		if (USoundNodeWavePlayer* WavePlayer = Cast<USoundNodeWavePlayer>(Node))
		{
			USoundWave* Wave = WavePlayer->GetSoundWave();
			if (Wave)
			{
				NodeInfo->SetStringField(TEXT("sound_wave"), Wave->GetPathName());
				NodeInfo->SetStringField(TEXT("sound_wave_name"), Wave->GetName());
			}
		}
		else if (USoundNodeModulator* Modulator = Cast<USoundNodeModulator>(Node))
		{
			NodeInfo->SetNumberField(TEXT("pitch_min"), Modulator->PitchMin);
			NodeInfo->SetNumberField(TEXT("pitch_max"), Modulator->PitchMax);
			NodeInfo->SetNumberField(TEXT("volume_min"), Modulator->VolumeMin);
			NodeInfo->SetNumberField(TEXT("volume_max"), Modulator->VolumeMax);
		}
		else if (USoundNodeRandom* RandomNode = Cast<USoundNodeRandom>(Node))
		{
			NodeInfo->SetBoolField(TEXT("randomize_without_replacement"), RandomNode->bRandomizeWithoutReplacement);
		}

		NodesArr.Add(MakeShared<FJsonValueObject>(NodeInfo));
	}
	Data->SetArrayField(TEXT("nodes"), NodesArr);

	return SuccessResponse(Data);
}
