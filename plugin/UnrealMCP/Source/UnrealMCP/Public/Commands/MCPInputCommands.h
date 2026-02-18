#pragma once

#include "Commands/MCPCommandBase.h"

// --- Create Input Action ---
class FMCPCreateInputActionCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("create_input_action"); }
};

// --- Create Input Mapping Context ---
class FMCPCreateInputMappingContextCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("create_input_mapping_context"); }
};

// --- Add Input Mapping ---
class FMCPAddInputMappingCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("add_input_mapping"); }
};

// --- Remove Input Mapping ---
class FMCPRemoveInputMappingCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("remove_input_mapping"); }
};

// --- Get Input Mapping Context ---
class FMCPGetInputMappingContextCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("get_input_mapping_context"); }
};
