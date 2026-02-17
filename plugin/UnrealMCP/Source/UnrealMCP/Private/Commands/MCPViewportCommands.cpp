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
#include "RenderingThread.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif
#include "Windows/HideWindowsPlatformTypes.h"

// --- Take Screenshot ---
TSharedPtr<FJsonObject> FMCPTakeScreenshotCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return ErrorResponse(TEXT("No editor available"));
	}

	// Get the main editor window - try active first, then fall back to searching
	// all visible windows (handles the case where another app like Claude Code has focus)
	TSharedPtr<SWindow> MainWindow = FSlateApplication::Get().GetActiveTopLevelRegularWindow();
	if (!MainWindow.IsValid())
	{
		// Fallback: find the largest visible regular window (the main editor frame)
		TArray<TSharedRef<SWindow>> AllWindows;
		FSlateApplication::Get().GetAllVisibleWindowsOrdered(AllWindows);

		float LargestArea = 0.f;
		for (const TSharedRef<SWindow>& Win : AllWindows)
		{
			if (Win->IsRegularWindow())
			{
				FVector2D Size = Win->GetSizeInScreen();
				float Area = Size.X * Size.Y;
				if (Area > LargestArea)
				{
					LargestArea = Area;
					MainWindow = Win;
				}
			}
		}
	}
	if (!MainWindow.IsValid())
	{
		return ErrorResponse(TEXT("No editor window found - is the editor open?"));
	}

	TSharedPtr<FGenericWindow> NativeWindow = MainWindow->GetNativeWindow();
	if (!NativeWindow.IsValid())
	{
		return ErrorResponse(TEXT("No native window handle available"));
	}

	HWND hWnd = reinterpret_cast<HWND>(NativeWindow->GetOSWindowHandle());
	if (!hWnd)
	{
		return ErrorResponse(TEXT("Failed to get OS window handle"));
	}

	// Get window dimensions
	RECT WindowRect;
	GetClientRect(hWnd, &WindowRect);
	int32 CaptureWidth = WindowRect.right - WindowRect.left;
	int32 CaptureHeight = WindowRect.bottom - WindowRect.top;

	if (CaptureWidth <= 0 || CaptureHeight <= 0)
	{
		return ErrorResponse(TEXT("Editor window has invalid size"));
	}

	// Capture the window using Win32
	HDC hdcWindow = GetDC(hWnd);
	HDC hdcMem = CreateCompatibleDC(hdcWindow);
	HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, CaptureWidth, CaptureHeight);
	HGDIOBJ hOld = SelectObject(hdcMem, hBitmap);

	// PW_RENDERFULLCONTENT captures the full window including DirectX/OpenGL content
	PrintWindow(hWnd, hdcMem, PW_RENDERFULLCONTENT);

	// Read pixel data
	BITMAPINFOHEADER bi = {};
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = CaptureWidth;
	bi.biHeight = -CaptureHeight; // negative = top-down
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;

	TArray<FColor> Bitmap;
	Bitmap.SetNum(CaptureWidth * CaptureHeight);
	GetDIBits(hdcMem, hBitmap, 0, CaptureHeight, Bitmap.GetData(), reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);

	// Cleanup GDI
	SelectObject(hdcMem, hOld);
	DeleteObject(hBitmap);
	DeleteDC(hdcMem);
	ReleaseDC(hWnd, hdcWindow);

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

	if (!ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), CaptureWidth, CaptureHeight, ERGBFormat::BGRA, 8))
	{
		return ErrorResponse(TEXT("Failed to set raw image data"));
	}

	TArray64<uint8> PNGData = ImageWrapper->GetCompressed();

	FString Base64 = FBase64::Encode(PNGData.GetData(), PNGData.Num());

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("image_base64"), Base64);
	Data->SetNumberField(TEXT("width"), CaptureWidth);
	Data->SetNumberField(TEXT("height"), CaptureHeight);
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
