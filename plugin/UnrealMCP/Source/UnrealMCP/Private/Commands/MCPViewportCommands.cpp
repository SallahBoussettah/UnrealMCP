#include "Commands/MCPViewportCommands.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "EditorViewportClient.h"
#include "Misc/Base64.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"

// --- Take Screenshot ---
TSharedPtr<FJsonObject> FMCPTakeScreenshotCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	int32 Width = 1280;
	int32 Height = 720;
	Params->TryGetNumberField(TEXT("width"), Width);
	Params->TryGetNumberField(TEXT("height"), Height);

	if (!GEditor || !GEditor->GetActiveViewport())
	{
		return ErrorResponse(TEXT("No active viewport available"));
	}

	FViewport* Viewport = GEditor->GetActiveViewport();
	if (!Viewport)
	{
		return ErrorResponse(TEXT("Failed to get active viewport"));
	}

	TArray<FColor> Bitmap;
	int32 ViewportWidth = Viewport->GetSizeXY().X;
	int32 ViewportHeight = Viewport->GetSizeXY().Y;

	if (!Viewport->ReadPixels(Bitmap))
	{
		return ErrorResponse(TEXT("Failed to read viewport pixels"));
	}

	// Encode as PNG using ImageWrapper
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	if (!ImageWrapper.IsValid())
	{
		return ErrorResponse(TEXT("Failed to create image wrapper"));
	}

	TArray<uint8> RawData;
	RawData.SetNum(Bitmap.Num() * 4);
	FMemory::Memcpy(RawData.GetData(), Bitmap.GetData(), RawData.Num());

	if (!ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), ViewportWidth, ViewportHeight, ERGBFormat::BGRA, 8))
	{
		return ErrorResponse(TEXT("Failed to set raw image data"));
	}

	TArray64<uint8> PNGData = ImageWrapper->GetCompressed();

	FString Base64 = FBase64::Encode(PNGData.GetData(), PNGData.Num());

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("image_base64"), Base64);
	Data->SetNumberField(TEXT("width"), ViewportWidth);
	Data->SetNumberField(TEXT("height"), ViewportHeight);
	Data->SetStringField(TEXT("format"), TEXT("png"));
	return SuccessResponse(Data);
}

// --- Focus Viewport ---
TSharedPtr<FJsonObject> FMCPFocusViewportCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Target = Params->GetStringField(TEXT("target"));
	double Distance = 500.0;
	Params->TryGetNumberField(TEXT("distance"), Distance);

	if (!GEditor)
	{
		return ErrorResponse(TEXT("No editor available"));
	}

	FLevelEditorViewportClient* ViewportClient = nullptr;
	if (GEditor->GetLevelViewportClients().Num() > 0)
	{
		ViewportClient = GEditor->GetLevelViewportClients()[0];
	}

	if (!ViewportClient)
	{
		return ErrorResponse(TEXT("No level viewport client available"));
	}

	FVector LookAtLocation = FVector::ZeroVector;

	if (!Target.IsEmpty())
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (World)
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->GetActorLabel() == Target || It->GetName() == Target)
				{
					LookAtLocation = It->GetActorLocation();
					break;
				}
			}
		}
	}
	else
	{
		const TArray<TSharedPtr<FJsonValue>>* LocArr;
		if (Params->TryGetArrayField(TEXT("location"), LocArr) && LocArr->Num() >= 3)
		{
			LookAtLocation = FVector((*LocArr)[0]->AsNumber(), (*LocArr)[1]->AsNumber(), (*LocArr)[2]->AsNumber());
		}
	}

	// Set viewport location
	FVector CameraLocation = LookAtLocation - ViewportClient->GetViewRotation().Vector() * Distance;
	ViewportClient->SetViewLocation(CameraLocation);

	const TArray<TSharedPtr<FJsonValue>>* RotArr;
	if (Params->TryGetArrayField(TEXT("rotation"), RotArr) && RotArr->Num() >= 3)
	{
		FRotator NewRot((*RotArr)[0]->AsNumber(), (*RotArr)[1]->AsNumber(), (*RotArr)[2]->AsNumber());
		ViewportClient->SetViewRotation(NewRot);
	}

	ViewportClient->Invalidate();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	FVector FinalLoc = ViewportClient->GetViewLocation();
	FRotator FinalRot = ViewportClient->GetViewRotation();

	TArray<TSharedPtr<FJsonValue>> LocResult = {
		MakeShared<FJsonValueNumber>(FinalLoc.X),
		MakeShared<FJsonValueNumber>(FinalLoc.Y),
		MakeShared<FJsonValueNumber>(FinalLoc.Z)
	};
	Data->SetArrayField(TEXT("camera_location"), LocResult);

	TArray<TSharedPtr<FJsonValue>> RotResult = {
		MakeShared<FJsonValueNumber>(FinalRot.Pitch),
		MakeShared<FJsonValueNumber>(FinalRot.Yaw),
		MakeShared<FJsonValueNumber>(FinalRot.Roll)
	};
	Data->SetArrayField(TEXT("camera_rotation"), RotResult);

	return SuccessResponse(Data);
}
