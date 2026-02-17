#pragma once

#include "Commands/MCPCommandBase.h"

class FMCPSpawnActorCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("spawn_actor"); }
};

class FMCPDeleteActorCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("delete_actor"); }
};

class FMCPSetActorTransformCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("set_actor_transform"); }
};

class FMCPGetActorsInLevelCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("get_actors_in_level"); }
};

class FMCPFindActorsCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("find_actors"); }
};

class FMCPDuplicateActorCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("duplicate_actor"); }
};
