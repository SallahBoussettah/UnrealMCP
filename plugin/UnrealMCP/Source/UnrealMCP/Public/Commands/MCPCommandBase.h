#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Base class for all MCP command handlers.
 * Each command handler processes a specific command from the MCP server.
 */
class UNREALMCP_API FMCPCommandBase
{
public:
	virtual ~FMCPCommandBase() = default;

	/**
	 * Execute the command with the given parameters.
	 * This is called on the game thread.
	 *
	 * @param Params - Command parameters from the JSON request
	 * @return Response JSON object with 'success', 'data', and optionally 'error' fields
	 */
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) = 0;

	/** Get the command name this handler responds to. */
	virtual FString GetCommandName() const = 0;

protected:
	/** Create a success response with data. */
	static TSharedPtr<FJsonObject> SuccessResponse(const TSharedPtr<FJsonObject>& Data);

	/** Create a success response with a simple message. */
	static TSharedPtr<FJsonObject> SuccessResponse(const FString& Message);

	/** Create a success response with an array. */
	static TSharedPtr<FJsonObject> SuccessResponse(const TArray<TSharedPtr<FJsonValue>>& Data);

	/** Create an error response. */
	static TSharedPtr<FJsonObject> ErrorResponse(const FString& Error);
};
