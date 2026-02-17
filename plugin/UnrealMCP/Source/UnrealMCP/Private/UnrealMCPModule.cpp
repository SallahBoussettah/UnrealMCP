#include "UnrealMCPModule.h"
#include "MCPTCPServer.h"

#define LOCTEXT_NAMESPACE "FUnrealMCPModule"

void FUnrealMCPModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("UnrealMCP: Starting module..."));

	TCPServer = MakeShared<FMCPTCPServer>(55555);
	if (TCPServer->Start())
	{
		UE_LOG(LogTemp, Log, TEXT("UnrealMCP: TCP server started on port 55555"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealMCP: Failed to start TCP server"));
	}
}

void FUnrealMCPModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("UnrealMCP: Shutting down module..."));

	if (TCPServer.IsValid())
	{
		TCPServer->Stop();
		TCPServer.Reset();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealMCPModule, UnrealMCP)
