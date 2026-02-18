#include "Commands/MCPDataTableCommands.h"
#include "Engine/DataTable.h"
#include "DataTableEditorUtils.h"
#include "Factories/DataTableFactory.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace DataTableCommandsLocal
{

// ---------------------------------------------------------------------------
// Helper: Resolve a row struct from a name or asset path
// ---------------------------------------------------------------------------
static UScriptStruct* FindOrLoadRowStruct(const FString& StructRef)
{
	if (StructRef.IsEmpty())
	{
		return nullptr;
	}

	// Try as a full asset path first (Blueprint structs: /Game/Structs/MyStruct.MyStruct)
	UScriptStruct* Struct = LoadObject<UScriptStruct>(nullptr, *StructRef);
	if (Struct)
	{
		return Struct;
	}

	// Try as a short name (C++ structs like FMyRowStruct or MyRowStruct)
	Struct = FindFirstObject<UScriptStruct>(*StructRef, EFindFirstObjectOptions::NativeFirst);
	if (Struct)
	{
		return Struct;
	}

	// Try without F prefix
	FString SearchName = StructRef;
	if (SearchName.StartsWith(TEXT("F")))
	{
		SearchName = SearchName.RightChop(1);
		Struct = FindFirstObject<UScriptStruct>(*SearchName, EFindFirstObjectOptions::NativeFirst);
	}

	return Struct;
}

// ---------------------------------------------------------------------------
// Helper: Load a DataTable by asset path
// ---------------------------------------------------------------------------
static UDataTable* LoadDataTable(const FString& AssetPath)
{
	return Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *AssetPath));
}

// ---------------------------------------------------------------------------
// Helper: Serialize a row to a JSON object using FProperty iteration
// ---------------------------------------------------------------------------
static TSharedPtr<FJsonObject> RowToJson(const UScriptStruct* RowStruct, const uint8* RowData)
{
	TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
	for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
	{
		FProperty* Prop = *It;
		FString ValueStr;
		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(RowData);
		Prop->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, nullptr, PPF_None);
		RowObj->SetStringField(Prop->GetName(), ValueStr);
	}
	return RowObj;
}

// ---------------------------------------------------------------------------
// Helper: Set row fields from a JSON values object
// ---------------------------------------------------------------------------
static bool SetRowFieldsFromJson(const UScriptStruct* RowStruct, uint8* RowData,
	const TSharedPtr<FJsonObject>& Values, FString& OutError)
{
	for (const auto& Pair : Values->Values)
	{
		FProperty* Prop = RowStruct->FindPropertyByName(FName(*Pair.Key));
		if (!Prop)
		{
			OutError = FString::Printf(TEXT("Field '%s' not found in row struct"), *Pair.Key);
			return false;
		}

		FString ValueStr;
		if (!Pair.Value->TryGetString(ValueStr))
		{
			// Try number → string conversion
			double NumVal;
			if (Pair.Value->TryGetNumber(NumVal))
			{
				ValueStr = FString::SanitizeFloat(NumVal);
			}
			else
			{
				bool BoolVal;
				if (Pair.Value->TryGetBool(BoolVal))
				{
					ValueStr = BoolVal ? TEXT("True") : TEXT("False");
				}
				else
				{
					OutError = FString::Printf(TEXT("Cannot convert value for field '%s' to string"), *Pair.Key);
					return false;
				}
			}
		}

		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(RowData);
		const TCHAR* Result = Prop->ImportText_Direct(*ValueStr, ValuePtr, nullptr, PPF_None);
		if (!Result)
		{
			OutError = FString::Printf(TEXT("Failed to set field '%s' to '%s'"), *Pair.Key, *ValueStr);
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------
// Helper: Get column info (name + type) for a row struct
// ---------------------------------------------------------------------------
static TArray<TSharedPtr<FJsonValue>> GetColumnInfo(const UScriptStruct* RowStruct)
{
	TArray<TSharedPtr<FJsonValue>> Columns;
	for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
	{
		FProperty* Prop = *It;
		TSharedPtr<FJsonObject> ColObj = MakeShared<FJsonObject>();
		ColObj->SetStringField(TEXT("name"), Prop->GetName());
		ColObj->SetStringField(TEXT("type"), Prop->GetCPPType());
		Columns.Add(MakeShared<FJsonValueObject>(ColObj));
	}
	return Columns;
}

} // namespace DataTableCommandsLocal

using namespace DataTableCommandsLocal;

// ===================================================================
// create_data_table
// ===================================================================
TSharedPtr<FJsonObject> FMCPCreateDataTableCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetName = Params->GetStringField(TEXT("asset_name"));
	FString PackagePath = Params->GetStringField(TEXT("package_path"));
	FString RowStructRef = Params->GetStringField(TEXT("row_struct"));

	if (AssetName.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_name is required"));
	}
	if (PackagePath.IsEmpty())
	{
		return ErrorResponse(TEXT("package_path is required"));
	}
	if (RowStructRef.IsEmpty())
	{
		return ErrorResponse(TEXT("row_struct is required"));
	}

	UScriptStruct* RowStruct = FindOrLoadRowStruct(RowStructRef);
	if (!RowStruct)
	{
		return ErrorResponse(FString::Printf(TEXT("Row struct not found: %s"), *RowStructRef));
	}

	UDataTableFactory* Factory = NewObject<UDataTableFactory>();
	Factory->Struct = RowStruct;

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UDataTable::StaticClass(), Factory);

	if (!NewAsset)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to create DataTable '%s' at '%s'"), *AssetName, *PackagePath));
	}

	UDataTable* DT = Cast<UDataTable>(NewAsset);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), NewAsset->GetPathName());
	Data->SetStringField(TEXT("asset_name"), AssetName);
	Data->SetStringField(TEXT("row_struct"), RowStruct->GetName());
	Data->SetStringField(TEXT("row_struct_path"), RowStruct->GetPathName());
	Data->SetArrayField(TEXT("columns"), GetColumnInfo(RowStruct));
	return SuccessResponse(Data);
}

// ===================================================================
// add_data_table_row
// ===================================================================
TSharedPtr<FJsonObject> FMCPAddDataTableRowCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString RowName = Params->GetStringField(TEXT("row_name"));

	if (AssetPath.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_path is required"));
	}
	if (RowName.IsEmpty())
	{
		return ErrorResponse(TEXT("row_name is required"));
	}

	UDataTable* DT = LoadDataTable(AssetPath);
	if (!DT)
	{
		return ErrorResponse(FString::Printf(TEXT("DataTable not found: %s"), *AssetPath));
	}

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct)
	{
		return ErrorResponse(TEXT("DataTable has no row struct"));
	}

	FName RowFName(*RowName);

	// Check if row already exists
	if (DT->FindRowUnchecked(RowFName))
	{
		return ErrorResponse(FString::Printf(TEXT("Row '%s' already exists"), *RowName));
	}

	// Add the row (returns pointer to zero-initialized row data)
	uint8* NewRowData = FDataTableEditorUtils::AddRow(DT, RowFName);
	if (!NewRowData)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to add row '%s'"), *RowName));
	}

	// Set field values if provided
	const TSharedPtr<FJsonObject>* ValuesObj = nullptr;
	if (Params->TryGetObjectField(TEXT("values"), ValuesObj) && ValuesObj && (*ValuesObj).IsValid())
	{
		FString Error;
		if (!SetRowFieldsFromJson(RowStruct, NewRowData, *ValuesObj, Error))
		{
			return ErrorResponse(Error);
		}
	}

	DT->MarkPackageDirty();
	FDataTableEditorUtils::BroadcastPostChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("row_name"), RowName);
	Data->SetObjectField(TEXT("values"), RowToJson(RowStruct, NewRowData));
	Data->SetNumberField(TEXT("total_rows"), DT->GetRowNames().Num());
	return SuccessResponse(Data);
}

// ===================================================================
// modify_data_table_row
// ===================================================================
TSharedPtr<FJsonObject> FMCPModifyDataTableRowCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString RowName = Params->GetStringField(TEXT("row_name"));

	if (AssetPath.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_path is required"));
	}
	if (RowName.IsEmpty())
	{
		return ErrorResponse(TEXT("row_name is required"));
	}

	UDataTable* DT = LoadDataTable(AssetPath);
	if (!DT)
	{
		return ErrorResponse(FString::Printf(TEXT("DataTable not found: %s"), *AssetPath));
	}

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct)
	{
		return ErrorResponse(TEXT("DataTable has no row struct"));
	}

	FName RowFName(*RowName);
	uint8* RowData = DT->FindRowUnchecked(RowFName);
	if (!RowData)
	{
		return ErrorResponse(FString::Printf(TEXT("Row '%s' not found"), *RowName));
	}

	const TSharedPtr<FJsonObject>* ValuesObj = nullptr;
	if (!Params->TryGetObjectField(TEXT("values"), ValuesObj) || !ValuesObj || !(*ValuesObj).IsValid())
	{
		return ErrorResponse(TEXT("values object is required"));
	}

	FDataTableEditorUtils::BroadcastPreChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowData);

	FString Error;
	if (!SetRowFieldsFromJson(RowStruct, RowData, *ValuesObj, Error))
	{
		return ErrorResponse(Error);
	}

	DT->MarkPackageDirty();
	FDataTableEditorUtils::BroadcastPostChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowData);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("row_name"), RowName);
	Data->SetObjectField(TEXT("values"), RowToJson(RowStruct, RowData));
	return SuccessResponse(Data);
}

// ===================================================================
// delete_data_table_row
// ===================================================================
TSharedPtr<FJsonObject> FMCPDeleteDataTableRowCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString RowName = Params->GetStringField(TEXT("row_name"));

	if (AssetPath.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_path is required"));
	}
	if (RowName.IsEmpty())
	{
		return ErrorResponse(TEXT("row_name is required"));
	}

	UDataTable* DT = LoadDataTable(AssetPath);
	if (!DT)
	{
		return ErrorResponse(FString::Printf(TEXT("DataTable not found: %s"), *AssetPath));
	}

	FName RowFName(*RowName);
	if (!DT->FindRowUnchecked(RowFName))
	{
		return ErrorResponse(FString::Printf(TEXT("Row '%s' not found"), *RowName));
	}

	// Use FDataTableEditorUtils::RemoveRow — NOT UDataTable::RemoveRow (crashes in UE 5.6!)
	bool bRemoved = FDataTableEditorUtils::RemoveRow(DT, RowFName);
	if (!bRemoved)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to remove row '%s'"), *RowName));
	}

	DT->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("row_name"), RowName);
	Data->SetNumberField(TEXT("remaining_rows"), DT->GetRowNames().Num());
	return SuccessResponse(Data);
}

// ===================================================================
// get_data_table_rows
// ===================================================================
TSharedPtr<FJsonObject> FMCPGetDataTableRowsCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	if (AssetPath.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_path is required"));
	}

	UDataTable* DT = LoadDataTable(AssetPath);
	if (!DT)
	{
		return ErrorResponse(FString::Printf(TEXT("DataTable not found: %s"), *AssetPath));
	}

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct)
	{
		return ErrorResponse(TEXT("DataTable has no row struct"));
	}

	FString SpecificRow;
	Params->TryGetStringField(TEXT("row_name"), SpecificRow);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("row_struct"), RowStruct->GetName());
	Data->SetStringField(TEXT("row_struct_path"), RowStruct->GetPathName());
	Data->SetArrayField(TEXT("columns"), GetColumnInfo(RowStruct));

	TArray<TSharedPtr<FJsonValue>> RowsArray;

	if (!SpecificRow.IsEmpty())
	{
		// Single row
		FName RowFName(*SpecificRow);
		uint8* RowData = DT->FindRowUnchecked(RowFName);
		if (!RowData)
		{
			return ErrorResponse(FString::Printf(TEXT("Row '%s' not found"), *SpecificRow));
		}

		TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
		RowObj->SetStringField(TEXT("row_name"), SpecificRow);
		RowObj->SetObjectField(TEXT("values"), RowToJson(RowStruct, RowData));
		RowsArray.Add(MakeShared<FJsonValueObject>(RowObj));
	}
	else
	{
		// All rows
		TArray<FName> RowNames = DT->GetRowNames();
		for (const FName& Name : RowNames)
		{
			uint8* RowData = DT->FindRowUnchecked(Name);
			if (RowData)
			{
				TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
				RowObj->SetStringField(TEXT("row_name"), Name.ToString());
				RowObj->SetObjectField(TEXT("values"), RowToJson(RowStruct, RowData));
				RowsArray.Add(MakeShared<FJsonValueObject>(RowObj));
			}
		}
	}

	Data->SetArrayField(TEXT("rows"), RowsArray);
	Data->SetNumberField(TEXT("total_rows"), DT->GetRowNames().Num());
	return SuccessResponse(Data);
}

// ===================================================================
// import_data_table_csv
// ===================================================================
TSharedPtr<FJsonObject> FMCPImportDataTableCSVCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString CsvData = Params->GetStringField(TEXT("csv_data"));

	if (AssetPath.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_path is required"));
	}
	if (CsvData.IsEmpty())
	{
		return ErrorResponse(TEXT("csv_data is required"));
	}

	UDataTable* DT = LoadDataTable(AssetPath);
	if (!DT)
	{
		return ErrorResponse(FString::Printf(TEXT("DataTable not found: %s"), *AssetPath));
	}

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct)
	{
		return ErrorResponse(TEXT("DataTable has no row struct"));
	}

	bool bAppend = false;
	Params->TryGetBoolField(TEXT("append"), bAppend);

	TArray<FString> Errors;

	if (!bAppend)
	{
		// CreateTableFromCSVString clears the table and imports fresh
		Errors = DT->CreateTableFromCSVString(CsvData);
	}
	else
	{
		// Append mode: parse CSV and add rows one by one
		// CSV format: first row = header (Name,---,Col1,Col2,...), subsequent rows = data
		TArray<FString> Lines;
		CsvData.ParseIntoArray(Lines, TEXT("\n"), true);

		if (Lines.Num() < 2)
		{
			return ErrorResponse(TEXT("CSV must have at least a header row and one data row"));
		}

		// Parse header
		TArray<FString> Headers;
		Lines[0].ParseIntoArray(Headers, TEXT(","), false);

		if (Headers.Num() < 1)
		{
			return ErrorResponse(TEXT("CSV header is empty"));
		}

		// First column is the row name, rest are field names
		// Second column is typically "---" (separator in UE CSV format), skip it
		int32 FirstDataCol = 1;
		if (Headers.Num() > 1 && Headers[1].TrimStartAndEnd() == TEXT("---"))
		{
			FirstDataCol = 2;
		}

		// Add each data row
		for (int32 i = 1; i < Lines.Num(); i++)
		{
			FString Line = Lines[i].TrimStartAndEnd();
			if (Line.IsEmpty())
			{
				continue;
			}

			TArray<FString> Fields;
			Line.ParseIntoArray(Fields, TEXT(","), false);

			if (Fields.Num() < 1)
			{
				continue;
			}

			FName RowFName(*Fields[0].TrimStartAndEnd());

			// Skip if row already exists
			if (DT->FindRowUnchecked(RowFName))
			{
				Errors.Add(FString::Printf(TEXT("Row '%s' already exists, skipped"), *Fields[0]));
				continue;
			}

			uint8* NewRowData = FDataTableEditorUtils::AddRow(DT, RowFName);
			if (!NewRowData)
			{
				Errors.Add(FString::Printf(TEXT("Failed to add row '%s'"), *Fields[0]));
				continue;
			}

			// Set fields from CSV columns
			for (int32 ColIdx = FirstDataCol; ColIdx < Headers.Num() && ColIdx < Fields.Num(); ColIdx++)
			{
				FString FieldName = Headers[ColIdx].TrimStartAndEnd();
				FString FieldValue = Fields[ColIdx].TrimStartAndEnd();

				if (FieldName.IsEmpty())
				{
					continue;
				}

				FProperty* Prop = RowStruct->FindPropertyByName(FName(*FieldName));
				if (!Prop)
				{
					Errors.Add(FString::Printf(TEXT("Row '%s': field '%s' not found in struct"), *Fields[0], *FieldName));
					continue;
				}

				void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(NewRowData);
				const TCHAR* Result = Prop->ImportText_Direct(*FieldValue, ValuePtr, nullptr, PPF_None);
				if (!Result)
				{
					Errors.Add(FString::Printf(TEXT("Row '%s': failed to set field '%s' to '%s'"), *Fields[0], *FieldName, *FieldValue));
				}
			}
		}

		FDataTableEditorUtils::BroadcastPostChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	}

	DT->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("total_rows"), DT->GetRowNames().Num());
	Data->SetBoolField(TEXT("appended"), bAppend);

	TArray<TSharedPtr<FJsonValue>> ErrorArray;
	for (const FString& Err : Errors)
	{
		ErrorArray.Add(MakeShared<FJsonValueString>(Err));
	}
	Data->SetArrayField(TEXT("errors"), ErrorArray);
	Data->SetNumberField(TEXT("error_count"), Errors.Num());

	return SuccessResponse(Data);
}
