#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Sockets.h"
#include "Dom/JsonObject.h"

class FMCPCommandBase;

/**
 * TCP server that listens for JSON commands from the Python MCP server.
 * Runs a listener thread, processes commands on the game thread.
 */
class UNREALMCP_API FMCPTCPServer : public FRunnable
{
public:
	FMCPTCPServer(int32 InPort = 55555);
	virtual ~FMCPTCPServer();

	/** Start listening for connections. */
	bool Start();

	/** Stop the server and disconnect all clients. */
	void Stop();

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Exit() override;

private:
	/** Process a JSON command and return the response. */
	TSharedPtr<FJsonObject> ProcessCommand(const TSharedPtr<FJsonObject>& Command);

	/** Process a batch of commands sequentially. */
	TSharedPtr<FJsonObject> ProcessBatchCommand(const TSharedPtr<FJsonObject>& Command);

	/** Send a JSON response to the client. */
	bool SendResponse(FSocket* ClientSocket, const TSharedPtr<FJsonObject>& Response);

	/** Read a length-prefixed message from a socket. */
	bool ReadMessage(FSocket* ClientSocket, FString& OutMessage);

	/** Register all command handlers. */
	void RegisterCommands();

	int32 Port;
	FSocket* ListenerSocket;
	FRunnableThread* Thread;
	FThreadSafeBool bShouldRun;

	/** Registered command handlers mapped by command name. */
	TMap<FString, TSharedPtr<FMCPCommandBase>> CommandHandlers;
};
