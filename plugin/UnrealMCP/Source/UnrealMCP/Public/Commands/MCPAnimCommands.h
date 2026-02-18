#pragma once

#include "Commands/MCPCommandBase.h"

// --- Create Animation Blueprint ---
class FMCPCreateAnimBlueprintCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("create_anim_blueprint"); }
};

// --- Add Animation State ---
class FMCPAddAnimStateCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("add_anim_state"); }
};

// --- Add Animation Transition ---
class FMCPAddAnimTransitionCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("add_anim_transition"); }
};

// --- Set Animation Transition Rule ---
class FMCPSetAnimTransitionRuleCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("set_anim_transition_rule"); }
};

// --- Add Blend Space ---
class FMCPAddBlendSpaceCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("add_blend_space"); }
};

// --- Add Animation Montage ---
class FMCPAddAnimMontageCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("add_anim_montage"); }
};

// --- Get Animation Graph ---
class FMCPGetAnimGraphCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("get_anim_graph"); }
};
