#pragma once

#include "Commands/MCPCommandBase.h"

// --- Get Level Info ---
class FMCPGetLevelInfoCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("get_level_info"); }
};

// --- Create Level ---
class FMCPCreateLevelCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("create_level"); }
};

// --- Save Level ---
class FMCPSaveLevelCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("save_level"); }
};

// --- Load Level ---
class FMCPLoadLevelCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("load_level"); }
};

// --- Add Streaming Level ---
class FMCPAddStreamingLevelCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("add_streaming_level"); }
};

// --- Remove Streaming Level ---
class FMCPRemoveStreamingLevelCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("remove_streaming_level"); }
};

// --- Set Level Visibility ---
class FMCPSetLevelVisibilityCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("set_level_visibility"); }
};
