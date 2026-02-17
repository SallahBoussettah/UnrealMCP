#pragma once

#include "Commands/MCPCommandBase.h"

// --- Create Blueprint ---
class FMCPCreateBlueprintCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("create_blueprint"); }
};

// --- List Blueprints ---
class FMCPListBlueprintsCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("list_blueprints"); }
};

// --- Get Blueprint Info ---
class FMCPGetBlueprintInfoCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("get_blueprint_info"); }
};

// --- Compile Blueprint ---
class FMCPCompileBlueprintCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("compile_blueprint"); }
};

// --- Delete Blueprint ---
class FMCPDeleteBlueprintCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("delete_blueprint"); }
};

// --- Add Blueprint Variable ---
class FMCPAddBlueprintVariableCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("add_blueprint_variable"); }
};

// --- Remove Blueprint Variable ---
class FMCPRemoveBlueprintVariableCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("remove_blueprint_variable"); }
};

// --- Add Blueprint Component ---
class FMCPAddBlueprintComponentCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("add_blueprint_component"); }
};

// --- Set Blueprint Component Defaults ---
class FMCPSetBlueprintComponentDefaultsCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("set_blueprint_component_defaults"); }
};
