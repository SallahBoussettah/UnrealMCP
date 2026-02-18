#pragma once

#include "Commands/MCPCommandBase.h"

// --- Create Widget Blueprint ---
class FMCPCreateWidgetBlueprintCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("create_widget_blueprint"); }
};

// --- Add Widget ---
class FMCPAddWidgetCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("add_widget"); }
};

// --- Set Widget Property ---
class FMCPSetWidgetPropertyCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("set_widget_property"); }
};

// --- Get Widget Tree ---
class FMCPGetWidgetTreeCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("get_widget_tree"); }
};

// --- Remove Widget ---
class FMCPRemoveWidgetCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("remove_widget"); }
};
