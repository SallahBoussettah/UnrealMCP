#include "EpicUnrealMCPModule.h"
#include "EpicUnrealMCPBridge.h"
#include "MCPTCPServer.h"
#include "Modules/ModuleManager.h"
#include "EditorSubsystem.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "FEpicUnrealMCPModule"

void FEpicUnrealMCPModule::StartupModule()
{
	UE_LOG(LogTemp, Display, TEXT("Epic Unreal MCP Module has started"));

	// Start our custom TCP server on port 55555 (alongside Epic's bridge on 55557)
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

void FEpicUnrealMCPModule::ShutdownModule()
{
	UE_LOG(LogTemp, Display, TEXT("Epic Unreal MCP Module has shut down"));

	if (TCPServer.IsValid())
	{
		TCPServer->Stop();
		TCPServer.Reset();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEpicUnrealMCPModule, UnrealMCP)
