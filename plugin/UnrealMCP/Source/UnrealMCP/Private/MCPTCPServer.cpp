#include "MCPTCPServer.h"
#include "Commands/MCPCommandBase.h"
#include "Commands/MCPBlueprintCommands.h"
#include "Commands/MCPActorCommands.h"
#include "Commands/MCPPropertyCommands.h"
#include "Commands/MCPNodeGraphCommands.h"
#include "Commands/MCPViewportCommands.h"
#include "Commands/MCPConsoleCommands.h"
#include "Commands/MCPMaterialCommands.h"
#include "Commands/MCPLevelCommands.h"
#include "Commands/MCPAssetCommands.h"
#include "Commands/MCPPIECommands.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"

FMCPTCPServer::FMCPTCPServer(int32 InPort)
	: Port(InPort)
	, ListenerSocket(nullptr)
	, Thread(nullptr)
	, bShouldRun(false)
{
}

FMCPTCPServer::~FMCPTCPServer()
{
	Stop();
}

bool FMCPTCPServer::Start()
{
	RegisterCommands();

	bShouldRun = true;
	Thread = FRunnableThread::Create(this, TEXT("MCPTCPServer"), 0, TPri_Normal);
	return Thread != nullptr;
}

void FMCPTCPServer::Stop()
{
	bShouldRun = false;

	if (ListenerSocket)
	{
		ListenerSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket);
		ListenerSocket = nullptr;
	}

	if (Thread)
	{
		Thread->WaitForCompletion();
		FRunnableThread* ThreadToDelete = Thread;
		Thread = nullptr;
		delete ThreadToDelete;
	}

	CommandHandlers.Empty();
}

bool FMCPTCPServer::Init()
{
	return true;
}

uint32 FMCPTCPServer::Run()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	FIPv4Endpoint Endpoint(FIPv4Address::Any, Port);
	ListenerSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("MCPListener"), false);

	if (!ListenerSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealMCP: Failed to create listener socket"));
		return 1;
	}

	ListenerSocket->SetReuseAddr(true);
	ListenerSocket->SetNonBlocking(true);

	if (!ListenerSocket->Bind(*Endpoint.ToInternetAddr()))
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealMCP: Failed to bind to port %d"), Port);
		return 1;
	}

	if (!ListenerSocket->Listen(1))
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealMCP: Failed to listen on port %d"), Port);
		return 1;
	}

	UE_LOG(LogTemp, Log, TEXT("UnrealMCP: Listening on port %d"), Port);

	while (bShouldRun)
	{
		bool bHasPendingConnection;
		if (ListenerSocket->HasPendingConnection(bHasPendingConnection) && bHasPendingConnection)
		{
			TSharedRef<FInternetAddr> RemoteAddress = SocketSubsystem->CreateInternetAddr();
			FSocket* ClientSocket = ListenerSocket->Accept(*RemoteAddress, TEXT("MCPClient"));

			if (ClientSocket)
			{
				UE_LOG(LogTemp, Log, TEXT("UnrealMCP: Client connected from %s"), *RemoteAddress->ToString(true));

				ClientSocket->SetNonBlocking(false);
				ClientSocket->SetNoDelay(true);

				// Handle client in a loop until disconnected
				while (bShouldRun && ClientSocket)
				{
					FString Message;
					if (ReadMessage(ClientSocket, Message))
					{
						// Parse JSON
						TSharedPtr<FJsonObject> JsonCommand;
						TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);

						if (FJsonSerializer::Deserialize(Reader, JsonCommand) && JsonCommand.IsValid())
						{
							// Process command on game thread and wait for result
							TSharedPtr<FJsonObject> Response;
							FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool(true);

							AsyncTask(ENamedThreads::GameThread, [this, JsonCommand, &Response, DoneEvent]()
							{
								Response = ProcessCommand(JsonCommand);
								DoneEvent->Trigger();
							});

							DoneEvent->Wait();
							FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

							if (!SendResponse(ClientSocket, Response))
							{
								UE_LOG(LogTemp, Warning, TEXT("UnrealMCP: Failed to send response, client disconnected"));
								break;
							}
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("UnrealMCP: Failed to parse JSON command"));

							TSharedPtr<FJsonObject> ErrorResp = MakeShared<FJsonObject>();
							ErrorResp->SetBoolField(TEXT("success"), false);
							ErrorResp->SetStringField(TEXT("error"), TEXT("Invalid JSON"));
							SendResponse(ClientSocket, ErrorResp);
						}
					}
					else
					{
						// Client disconnected or read error
						break;
					}
				}

				UE_LOG(LogTemp, Log, TEXT("UnrealMCP: Client disconnected"));
				ClientSocket->Close();
				SocketSubsystem->DestroySocket(ClientSocket);
			}
		}

		FPlatformProcess::Sleep(0.01f);
	}

	return 0;
}

void FMCPTCPServer::Exit()
{
}

bool FMCPTCPServer::ReadMessage(FSocket* ClientSocket, FString& OutMessage)
{
	// Read 4-byte length header (big-endian)
	uint8 HeaderBytes[4];
	int32 BytesRead = 0;
	int32 TotalRead = 0;

	while (TotalRead < 4)
	{
		if (!ClientSocket->Recv(HeaderBytes + TotalRead, 4 - TotalRead, BytesRead))
		{
			return false;
		}
		if (BytesRead == 0)
		{
			return false; // Connection closed
		}
		TotalRead += BytesRead;
	}

	int32 MessageLength = (HeaderBytes[0] << 24) | (HeaderBytes[1] << 16) | (HeaderBytes[2] << 8) | HeaderBytes[3];

	if (MessageLength <= 0 || MessageLength > 10 * 1024 * 1024) // 10MB max
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealMCP: Invalid message length: %d"), MessageLength);
		return false;
	}

	// Read the message body
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(MessageLength + 1); // +1 for null terminator
	TotalRead = 0;

	while (TotalRead < MessageLength)
	{
		if (!ClientSocket->Recv(Buffer.GetData() + TotalRead, MessageLength - TotalRead, BytesRead))
		{
			return false;
		}
		if (BytesRead == 0)
		{
			return false;
		}
		TotalRead += BytesRead;
	}

	Buffer[MessageLength] = 0; // Null-terminate the buffer
	OutMessage = UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer.GetData()));
	return true;
}

bool FMCPTCPServer::SendResponse(FSocket* ClientSocket, const TSharedPtr<FJsonObject>& Response)
{
	FString ResponseStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseStr);
	FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);

	FTCHARToUTF8 Converter(*ResponseStr);
	int32 PayloadLength = Converter.Length();

	// Write 4-byte length header (big-endian)
	uint8 Header[4];
	Header[0] = (PayloadLength >> 24) & 0xFF;
	Header[1] = (PayloadLength >> 16) & 0xFF;
	Header[2] = (PayloadLength >> 8) & 0xFF;
	Header[3] = PayloadLength & 0xFF;

	int32 BytesSent;
	if (!ClientSocket->Send(Header, 4, BytesSent) || BytesSent != 4)
	{
		return false;
	}

	// Send payload in a loop to handle partial sends (important for large responses like screenshots)
	const uint8* PayloadData = reinterpret_cast<const uint8*>(Converter.Get());
	int32 TotalSent = 0;
	while (TotalSent < PayloadLength)
	{
		if (!ClientSocket->Send(PayloadData + TotalSent, PayloadLength - TotalSent, BytesSent))
		{
			return false;
		}
		if (BytesSent == 0)
		{
			return false;
		}
		TotalSent += BytesSent;
	}

	return true;
}

TSharedPtr<FJsonObject> FMCPTCPServer::ProcessCommand(const TSharedPtr<FJsonObject>& Command)
{
	FString CommandName = Command->GetStringField(TEXT("command"));
	FString RequestId = Command->GetStringField(TEXT("id"));

	TSharedPtr<FJsonObject> Response;

	if (CommandName == TEXT("batch_execute"))
	{
		// Special handling: execute multiple commands sequentially
		Response = ProcessBatchCommand(Command);
	}
	else if (TSharedPtr<FMCPCommandBase>* Handler = CommandHandlers.Find(CommandName))
	{
		const TSharedPtr<FJsonObject>* Params = nullptr;
		Command->TryGetObjectField(TEXT("params"), Params);

		TSharedPtr<FJsonObject> ParamsObj = Params ? *Params : MakeShared<FJsonObject>();
		Response = (*Handler)->Execute(ParamsObj);
	}
	else
	{
		Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown command: %s"), *CommandName));
	}

	// Always include the request ID in the response
	Response->SetStringField(TEXT("id"), RequestId);
	return Response;
}

TSharedPtr<FJsonObject> FMCPTCPServer::ProcessBatchCommand(const TSharedPtr<FJsonObject>& Command)
{
	const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
	Command->TryGetObjectField(TEXT("params"), ParamsPtr);
	if (!ParamsPtr)
	{
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), TEXT("Missing params"));
		return Err;
	}

	const TArray<TSharedPtr<FJsonValue>>* CommandsArr = nullptr;
	if (!(*ParamsPtr)->TryGetArrayField(TEXT("commands"), CommandsArr) || !CommandsArr)
	{
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), TEXT("Missing 'commands' array in params"));
		return Err;
	}

	bool bStopOnError = false;
	(*ParamsPtr)->TryGetBoolField(TEXT("stop_on_error"), bStopOnError);

	TArray<TSharedPtr<FJsonValue>> Results;
	bool bAllSucceeded = true;

	for (int32 i = 0; i < CommandsArr->Num(); ++i)
	{
		const TSharedPtr<FJsonObject>* SubCmdPtr = nullptr;
		if (!(*CommandsArr)[i]->TryGetObject(SubCmdPtr) || !SubCmdPtr)
		{
			TSharedPtr<FJsonObject> SubErr = MakeShared<FJsonObject>();
			SubErr->SetNumberField(TEXT("index"), i);
			SubErr->SetBoolField(TEXT("success"), false);
			SubErr->SetStringField(TEXT("error"), TEXT("Invalid command object"));
			Results.Add(MakeShared<FJsonValueObject>(SubErr));
			bAllSucceeded = false;
			if (bStopOnError) break;
			continue;
		}

		FString SubCommandName = (*SubCmdPtr)->GetStringField(TEXT("command"));
		const TSharedPtr<FJsonObject>* SubParams = nullptr;
		(*SubCmdPtr)->TryGetObjectField(TEXT("params"), SubParams);
		TSharedPtr<FJsonObject> SubParamsObj = SubParams ? *SubParams : MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> SubResult;
		if (TSharedPtr<FMCPCommandBase>* Handler = CommandHandlers.Find(SubCommandName))
		{
			SubResult = (*Handler)->Execute(SubParamsObj);
		}
		else
		{
			SubResult = MakeShared<FJsonObject>();
			SubResult->SetBoolField(TEXT("success"), false);
			SubResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown command: %s"), *SubCommandName));
		}

		SubResult->SetNumberField(TEXT("index"), i);
		SubResult->SetStringField(TEXT("command"), SubCommandName);
		Results.Add(MakeShared<FJsonValueObject>(SubResult));

		if (!SubResult->GetBoolField(TEXT("success")))
		{
			bAllSucceeded = false;
			if (bStopOnError) break;
		}
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), bAllSucceeded);
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("results"), Results);
	Data->SetNumberField(TEXT("total"), CommandsArr->Num());
	Data->SetNumberField(TEXT("executed"), Results.Num());
	Response->SetObjectField(TEXT("data"), Data);
	return Response;
}

void FMCPTCPServer::RegisterCommands()
{
	// Create log capture for console commands
	TSharedPtr<FMCPLogCapture> LogCapture = MakeShared<FMCPLogCapture>(500);
	LogCapture->Register();

	auto Register = [this](TSharedPtr<FMCPCommandBase> Cmd)
	{
		CommandHandlers.Add(Cmd->GetCommandName(), Cmd);
	};

	// Blueprint commands
	Register(MakeShared<FMCPCreateBlueprintCommand>());
	Register(MakeShared<FMCPListBlueprintsCommand>());
	Register(MakeShared<FMCPGetBlueprintInfoCommand>());
	Register(MakeShared<FMCPCompileBlueprintCommand>());
	Register(MakeShared<FMCPDeleteBlueprintCommand>());
	Register(MakeShared<FMCPAddBlueprintVariableCommand>());
	Register(MakeShared<FMCPRemoveBlueprintVariableCommand>());
	Register(MakeShared<FMCPAddBlueprintComponentCommand>());
	Register(MakeShared<FMCPSetBlueprintComponentDefaultsCommand>());

	// Actor commands
	Register(MakeShared<FMCPSpawnActorCommand>());
	Register(MakeShared<FMCPDeleteActorCommand>());
	Register(MakeShared<FMCPSetActorTransformCommand>());
	Register(MakeShared<FMCPGetActorsInLevelCommand>());
	Register(MakeShared<FMCPFindActorsCommand>());
	Register(MakeShared<FMCPDuplicateActorCommand>());

	// Property commands
	Register(MakeShared<FMCPGetObjectPropertiesCommand>());
	Register(MakeShared<FMCPSetObjectPropertyCommand>());
	Register(MakeShared<FMCPGetComponentHierarchyCommand>());
	Register(MakeShared<FMCPGetClassDefaultsCommand>());
	Register(MakeShared<FMCPSetComponentPropertyCommand>());

	// Node graph commands
	Register(MakeShared<FMCPAddNodeCommand>());
	Register(MakeShared<FMCPConnectPinsCommand>());
	Register(MakeShared<FMCPDisconnectPinsCommand>());
	Register(MakeShared<FMCPDeleteNodeCommand>());
	Register(MakeShared<FMCPGetGraphNodesCommand>());
	Register(MakeShared<FMCPSetPinValueCommand>());
	Register(MakeShared<FMCPCreateFunctionCommand>());
	Register(MakeShared<FMCPDeleteFunctionCommand>());

	// Viewport commands
	Register(MakeShared<FMCPTakeScreenshotCommand>());
	Register(MakeShared<FMCPFocusViewportCommand>());

	// Console commands
	Register(MakeShared<FMCPGetConsoleLogsCommand>(LogCapture));
	Register(MakeShared<FMCPExecuteConsoleCommandCommand>());

	// Material commands
	Register(MakeShared<FMCPCreateMaterialCommand>());
	Register(MakeShared<FMCPAssignMaterialCommand>());
	Register(MakeShared<FMCPModifyMaterialCommand>());
	Register(MakeShared<FMCPGetMaterialInfoCommand>());

	// Level commands
	Register(MakeShared<FMCPGetLevelInfoCommand>());
	Register(MakeShared<FMCPCreateLevelCommand>());
	Register(MakeShared<FMCPSaveLevelCommand>());
	Register(MakeShared<FMCPLoadLevelCommand>());
	Register(MakeShared<FMCPAddStreamingLevelCommand>());
	Register(MakeShared<FMCPRemoveStreamingLevelCommand>());
	Register(MakeShared<FMCPSetLevelVisibilityCommand>());

	// Asset commands
	Register(MakeShared<FMCPImportAssetCommand>());
	Register(MakeShared<FMCPSearchAssetsCommand>());
	Register(MakeShared<FMCPGetAssetInfoCommand>());
	Register(MakeShared<FMCPDeleteAssetCommand>());
	Register(MakeShared<FMCPRenameAssetCommand>());

	// PIE commands
	Register(MakeShared<FMCPStartPIECommand>());
	Register(MakeShared<FMCPStopPIECommand>());
	Register(MakeShared<FMCPGetPIEStatusCommand>());
	Register(MakeShared<FMCPSetPIEPausedCommand>());

	UE_LOG(LogTemp, Log, TEXT("UnrealMCP: Registered %d command handlers"), CommandHandlers.Num());
}
