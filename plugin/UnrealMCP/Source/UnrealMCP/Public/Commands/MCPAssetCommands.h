#pragma once

#include "Commands/MCPCommandBase.h"

// --- Import Asset ---
class FMCPImportAssetCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("import_asset"); }
};

// --- Search Assets ---
class FMCPSearchAssetsCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("search_assets"); }
};

// --- Get Asset Info ---
class FMCPGetAssetInfoCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("get_asset_info"); }
};

// --- Delete Asset ---
class FMCPDeleteAssetCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("delete_asset"); }
};

// --- Rename Asset ---
class FMCPRenameAssetCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("rename_asset"); }
};
