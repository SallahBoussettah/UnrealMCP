#include "Commands/MCPConsoleCommands.h"
#include "Editor.h"
#include "Engine/World.h"

// --- Log Capture ---
FMCPLogCapture::FMCPLogCapture(int32 InMaxEntries)
	: MaxEntries(InMaxEntries)
	, bRegistered(false)
{
}

FMCPLogCapture::~FMCPLogCapture()
{
	Unregister();
}

void FMCPLogCapture::Register()
{
	if (!bRegistered)
	{
		GLog->AddOutputDevice(this);
		bRegistered = true;
	}
}

void FMCPLogCapture::Unregister()
{
	if (bRegistered)
	{
		GLog->RemoveOutputDevice(this);
		bRegistered = false;
	}
}

void FMCPLogCapture::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
	FScopeLock ScopeLock(&Lock);

	FLogEntry Entry;
	Entry.Timestamp = FDateTime::Now();
	Entry.Verbosity = Verbosity;
	Entry.Category = Category;
	Entry.Message = FString(V);

	LogBuffer.Add(Entry);

	// Ring buffer behavior
	while (LogBuffer.Num() > MaxEntries)
	{
		LogBuffer.RemoveAt(0);
	}
}

TArray<FMCPLogCapture::FLogEntry> FMCPLogCapture::GetLogs(int32 Count, ELogVerbosity::Type MinVerbosity, const FString& CategoryFilter) const
{
	FScopeLock ScopeLock(&Lock);

	TArray<FLogEntry> Result;
	for (int32 i = LogBuffer.Num() - 1; i >= 0 && Result.Num() < Count; --i)
	{
		const FLogEntry& Entry = LogBuffer[i];

		if (MinVerbosity != ELogVerbosity::All && Entry.Verbosity > MinVerbosity)
			continue;

		if (!CategoryFilter.IsEmpty() && Entry.Category.ToString() != CategoryFilter)
			continue;

		Result.Add(Entry);
	}

	// Reverse so oldest is first
	Algo::Reverse(Result);
	return Result;
}

// --- Get Console Logs ---
FMCPGetConsoleLogsCommand::FMCPGetConsoleLogsCommand(TSharedPtr<FMCPLogCapture> InLogCapture)
	: LogCapture(InLogCapture)
{
}

TSharedPtr<FJsonObject> FMCPGetConsoleLogsCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	int32 Count = 50;
	Params->TryGetNumberField(TEXT("count"), Count);

	FString VerbosityFilter = Params->GetStringField(TEXT("verbosity_filter"));
	FString CategoryFilter = Params->GetStringField(TEXT("category_filter"));

	ELogVerbosity::Type MinVerbosity = ELogVerbosity::All;
	if (VerbosityFilter == TEXT("Error")) MinVerbosity = ELogVerbosity::Error;
	else if (VerbosityFilter == TEXT("Warning")) MinVerbosity = ELogVerbosity::Warning;
	else if (VerbosityFilter == TEXT("Display")) MinVerbosity = ELogVerbosity::Display;
	else if (VerbosityFilter == TEXT("Log")) MinVerbosity = ELogVerbosity::Log;

	TArray<FMCPLogCapture::FLogEntry> Logs = LogCapture->GetLogs(Count, MinVerbosity, CategoryFilter);

	TArray<TSharedPtr<FJsonValue>> LogArray;
	for (const FMCPLogCapture::FLogEntry& Entry : Logs)
	{
		TSharedPtr<FJsonObject> LogEntryObj = MakeShared<FJsonObject>();
		LogEntryObj->SetStringField(TEXT("timestamp"), Entry.Timestamp.ToString());
		LogEntryObj->SetStringField(TEXT("category"), Entry.Category.ToString());
		LogEntryObj->SetStringField(TEXT("message"), Entry.Message);

		FString VerbStr;
		switch (Entry.Verbosity)
		{
			case ELogVerbosity::Fatal: VerbStr = TEXT("Fatal"); break;
			case ELogVerbosity::Error: VerbStr = TEXT("Error"); break;
			case ELogVerbosity::Warning: VerbStr = TEXT("Warning"); break;
			case ELogVerbosity::Display: VerbStr = TEXT("Display"); break;
			case ELogVerbosity::Log: VerbStr = TEXT("Log"); break;
			case ELogVerbosity::Verbose: VerbStr = TEXT("Verbose"); break;
			default: VerbStr = TEXT("Unknown"); break;
		}
		LogEntryObj->SetStringField(TEXT("verbosity"), VerbStr);

		LogArray.Add(MakeShared<FJsonValueObject>(LogEntryObj));
	}

	return SuccessResponse(LogArray);
}

// --- Execute Console Command ---
TSharedPtr<FJsonObject> FMCPExecuteConsoleCommandCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Command = Params->GetStringField(TEXT("command"));

	if (Command.IsEmpty())
	{
		return ErrorResponse(TEXT("Command is required"));
	}

	FString Target = Params->GetStringField(TEXT("target"));

	UWorld* World = nullptr;
	FString UsedTarget;

	if (Target == TEXT("pie"))
	{
		if (!GEditor || !GEditor->PlayWorld)
		{
			return ErrorResponse(TEXT("No Play-in-Editor session is running. Start one with start_pie first."));
		}
		World = GEditor->PlayWorld;
		UsedTarget = TEXT("pie");
	}
	else
	{
		World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		UsedTarget = TEXT("editor");
	}

	if (!World)
	{
		return ErrorResponse(TEXT("No world available"));
	}

	GEditor->Exec(World, *Command);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("command"), Command);
	Data->SetStringField(TEXT("target"), UsedTarget);
	Data->SetStringField(TEXT("status"), TEXT("executed"));
	return SuccessResponse(Data);
}
