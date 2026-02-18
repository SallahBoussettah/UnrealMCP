#pragma once

#include "Commands/MCPCommandBase.h"

// --- Set Breakpoint ---
class FMCPSetBreakpointCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("set_breakpoint"); }
};

// --- Get Breakpoints ---
class FMCPGetBreakpointsCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("get_breakpoints"); }
};

// --- Get Watch Values ---
class FMCPGetWatchValuesCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("get_watch_values"); }
};

// --- Step Execution ---
class FMCPStepExecutionCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("step_execution"); }
};

// --- Get Call Stack ---
class FMCPGetCallStackCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("get_call_stack"); }
};
