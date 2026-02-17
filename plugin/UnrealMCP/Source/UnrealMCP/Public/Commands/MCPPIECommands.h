#pragma once

#include "Commands/MCPCommandBase.h"

// --- Start PIE ---
class FMCPStartPIECommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("start_pie"); }
};

// --- Stop PIE ---
class FMCPStopPIECommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("stop_pie"); }
};

// --- Get PIE Status ---
class FMCPGetPIEStatusCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("get_pie_status"); }
};

// --- Set PIE Paused ---
class FMCPSetPIEPausedCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("set_pie_paused"); }
};
