#pragma once

#include "Commands/MCPCommandBase.h"

class FMCPTakeScreenshotCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("take_screenshot"); }
};

class FMCPFocusViewportCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("focus_viewport"); }
};
