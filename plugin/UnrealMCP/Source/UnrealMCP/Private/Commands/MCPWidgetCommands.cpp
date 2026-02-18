#include "Commands/MCPWidgetCommands.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Blueprint/UserWidget.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/EditableTextBox.h"
#include "Components/Spacer.h"
#include "Components/ScrollBox.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/WrapBox.h"
#include "Components/GridPanel.h"
#include "Components/PanelWidget.h"

namespace WidgetCommandsLocal
{

// ============================================================================
// Helpers
// ============================================================================

static UWidgetBlueprint* LoadWidgetBP(const FString& AssetPath)
{
	FString FullPath = AssetPath;
	// Ensure we have the object path (with .ClassName suffix)
	if (!FullPath.Contains(TEXT(".")))
	{
		FString AssetName = FPaths::GetBaseFilename(FullPath);
		FullPath = FullPath + TEXT(".") + AssetName;
	}

	UObject* Loaded = StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *FullPath);
	return Cast<UWidgetBlueprint>(Loaded);
}

static UWidget* FindWidgetByName(UWidgetTree* WidgetTree, const FString& Name)
{
	if (!WidgetTree) return nullptr;

	FName WidgetName(*Name);

	// Check root
	if (WidgetTree->RootWidget && WidgetTree->RootWidget->GetFName() == WidgetName)
	{
		return WidgetTree->RootWidget;
	}

	// Search all widgets
	TArray<UWidget*> AllWidgets;
	WidgetTree->GetAllWidgets(AllWidgets);
	for (UWidget* W : AllWidgets)
	{
		if (W && W->GetFName() == WidgetName)
		{
			return W;
		}
	}
	return nullptr;
}

static TSharedPtr<FJsonObject> WidgetToJson(UWidget* Widget)
{
	if (!Widget) return nullptr;

	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("name"), Widget->GetName());
	Obj->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
	Obj->SetStringField(TEXT("visibility"), UEnum::GetValueAsString(Widget->GetVisibility()));
	Obj->SetBoolField(TEXT("is_enabled"), Widget->GetIsEnabled());
	Obj->SetNumberField(TEXT("render_opacity"), Widget->GetRenderOpacity());

	// Type-specific properties
	if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
	{
		Obj->SetStringField(TEXT("text"), TextBlock->GetText().ToString());
	}
	else if (UProgressBar* Progress = Cast<UProgressBar>(Widget))
	{
		Obj->SetNumberField(TEXT("percent"), Progress->GetPercent());
	}
	else if (UCheckBox* Check = Cast<UCheckBox>(Widget))
	{
		Obj->SetBoolField(TEXT("is_checked"), Check->IsChecked());
	}
	else if (USlider* Sl = Cast<USlider>(Widget))
	{
		Obj->SetNumberField(TEXT("value"), Sl->GetValue());
		Obj->SetNumberField(TEXT("min_value"), Sl->GetMinValue());
		Obj->SetNumberField(TEXT("max_value"), Sl->GetMaxValue());
	}
	else if (UEditableTextBox* EditBox = Cast<UEditableTextBox>(Widget))
	{
		Obj->SetStringField(TEXT("text"), EditBox->GetText().ToString());
	}

	// Slot info
	if (UPanelSlot* Slot = Widget->Slot)
	{
		TSharedPtr<FJsonObject> SlotObj = MakeShared<FJsonObject>();
		SlotObj->SetStringField(TEXT("class"), Slot->GetClass()->GetName());

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			FVector2D Pos = CanvasSlot->GetPosition();
			FVector2D Size = CanvasSlot->GetSize();
			FAnchors Anchors = CanvasSlot->GetAnchors();
			FVector2D Align = CanvasSlot->GetAlignment();

			TArray<TSharedPtr<FJsonValue>> PosArr = {
				MakeShared<FJsonValueNumber>(Pos.X),
				MakeShared<FJsonValueNumber>(Pos.Y)
			};
			SlotObj->SetArrayField(TEXT("position"), PosArr);

			TArray<TSharedPtr<FJsonValue>> SizeArr = {
				MakeShared<FJsonValueNumber>(Size.X),
				MakeShared<FJsonValueNumber>(Size.Y)
			};
			SlotObj->SetArrayField(TEXT("size"), SizeArr);

			TArray<TSharedPtr<FJsonValue>> AnchorArr = {
				MakeShared<FJsonValueNumber>(Anchors.Minimum.X),
				MakeShared<FJsonValueNumber>(Anchors.Minimum.Y),
				MakeShared<FJsonValueNumber>(Anchors.Maximum.X),
				MakeShared<FJsonValueNumber>(Anchors.Maximum.Y)
			};
			SlotObj->SetArrayField(TEXT("anchors"), AnchorArr);

			TArray<TSharedPtr<FJsonValue>> AlignArr = {
				MakeShared<FJsonValueNumber>(Align.X),
				MakeShared<FJsonValueNumber>(Align.Y)
			};
			SlotObj->SetArrayField(TEXT("alignment"), AlignArr);
		}

		Obj->SetObjectField(TEXT("slot"), SlotObj);
	}

	// Children
	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		TArray<TSharedPtr<FJsonValue>> ChildArr;
		for (int32 i = 0; i < Panel->GetChildrenCount(); i++)
		{
			UWidget* Child = Panel->GetChildAt(i);
			TSharedPtr<FJsonObject> ChildJson = WidgetToJson(Child);
			if (ChildJson.IsValid())
			{
				ChildArr.Add(MakeShared<FJsonValueObject>(ChildJson));
			}
		}
		Obj->SetArrayField(TEXT("children"), ChildArr);
	}

	return Obj;
}

// Maps a string widget type to a constructed widget
static UWidget* ConstructWidgetByType(UWidgetTree* WidgetTree, const FString& WidgetType, const FString& WidgetName)
{
	FName Name(*WidgetName);

#define CONSTRUCT_TYPE(TypeStr, TypeClass) \
	if (WidgetType.Equals(TEXT(TypeStr), ESearchCase::IgnoreCase)) \
		return WidgetTree->ConstructWidget<TypeClass>(TypeClass::StaticClass(), Name);

	CONSTRUCT_TYPE("CanvasPanel", UCanvasPanel)
	CONSTRUCT_TYPE("VerticalBox", UVerticalBox)
	CONSTRUCT_TYPE("HorizontalBox", UHorizontalBox)
	CONSTRUCT_TYPE("GridPanel", UGridPanel)
	CONSTRUCT_TYPE("ScrollBox", UScrollBox)
	CONSTRUCT_TYPE("Border", UBorder)
	CONSTRUCT_TYPE("Overlay", UOverlay)
	CONSTRUCT_TYPE("SizeBox", USizeBox)
	CONSTRUCT_TYPE("WrapBox", UWrapBox)
	CONSTRUCT_TYPE("Button", UButton)
	CONSTRUCT_TYPE("TextBlock", UTextBlock)
	CONSTRUCT_TYPE("Image", UImage)
	CONSTRUCT_TYPE("ProgressBar", UProgressBar)
	CONSTRUCT_TYPE("CheckBox", UCheckBox)
	CONSTRUCT_TYPE("Slider", USlider)
	CONSTRUCT_TYPE("EditableTextBox", UEditableTextBox)
	CONSTRUCT_TYPE("Spacer", USpacer)

#undef CONSTRUCT_TYPE

	return nullptr;
}

} // namespace WidgetCommandsLocal

using namespace WidgetCommandsLocal;

// ============================================================================
// create_widget_blueprint
// ============================================================================

TSharedPtr<FJsonObject> FMCPCreateWidgetBlueprintCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Name = Params->GetStringField(TEXT("name"));
	FString Path = Params->GetStringField(TEXT("path"));
	FString RootWidgetType = Params->GetStringField(TEXT("root_widget_type"));

	if (Name.IsEmpty())
	{
		return ErrorResponse(TEXT("Widget blueprint name is required"));
	}
	if (Path.IsEmpty())
	{
		Path = TEXT("/Game/UI");
	}
	if (RootWidgetType.IsEmpty())
	{
		RootWidgetType = TEXT("CanvasPanel");
	}

	FString PackagePath = Path / Name;

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		return ErrorResponse(FString::Printf(TEXT("Failed to create package at '%s'"), *PackagePath));
	}

	// Create the Widget Blueprint using FKismetEditorUtilities
	UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(
		FKismetEditorUtilities::CreateBlueprint(
			UUserWidget::StaticClass(),
			Package,
			*Name,
			BPTYPE_Normal,
			UWidgetBlueprint::StaticClass(),
			UWidgetBlueprintGeneratedClass::StaticClass()
		)
	);

	if (!WidgetBP)
	{
		return ErrorResponse(TEXT("Failed to create Widget Blueprint"));
	}

	// Create root widget
	UWidgetTree* WidgetTree = WidgetBP->WidgetTree;
	if (WidgetTree)
	{
		UWidget* RootWidget = ConstructWidgetByType(WidgetTree, RootWidgetType, TEXT("RootPanel"));
		if (RootWidget)
		{
			WidgetTree->RootWidget = RootWidget;
		}
		else
		{
			// Default to CanvasPanel if type is invalid
			UCanvasPanel* DefaultRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootPanel"));
			WidgetTree->RootWidget = DefaultRoot;
		}
	}

	// Compile
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);

	// Register and save
	FAssetRegistryModule::AssetCreated(WidgetBP);
	Package->FullyLoad();
	Package->SetDirtyFlag(true);

	FString PackageFileName = FPackageName::LongPackageNameToFilename(
		PackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, WidgetBP, *PackageFileName, SaveArgs);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Name);
	Data->SetStringField(TEXT("asset_path"), WidgetBP->GetPathName());
	Data->SetStringField(TEXT("root_widget"), WidgetTree->RootWidget ? WidgetTree->RootWidget->GetName() : TEXT("none"));
	return SuccessResponse(Data);
}

// ============================================================================
// add_widget
// ============================================================================

TSharedPtr<FJsonObject> FMCPAddWidgetCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString WidgetType = Params->GetStringField(TEXT("widget_type"));
	FString WidgetName = Params->GetStringField(TEXT("widget_name"));
	FString ParentName = Params->GetStringField(TEXT("parent_name"));

	if (AssetPath.IsEmpty() || WidgetType.IsEmpty() || WidgetName.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_path, widget_type, and widget_name are required"));
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath);
	if (!WidgetBP)
	{
		return ErrorResponse(FString::Printf(TEXT("Widget Blueprint not found: %s"), *AssetPath));
	}

	UWidgetTree* WidgetTree = WidgetBP->WidgetTree;
	if (!WidgetTree)
	{
		return ErrorResponse(TEXT("Widget Blueprint has no WidgetTree"));
	}

	// Find parent
	UPanelWidget* ParentPanel = nullptr;
	if (ParentName.IsEmpty())
	{
		ParentPanel = Cast<UPanelWidget>(WidgetTree->RootWidget);
	}
	else
	{
		ParentPanel = Cast<UPanelWidget>(FindWidgetByName(WidgetTree, ParentName));
	}

	if (!ParentPanel)
	{
		return ErrorResponse(FString::Printf(TEXT("Parent widget not found or not a panel: %s"),
			ParentName.IsEmpty() ? TEXT("(root)") : *ParentName));
	}

	// Construct the widget
	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Add Widget")));
	WidgetBP->Modify();

	UWidget* NewWidget = ConstructWidgetByType(WidgetTree, WidgetType, WidgetName);
	if (!NewWidget)
	{
		return ErrorResponse(FString::Printf(TEXT("Unsupported widget type: %s. Supported: CanvasPanel, VerticalBox, HorizontalBox, GridPanel, ScrollBox, Border, Overlay, SizeBox, WrapBox, Button, TextBlock, Image, ProgressBar, CheckBox, Slider, EditableTextBox, Spacer"), *WidgetType));
	}

	// Add to parent
	UPanelSlot* Slot = ParentPanel->AddChild(NewWidget);
	if (!Slot)
	{
		return ErrorResponse(TEXT("Failed to add widget to parent"));
	}

	// Apply slot properties if provided
	const TSharedPtr<FJsonObject>* SlotObj;
	if (Params->TryGetObjectField(TEXT("slot"), SlotObj))
	{
		// Canvas panel slot
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			const TArray<TSharedPtr<FJsonValue>>* PosArr;
			if ((*SlotObj)->TryGetArrayField(TEXT("position"), PosArr) && PosArr->Num() >= 2)
			{
				CanvasSlot->SetPosition(FVector2D(
					(*PosArr)[0]->AsNumber(),
					(*PosArr)[1]->AsNumber()
				));
			}

			const TArray<TSharedPtr<FJsonValue>>* SizeArr;
			if ((*SlotObj)->TryGetArrayField(TEXT("size"), SizeArr) && SizeArr->Num() >= 2)
			{
				CanvasSlot->SetSize(FVector2D(
					(*SizeArr)[0]->AsNumber(),
					(*SizeArr)[1]->AsNumber()
				));
			}

			const TArray<TSharedPtr<FJsonValue>>* AnchorArr;
			if ((*SlotObj)->TryGetArrayField(TEXT("anchors"), AnchorArr) && AnchorArr->Num() >= 4)
			{
				FAnchors Anchors(
					(*AnchorArr)[0]->AsNumber(),
					(*AnchorArr)[1]->AsNumber(),
					(*AnchorArr)[2]->AsNumber(),
					(*AnchorArr)[3]->AsNumber()
				);
				CanvasSlot->SetAnchors(Anchors);
			}

			const TArray<TSharedPtr<FJsonValue>>* AlignArr;
			if ((*SlotObj)->TryGetArrayField(TEXT("alignment"), AlignArr) && AlignArr->Num() >= 2)
			{
				CanvasSlot->SetAlignment(FVector2D(
					(*AlignArr)[0]->AsNumber(),
					(*AlignArr)[1]->AsNumber()
				));
			}

			const TArray<TSharedPtr<FJsonValue>>* PaddingArr;
			if ((*SlotObj)->TryGetArrayField(TEXT("padding"), PaddingArr) && PaddingArr->Num() >= 4)
			{
				FMargin Padding(
					(*PaddingArr)[0]->AsNumber(),
					(*PaddingArr)[1]->AsNumber(),
					(*PaddingArr)[2]->AsNumber(),
					(*PaddingArr)[3]->AsNumber()
				);
				CanvasSlot->SetOffsets(Padding);
			}
		}

		// Vertical/Horizontal box slot
		if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Slot))
		{
			const TArray<TSharedPtr<FJsonValue>>* PaddingArr;
			if ((*SlotObj)->TryGetArrayField(TEXT("padding"), PaddingArr) && PaddingArr->Num() >= 4)
			{
				VSlot->SetPadding(FMargin(
					(*PaddingArr)[0]->AsNumber(),
					(*PaddingArr)[1]->AsNumber(),
					(*PaddingArr)[2]->AsNumber(),
					(*PaddingArr)[3]->AsNumber()
				));
			}

			FString HAlign;
			if ((*SlotObj)->TryGetStringField(TEXT("horizontal_alignment"), HAlign))
			{
				if (HAlign == TEXT("Fill")) VSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
				else if (HAlign == TEXT("Left")) VSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Left);
				else if (HAlign == TEXT("Center")) VSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
				else if (HAlign == TEXT("Right")) VSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Right);
			}

			FString VAlign;
			if ((*SlotObj)->TryGetStringField(TEXT("vertical_alignment"), VAlign))
			{
				if (VAlign == TEXT("Fill")) VSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
				else if (VAlign == TEXT("Top")) VSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Top);
				else if (VAlign == TEXT("Center")) VSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
				else if (VAlign == TEXT("Bottom")) VSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Bottom);
			}

			double SizeValue = 0.0;
			if ((*SlotObj)->TryGetNumberField(TEXT("size"), SizeValue))
			{
				FSlateChildSize SlotSize;
				SlotSize.SizeRule = ESlateSizeRule::Fill;
				SlotSize.Value = static_cast<float>(SizeValue);
				VSlot->SetSize(SlotSize);
			}
		}

		if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Slot))
		{
			const TArray<TSharedPtr<FJsonValue>>* PaddingArr;
			if ((*SlotObj)->TryGetArrayField(TEXT("padding"), PaddingArr) && PaddingArr->Num() >= 4)
			{
				HSlot->SetPadding(FMargin(
					(*PaddingArr)[0]->AsNumber(),
					(*PaddingArr)[1]->AsNumber(),
					(*PaddingArr)[2]->AsNumber(),
					(*PaddingArr)[3]->AsNumber()
				));
			}

			FString HAlign;
			if ((*SlotObj)->TryGetStringField(TEXT("horizontal_alignment"), HAlign))
			{
				if (HAlign == TEXT("Fill")) HSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
				else if (HAlign == TEXT("Left")) HSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Left);
				else if (HAlign == TEXT("Center")) HSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
				else if (HAlign == TEXT("Right")) HSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Right);
			}

			FString VAlign;
			if ((*SlotObj)->TryGetStringField(TEXT("vertical_alignment"), VAlign))
			{
				if (VAlign == TEXT("Fill")) HSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
				else if (VAlign == TEXT("Top")) HSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Top);
				else if (VAlign == TEXT("Center")) HSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
				else if (VAlign == TEXT("Bottom")) HSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Bottom);
			}

			double SizeValue = 0.0;
			if ((*SlotObj)->TryGetNumberField(TEXT("size"), SizeValue))
			{
				FSlateChildSize SlotSize;
				SlotSize.SizeRule = ESlateSizeRule::Fill;
				SlotSize.Value = static_cast<float>(SizeValue);
				HSlot->SetSize(SlotSize);
			}
		}
	}

	// Compile
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	WidgetBP->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("widget_name"), NewWidget->GetName());
	Data->SetStringField(TEXT("widget_type"), WidgetType);
	Data->SetStringField(TEXT("parent"), ParentPanel->GetName());
	Data->SetStringField(TEXT("slot_class"), Slot->GetClass()->GetName());
	return SuccessResponse(Data);
}

// ============================================================================
// set_widget_property
// ============================================================================

TSharedPtr<FJsonObject> FMCPSetWidgetPropertyCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString WidgetName = Params->GetStringField(TEXT("widget_name"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	FString PropertyValue = Params->GetStringField(TEXT("property_value"));

	if (AssetPath.IsEmpty() || WidgetName.IsEmpty() || PropertyName.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_path, widget_name, and property_name are required"));
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath);
	if (!WidgetBP)
	{
		return ErrorResponse(FString::Printf(TEXT("Widget Blueprint not found: %s"), *AssetPath));
	}

	UWidgetTree* WidgetTree = WidgetBP->WidgetTree;
	UWidget* Widget = FindWidgetByName(WidgetTree, WidgetName);
	if (!Widget)
	{
		return ErrorResponse(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Set Widget Property")));
	WidgetBP->Modify();

	bool bSuccess = false;
	FString ResultInfo;

	// ---- Common widget properties ----
	if (PropertyName == TEXT("Visibility"))
	{
		if (PropertyValue == TEXT("Visible")) { Widget->SetVisibility(ESlateVisibility::Visible); bSuccess = true; }
		else if (PropertyValue == TEXT("Collapsed")) { Widget->SetVisibility(ESlateVisibility::Collapsed); bSuccess = true; }
		else if (PropertyValue == TEXT("Hidden")) { Widget->SetVisibility(ESlateVisibility::Hidden); bSuccess = true; }
		else if (PropertyValue == TEXT("HitTestInvisible")) { Widget->SetVisibility(ESlateVisibility::HitTestInvisible); bSuccess = true; }
		else if (PropertyValue == TEXT("SelfHitTestInvisible")) { Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible); bSuccess = true; }
		ResultInfo = FString::Printf(TEXT("Visibility = %s"), *PropertyValue);
	}
	else if (PropertyName == TEXT("IsEnabled"))
	{
		Widget->SetIsEnabled(PropertyValue.ToBool());
		bSuccess = true;
		ResultInfo = FString::Printf(TEXT("IsEnabled = %s"), *PropertyValue);
	}
	else if (PropertyName == TEXT("ToolTipText"))
	{
		Widget->SetToolTipText(FText::FromString(PropertyValue));
		bSuccess = true;
		ResultInfo = FString::Printf(TEXT("ToolTipText = %s"), *PropertyValue);
	}
	else if (PropertyName == TEXT("RenderOpacity"))
	{
		Widget->SetRenderOpacity(FCString::Atof(*PropertyValue));
		bSuccess = true;
		ResultInfo = FString::Printf(TEXT("RenderOpacity = %s"), *PropertyValue);
	}
	// ---- TextBlock properties ----
	else if (PropertyName == TEXT("Text"))
	{
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
			TextBlock->SetText(FText::FromString(PropertyValue));
			bSuccess = true;
			ResultInfo = FString::Printf(TEXT("Text = %s"), *PropertyValue);
		}
		else if (UEditableTextBox* EditBox = Cast<UEditableTextBox>(Widget))
		{
			EditBox->SetText(FText::FromString(PropertyValue));
			bSuccess = true;
			ResultInfo = FString::Printf(TEXT("Text = %s"), *PropertyValue);
		}
		else
		{
			return ErrorResponse(TEXT("Text property is only valid for TextBlock and EditableTextBox"));
		}
	}
	else if (PropertyName == TEXT("ColorAndOpacity"))
	{
		// Parse "R,G,B,A" or "R,G,B"
		TArray<FString> Parts;
		PropertyValue.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() >= 3)
		{
			FLinearColor Color(
				FCString::Atof(*Parts[0].TrimStartAndEnd()),
				FCString::Atof(*Parts[1].TrimStartAndEnd()),
				FCString::Atof(*Parts[2].TrimStartAndEnd()),
				Parts.Num() >= 4 ? FCString::Atof(*Parts[3].TrimStartAndEnd()) : 1.0f
			);

			if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
			{
				TextBlock->SetColorAndOpacity(FSlateColor(Color));
				bSuccess = true;
			}
			else if (UButton* Button = Cast<UButton>(Widget))
			{
				Button->SetColorAndOpacity(Color);
				bSuccess = true;
			}
			else if (UImage* Img = Cast<UImage>(Widget))
			{
				Img->SetColorAndOpacity(Color);
				bSuccess = true;
			}
			ResultInfo = FString::Printf(TEXT("ColorAndOpacity = %s"), *Color.ToString());
		}
		else
		{
			return ErrorResponse(TEXT("ColorAndOpacity requires at least 3 comma-separated values (R,G,B[,A])"));
		}
	}
	else if (PropertyName == TEXT("Font.Size"))
	{
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
			FSlateFontInfo Font = TextBlock->GetFont();
			Font.Size = FCString::Atoi(*PropertyValue);
			TextBlock->SetFont(Font);
			bSuccess = true;
			ResultInfo = FString::Printf(TEXT("Font.Size = %s"), *PropertyValue);
		}
		else
		{
			return ErrorResponse(TEXT("Font.Size is only valid for TextBlock"));
		}
	}
	else if (PropertyName == TEXT("BackgroundColor"))
	{
		if (UButton* Button = Cast<UButton>(Widget))
		{
			TArray<FString> Parts;
			PropertyValue.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() >= 3)
			{
				FLinearColor Color(
					FCString::Atof(*Parts[0].TrimStartAndEnd()),
					FCString::Atof(*Parts[1].TrimStartAndEnd()),
					FCString::Atof(*Parts[2].TrimStartAndEnd()),
					Parts.Num() >= 4 ? FCString::Atof(*Parts[3].TrimStartAndEnd()) : 1.0f
				);
				Button->SetBackgroundColor(Color);
				bSuccess = true;
				ResultInfo = FString::Printf(TEXT("BackgroundColor = %s"), *Color.ToString());
			}
			else
			{
				return ErrorResponse(TEXT("BackgroundColor requires at least 3 comma-separated values (R,G,B[,A])"));
			}
		}
		else
		{
			return ErrorResponse(TEXT("BackgroundColor is only valid for Button"));
		}
	}
	// ---- ProgressBar properties ----
	else if (PropertyName == TEXT("Percent"))
	{
		if (UProgressBar* Progress = Cast<UProgressBar>(Widget))
		{
			Progress->SetPercent(FCString::Atof(*PropertyValue));
			bSuccess = true;
			ResultInfo = FString::Printf(TEXT("Percent = %s"), *PropertyValue);
		}
		else
		{
			return ErrorResponse(TEXT("Percent is only valid for ProgressBar"));
		}
	}
	else if (PropertyName == TEXT("FillColorAndOpacity"))
	{
		if (UProgressBar* Progress = Cast<UProgressBar>(Widget))
		{
			TArray<FString> Parts;
			PropertyValue.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() >= 3)
			{
				FLinearColor Color(
					FCString::Atof(*Parts[0].TrimStartAndEnd()),
					FCString::Atof(*Parts[1].TrimStartAndEnd()),
					FCString::Atof(*Parts[2].TrimStartAndEnd()),
					Parts.Num() >= 4 ? FCString::Atof(*Parts[3].TrimStartAndEnd()) : 1.0f
				);
				Progress->SetFillColorAndOpacity(Color);
				bSuccess = true;
				ResultInfo = FString::Printf(TEXT("FillColorAndOpacity = %s"), *Color.ToString());
			}
		}
		else
		{
			return ErrorResponse(TEXT("FillColorAndOpacity is only valid for ProgressBar"));
		}
	}
	// ---- CheckBox properties ----
	else if (PropertyName == TEXT("IsChecked"))
	{
		if (UCheckBox* Check = Cast<UCheckBox>(Widget))
		{
			Check->SetIsChecked(PropertyValue.ToBool());
			bSuccess = true;
			ResultInfo = FString::Printf(TEXT("IsChecked = %s"), *PropertyValue);
		}
		else
		{
			return ErrorResponse(TEXT("IsChecked is only valid for CheckBox"));
		}
	}
	// ---- Slider properties ----
	else if (PropertyName == TEXT("Value"))
	{
		if (USlider* Sl = Cast<USlider>(Widget))
		{
			Sl->SetValue(FCString::Atof(*PropertyValue));
			bSuccess = true;
			ResultInfo = FString::Printf(TEXT("Value = %s"), *PropertyValue);
		}
		else
		{
			return ErrorResponse(TEXT("Value is only valid for Slider"));
		}
	}
	else if (PropertyName == TEXT("MinValue"))
	{
		if (USlider* Sl = Cast<USlider>(Widget))
		{
			Sl->SetMinValue(FCString::Atof(*PropertyValue));
			bSuccess = true;
			ResultInfo = FString::Printf(TEXT("MinValue = %s"), *PropertyValue);
		}
		else
		{
			return ErrorResponse(TEXT("MinValue is only valid for Slider"));
		}
	}
	else if (PropertyName == TEXT("MaxValue"))
	{
		if (USlider* Sl = Cast<USlider>(Widget))
		{
			Sl->SetMaxValue(FCString::Atof(*PropertyValue));
			bSuccess = true;
			ResultInfo = FString::Printf(TEXT("MaxValue = %s"), *PropertyValue);
		}
		else
		{
			return ErrorResponse(TEXT("MaxValue is only valid for Slider"));
		}
	}
	// ---- EditableTextBox properties ----
	else if (PropertyName == TEXT("HintText"))
	{
		if (UEditableTextBox* EditBox = Cast<UEditableTextBox>(Widget))
		{
			EditBox->SetHintText(FText::FromString(PropertyValue));
			bSuccess = true;
			ResultInfo = FString::Printf(TEXT("HintText = %s"), *PropertyValue);
		}
		else
		{
			return ErrorResponse(TEXT("HintText is only valid for EditableTextBox"));
		}
	}
	// ---- Canvas slot properties ----
	else if (PropertyName == TEXT("Position"))
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			TArray<FString> Parts;
			PropertyValue.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() >= 2)
			{
				CanvasSlot->SetPosition(FVector2D(
					FCString::Atof(*Parts[0].TrimStartAndEnd()),
					FCString::Atof(*Parts[1].TrimStartAndEnd())
				));
				bSuccess = true;
				ResultInfo = FString::Printf(TEXT("Position = %s"), *PropertyValue);
			}
		}
		else
		{
			return ErrorResponse(TEXT("Position is only valid for widgets in a CanvasPanel"));
		}
	}
	else if (PropertyName == TEXT("Size"))
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			TArray<FString> Parts;
			PropertyValue.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() >= 2)
			{
				CanvasSlot->SetSize(FVector2D(
					FCString::Atof(*Parts[0].TrimStartAndEnd()),
					FCString::Atof(*Parts[1].TrimStartAndEnd())
				));
				bSuccess = true;
				ResultInfo = FString::Printf(TEXT("Size = %s"), *PropertyValue);
			}
		}
		else
		{
			return ErrorResponse(TEXT("Size is only valid for widgets in a CanvasPanel"));
		}
	}
	else if (PropertyName == TEXT("Anchors"))
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			TArray<FString> Parts;
			PropertyValue.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() >= 4)
			{
				FAnchors Anchors(
					FCString::Atof(*Parts[0].TrimStartAndEnd()),
					FCString::Atof(*Parts[1].TrimStartAndEnd()),
					FCString::Atof(*Parts[2].TrimStartAndEnd()),
					FCString::Atof(*Parts[3].TrimStartAndEnd())
				);
				CanvasSlot->SetAnchors(Anchors);
				bSuccess = true;
				ResultInfo = FString::Printf(TEXT("Anchors = %s"), *PropertyValue);
			}
		}
		else
		{
			return ErrorResponse(TEXT("Anchors is only valid for widgets in a CanvasPanel"));
		}
	}
	else if (PropertyName == TEXT("Alignment"))
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			TArray<FString> Parts;
			PropertyValue.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() >= 2)
			{
				CanvasSlot->SetAlignment(FVector2D(
					FCString::Atof(*Parts[0].TrimStartAndEnd()),
					FCString::Atof(*Parts[1].TrimStartAndEnd())
				));
				bSuccess = true;
				ResultInfo = FString::Printf(TEXT("Alignment = %s"), *PropertyValue);
			}
		}
		else
		{
			return ErrorResponse(TEXT("Alignment is only valid for widgets in a CanvasPanel"));
		}
	}
	// ---- Box slot properties ----
	else if (PropertyName == TEXT("Padding"))
	{
		if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
		{
			TArray<FString> Parts;
			PropertyValue.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() >= 4)
			{
				VSlot->SetPadding(FMargin(
					FCString::Atof(*Parts[0].TrimStartAndEnd()),
					FCString::Atof(*Parts[1].TrimStartAndEnd()),
					FCString::Atof(*Parts[2].TrimStartAndEnd()),
					FCString::Atof(*Parts[3].TrimStartAndEnd())
				));
				bSuccess = true;
			}
		}
		else if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Widget->Slot))
		{
			TArray<FString> Parts;
			PropertyValue.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() >= 4)
			{
				HSlot->SetPadding(FMargin(
					FCString::Atof(*Parts[0].TrimStartAndEnd()),
					FCString::Atof(*Parts[1].TrimStartAndEnd()),
					FCString::Atof(*Parts[2].TrimStartAndEnd()),
					FCString::Atof(*Parts[3].TrimStartAndEnd())
				));
				bSuccess = true;
			}
		}
		else if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			TArray<FString> Parts;
			PropertyValue.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() >= 4)
			{
				CanvasSlot->SetOffsets(FMargin(
					FCString::Atof(*Parts[0].TrimStartAndEnd()),
					FCString::Atof(*Parts[1].TrimStartAndEnd()),
					FCString::Atof(*Parts[2].TrimStartAndEnd()),
					FCString::Atof(*Parts[3].TrimStartAndEnd())
				));
				bSuccess = true;
			}
		}
		if (bSuccess)
		{
			ResultInfo = FString::Printf(TEXT("Padding = %s"), *PropertyValue);
		}
		else
		{
			return ErrorResponse(TEXT("Padding is only valid for widgets in a Box or CanvasPanel slot"));
		}
	}
	else if (PropertyName == TEXT("HorizontalAlignment"))
	{
		auto SetHAlignment = [&](auto* BoxSlot) -> bool
		{
			if (PropertyValue == TEXT("Fill")) { BoxSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill); return true; }
			if (PropertyValue == TEXT("Left")) { BoxSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Left); return true; }
			if (PropertyValue == TEXT("Center")) { BoxSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center); return true; }
			if (PropertyValue == TEXT("Right")) { BoxSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Right); return true; }
			return false;
		};

		if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
		{
			bSuccess = SetHAlignment(VSlot);
		}
		else if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Widget->Slot))
		{
			bSuccess = SetHAlignment(HSlot);
		}
		if (bSuccess)
		{
			ResultInfo = FString::Printf(TEXT("HorizontalAlignment = %s"), *PropertyValue);
		}
		else
		{
			return ErrorResponse(TEXT("HorizontalAlignment is only valid for widgets in Box slots"));
		}
	}
	else if (PropertyName == TEXT("VerticalAlignment"))
	{
		auto SetVAlignment = [&](auto* BoxSlot) -> bool
		{
			if (PropertyValue == TEXT("Fill")) { BoxSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill); return true; }
			if (PropertyValue == TEXT("Top")) { BoxSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Top); return true; }
			if (PropertyValue == TEXT("Center")) { BoxSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center); return true; }
			if (PropertyValue == TEXT("Bottom")) { BoxSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Bottom); return true; }
			return false;
		};

		if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
		{
			bSuccess = SetVAlignment(VSlot);
		}
		else if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Widget->Slot))
		{
			bSuccess = SetVAlignment(HSlot);
		}
		if (bSuccess)
		{
			ResultInfo = FString::Printf(TEXT("VerticalAlignment = %s"), *PropertyValue);
		}
		else
		{
			return ErrorResponse(TEXT("VerticalAlignment is only valid for widgets in Box slots"));
		}
	}

	// ---- Generic FProperty fallback ----
	if (!bSuccess)
	{
		FProperty* Prop = Widget->GetClass()->FindPropertyByName(*PropertyName);
		if (Prop)
		{
			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Widget);
			if (Prop->ImportText_Direct(*PropertyValue, ValuePtr, Widget, PPF_None))
			{
				bSuccess = true;
				ResultInfo = FString::Printf(TEXT("%s = %s (via FProperty)"), *PropertyName, *PropertyValue);
			}
			else
			{
				return ErrorResponse(FString::Printf(TEXT("Failed to parse value '%s' for property '%s'"), *PropertyValue, *PropertyName));
			}
		}
		else
		{
			return ErrorResponse(FString::Printf(TEXT("Unknown property: %s"), *PropertyName));
		}
	}

	// Compile
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	WidgetBP->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("widget"), WidgetName);
	Data->SetStringField(TEXT("property"), ResultInfo);
	return SuccessResponse(Data);
}

// ============================================================================
// get_widget_tree
// ============================================================================

TSharedPtr<FJsonObject> FMCPGetWidgetTreeCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	if (AssetPath.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_path is required"));
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath);
	if (!WidgetBP)
	{
		return ErrorResponse(FString::Printf(TEXT("Widget Blueprint not found: %s"), *AssetPath));
	}

	UWidgetTree* WidgetTree = WidgetBP->WidgetTree;
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		return ErrorResponse(TEXT("Widget Blueprint has no widget tree or root widget"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), WidgetBP->GetPathName());
	Data->SetStringField(TEXT("name"), WidgetBP->GetName());

	// Count all widgets
	TArray<UWidget*> AllWidgets;
	WidgetTree->GetAllWidgets(AllWidgets);
	Data->SetNumberField(TEXT("widget_count"), AllWidgets.Num());

	// Serialize tree
	TSharedPtr<FJsonObject> RootJson = WidgetToJson(WidgetTree->RootWidget);
	if (RootJson.IsValid())
	{
		Data->SetObjectField(TEXT("root"), RootJson);
	}

	return SuccessResponse(Data);
}

// ============================================================================
// remove_widget
// ============================================================================

TSharedPtr<FJsonObject> FMCPRemoveWidgetCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString WidgetName = Params->GetStringField(TEXT("widget_name"));

	if (AssetPath.IsEmpty() || WidgetName.IsEmpty())
	{
		return ErrorResponse(TEXT("asset_path and widget_name are required"));
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath);
	if (!WidgetBP)
	{
		return ErrorResponse(FString::Printf(TEXT("Widget Blueprint not found: %s"), *AssetPath));
	}

	UWidgetTree* WidgetTree = WidgetBP->WidgetTree;
	UWidget* Widget = FindWidgetByName(WidgetTree, WidgetName);
	if (!Widget)
	{
		return ErrorResponse(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}

	// Don't allow removing the root widget
	if (Widget == WidgetTree->RootWidget)
	{
		return ErrorResponse(TEXT("Cannot remove the root widget"));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Remove Widget")));
	WidgetBP->Modify();

	// Remove from parent first
	if (UPanelWidget* Parent = Widget->GetParent())
	{
		Parent->RemoveChild(Widget);
	}

	// Remove from widget tree
	WidgetTree->RemoveWidget(Widget);

	// Compile
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	WidgetBP->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("removed"), WidgetName);
	return SuccessResponse(Data);
}
