#include "Commands/MCPPIECommands.h"
#include "Editor.h"
#include "Engine/World.h"
#include "LevelEditor.h"
#include "ILevelEditor.h"
#include "SLevelViewport.h"
#include "GameFramework/PlayerController.h"

// --- Start PIE ---
TSharedPtr<FJsonObject> FMCPStartPIECommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return ErrorResponse(TEXT("GEditor not available"));
	}

	if (GEditor->PlayWorld)
	{
		return ErrorResponse(TEXT("A Play-in-Editor session is already running. Stop it first with stop_pie."));
	}

	FString Mode = Params->GetStringField(TEXT("mode"));

	FRequestPlaySessionParams SessionParams;

	if (Mode == TEXT("simulate"))
	{
		SessionParams.WorldType = EPlaySessionWorldType::SimulateInEditor;
	}
	else if (Mode == TEXT("new_window"))
	{
		// Leave DestinationSlateViewport unset — engine opens a new window
	}
	else if (Mode == TEXT("viewport") || Mode.IsEmpty())
	{
		// Use the first active viewport for in-viewport PIE
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedPtr<SLevelViewport> ActiveViewport = LevelEditorModule.GetFirstActiveLevelViewport();
		if (ActiveViewport.IsValid())
		{
			SessionParams.DestinationSlateViewport = ActiveViewport;
		}
	}

	GEditor->RequestPlaySession(SessionParams);
	GEditor->StartQueuedPlaySessionRequest();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	bool bIsRunning = GEditor->PlayWorld != nullptr;
	Data->SetBoolField(TEXT("is_running"), bIsRunning);
	Data->SetBoolField(TEXT("is_simulating"), GEditor->bIsSimulatingInEditor);
	Data->SetStringField(TEXT("mode"), Mode.IsEmpty() ? TEXT("viewport") : Mode);

	if (bIsRunning)
	{
		Data->SetStringField(TEXT("world_name"), GEditor->PlayWorld->GetName());
	}

	return SuccessResponse(Data);
}

// --- Stop PIE ---
TSharedPtr<FJsonObject> FMCPStopPIECommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return ErrorResponse(TEXT("GEditor not available"));
	}

	if (!GEditor->PlayWorld)
	{
		return ErrorResponse(TEXT("No Play-in-Editor session is running"));
	}

	GEditor->RequestEndPlayMap();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("status"), TEXT("stop_requested"));
	return SuccessResponse(Data);
}

// --- Get PIE Status ---
TSharedPtr<FJsonObject> FMCPGetPIEStatusCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return ErrorResponse(TEXT("GEditor not available"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	bool bIsRunning = GEditor->PlayWorld != nullptr;
	Data->SetBoolField(TEXT("is_running"), bIsRunning);
	Data->SetBoolField(TEXT("is_simulating"), GEditor->bIsSimulatingInEditor);

	if (bIsRunning && GEditor->PlayWorld)
	{
		Data->SetBoolField(TEXT("is_paused"), GEditor->PlayWorld->IsPaused());
		Data->SetStringField(TEXT("world_name"), GEditor->PlayWorld->GetName());

		// Count player controllers
		int32 PlayerCount = 0;
		for (FConstPlayerControllerIterator It = GEditor->PlayWorld->GetPlayerControllerIterator(); It; ++It)
		{
			++PlayerCount;
		}
		Data->SetNumberField(TEXT("player_count"), PlayerCount);
	}
	else
	{
		Data->SetBoolField(TEXT("is_paused"), false);
	}

	return SuccessResponse(Data);
}

// --- Set PIE Paused ---
TSharedPtr<FJsonObject> FMCPSetPIEPausedCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return ErrorResponse(TEXT("GEditor not available"));
	}

	if (!GEditor->PlayWorld)
	{
		return ErrorResponse(TEXT("No Play-in-Editor session is running"));
	}

	bool bPaused = false;
	if (!Params->TryGetBoolField(TEXT("paused"), bPaused))
	{
		return ErrorResponse(TEXT("'paused' parameter (bool) is required"));
	}

	GEditor->SetPIEWorldsPaused(bPaused);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("is_paused"), GEditor->PlayWorld->IsPaused());
	Data->SetBoolField(TEXT("is_running"), true);
	return SuccessResponse(Data);
}
