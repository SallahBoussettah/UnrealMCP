#include "Commands/MCPCommandBase.h"

TSharedPtr<FJsonObject> FMCPCommandBase::SuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetObjectField(TEXT("data"), Data);
	return Response;
}

TSharedPtr<FJsonObject> FMCPCommandBase::SuccessResponse(const FString& Message)
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), Message);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetObjectField(TEXT("data"), Data);
	return Response;
}

TSharedPtr<FJsonObject> FMCPCommandBase::SuccessResponse(const TArray<TSharedPtr<FJsonValue>>& Data)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetArrayField(TEXT("data"), Data);
	return Response;
}

TSharedPtr<FJsonObject> FMCPCommandBase::ErrorResponse(const FString& Error)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error"), Error);
	return Response;
}
