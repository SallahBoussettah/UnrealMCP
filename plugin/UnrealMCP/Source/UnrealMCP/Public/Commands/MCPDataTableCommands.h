#pragma once

#include "Commands/MCPCommandBase.h"

// --- Create Data Table ---
class FMCPCreateDataTableCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("create_data_table"); }
};

// --- Add Data Table Row ---
class FMCPAddDataTableRowCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("add_data_table_row"); }
};

// --- Modify Data Table Row ---
class FMCPModifyDataTableRowCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("modify_data_table_row"); }
};

// --- Delete Data Table Row ---
class FMCPDeleteDataTableRowCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("delete_data_table_row"); }
};

// --- Get Data Table Rows ---
class FMCPGetDataTableRowsCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("get_data_table_rows"); }
};

// --- Import Data Table CSV ---
class FMCPImportDataTableCSVCommand : public FMCPCommandBase
{
public:
	virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual FString GetCommandName() const override { return TEXT("import_data_table_csv"); }
};
