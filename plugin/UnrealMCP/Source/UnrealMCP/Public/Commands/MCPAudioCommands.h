#pragma once

#include "Commands/MCPCommandBase.h"

class FMCPCreateSoundCueCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("create_sound_cue"); }
};

class FMCPGetSoundCueInfoCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("get_sound_cue_info"); }
};
