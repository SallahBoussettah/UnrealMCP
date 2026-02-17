#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMCPTCPServer;

class FUnrealMCPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<FMCPTCPServer> TCPServer;
};
