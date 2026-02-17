#pragma once

#include "Commands/MCPCommandBase.h"
#include "Misc/OutputDevice.h"

/**
 * Custom output device that captures console log messages for the MCP server.
 */
class FMCPLogCapture : public FOutputDevice
{
public:
	struct FLogEntry
	{
		FDateTime Timestamp;
		ELogVerbosity::Type Verbosity;
		FName Category;
		FString Message;
	};

	FMCPLogCapture(int32 MaxEntries = 500);
	virtual ~FMCPLogCapture();

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;

	/** Get recent log entries, optionally filtered. */
	TArray<FLogEntry> GetLogs(int32 Count, ELogVerbosity::Type MinVerbosity = ELogVerbosity::All, const FString& CategoryFilter = TEXT("")) const;

	/** Register/unregister with the global output device. */
	void Register();
	void Unregister();

private:
	TArray<FLogEntry> LogBuffer;
	int32 MaxEntries;
	mutable FCriticalSection Lock;
	bool bRegistered;
};

class FMCPGetConsoleLogsCommand : public FMCPCommandBase
{
public:
	FMCPGetConsoleLogsCommand(TSharedPtr<FMCPLogCapture> InLogCapture);
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("get_console_logs"); }

private:
	TSharedPtr<FMCPLogCapture> LogCapture;
};

class FMCPExecuteConsoleCommandCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("execute_console_command"); }
};
