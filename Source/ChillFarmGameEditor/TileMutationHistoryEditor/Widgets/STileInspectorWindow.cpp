#include "STileInspectorWindow.h"

#include "TileDebugSelectionEditorSubsystem.h"
#include "TileHistoryEditorSubsystem.h"
#include "TileWorldSubsystem.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TileInspector"

namespace TileInspector
{
	constexpr float Padding = 6.f;

	// Order of the rows in the overview section.
	enum EOverviewField : int32
	{
		Coordinates,
		Type,
		Biome,
		Height,
		Fertility,
		Moisture,
		Flags,
		Changes,
		Count
	};

	const ETileField AllFields[] = { ETileField::Type, ETileField::Biome, ETileField::Height, ETileField::Fertility, ETileField::Moisture, ETileField::Flags };

	FText FormatField(ETileField InField, const FTileData& InTile)
	{
		switch (InField)
		{
			case ETileField::Type:
				return InTile.IsGenerated() ? UEnum::GetDisplayValueAsText(InTile.Type) : LOCTEXT("NotGenerated", "Not generated");
			case ETileField::Biome:
				return InTile.Biome == EBiomeType::EBiomeType_MAX ? LOCTEXT("NoBiome", "None") : UEnum::GetDisplayValueAsText(InTile.Biome);
			case ETileField::Height:
				return FText::AsNumber(InTile.Height);
			case ETileField::Fertility:
			case ETileField::Moisture:
				return FText::Format(LOCTEXT("Percent", "{0}%"), FText::AsNumber(InField == ETileField::Fertility ? InTile.Fertility : InTile.Moisture));
			case ETileField::Flags:
			{
				TArray<FText> Names;
				if (InTile.HasFlag(ETileFlags::Occupied)) Names.Add(LOCTEXT("Occupied", "Occupied"));
				if (InTile.HasFlag(ETileFlags::Watered)) Names.Add(LOCTEXT("Watered", "Watered"));
				return Names.IsEmpty() ? LOCTEXT("NoFlags", "None") : FText::Join(LOCTEXT("FlagSeparator", ", "), Names);
			}
			default:
				return FText::GetEmpty();
		}
	}
}


void STileInspectorWindow::Construct(const FArguments& InArgs)
{
	OverviewValues.Init(FText::GetEmpty(), TileInspector::Count);

	ChildSlot
	[
		SNew(SScrollBox)

		+ SScrollBox::Slot().Padding(TileInspector::Padding)
		[
			BuildTileSelectorSection()
		]

		+ SScrollBox::Slot().Padding(TileInspector::Padding)
		[
			BuildOverviewSection()
		]

		+ SScrollBox::Slot().Padding(TileInspector::Padding)
		[
			BuildHistorySection()
		]
	];

	if (GEditor)
	{
		if (UTileDebugSelectionEditorSubsystem* Selection = GEditor->GetEditorSubsystem<UTileDebugSelectionEditorSubsystem>())
		{
			SelectedTile = Selection->GetSelectedTile();
			SelectionHandle = Selection->OnTileSelected.AddSP(this, &STileInspectorWindow::OnSelectedTileChanged);
		}
	}

	// Polling a version number is cheaper and simpler than following subsystems across map changes.
	RegisterActiveTimer(0.2f, FWidgetActiveTimerDelegate::CreateSP(this, &STileInspectorWindow::RefreshIfNeeded));
	OnSelectedTileChanged(SelectedTile);
}

STileInspectorWindow::~STileInspectorWindow()
{
	if (GEditor && SelectionHandle.IsValid())
	{
		if (UTileDebugSelectionEditorSubsystem* Selection = GEditor->GetEditorSubsystem<UTileDebugSelectionEditorSubsystem>())
		{
			Selection->OnTileSelected.Remove(SelectionHandle);
		}
	}
}

TSharedRef<SWidget> STileInspectorWindow::BuildTileSelectorSection()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TileCoordinatesLabel", "Tile (X, Y):"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(4.f, 0.f)
		[
			SAssignNew(TileEditTextBox, SEditableTextBox)
			.HintText(LOCTEXT("TileCoordinatesHint", "Click a tile in the viewport or type 12,34"))
			.OnTextCommitted(this, &STileInspectorWindow::OnTileCoordinatesCommitted)
		];
}

TSharedRef<SWidget> STileInspectorWindow::BuildOverviewSection()
{
	const TPair<TileInspector::EOverviewField, FText> Labels[] = {
		{ TileInspector::Coordinates, LOCTEXT("CoordinatesLabel", "Coordinates") },
		{ TileInspector::Type,        LOCTEXT("TypeLabel", "Type") },
		{ TileInspector::Biome,       LOCTEXT("BiomeLabel", "Biome") },
		{ TileInspector::Height,      LOCTEXT("HeightLabel", "Height level") },
		{ TileInspector::Fertility,   LOCTEXT("FertilityLabel", "Fertility") },
		{ TileInspector::Moisture,    LOCTEXT("MoistureLabel", "Moisture") },
		{ TileInspector::Flags,       LOCTEXT("FlagsLabel", "Flags") },
		{ TileInspector::Changes,     LOCTEXT("ChangesLabel", "Recorded changes") },
	};

	TSharedRef<SGridPanel> Grid = SNew(SGridPanel).FillColumn(1, 1.f);
	for (int32 Row = 0; Row < static_cast<int32>(UE_ARRAY_COUNT(Labels)); ++Row)
	{
		const int32 FieldIndex = Labels[Row].Key;

		Grid->AddSlot(0, Row).Padding(2.f, 1.f, 12.f, 1.f)
		[
			SNew(STextBlock).Text(Labels[Row].Value).ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
		Grid->AddSlot(1, Row).Padding(2.f, 1.f)
		[
			SNew(STextBlock).Text_Lambda([this, FieldIndex]() { return GetOverviewValue(FieldIndex); })
		];
	}

	return SNew(SExpandableArea)
		.InitiallyCollapsed(false)
		.HeaderContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("OverviewHeader", "Tile"))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		]
		.BodyContent()
		[
			Grid
		];
}

TSharedRef<SWidget> STileInspectorWindow::BuildHistorySection()
{
	return SNew(SExpandableArea)
		.InitiallyCollapsed(false)
		.HeaderContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HistoryHeader", "History (oldest first)"))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		]
		.BodyContent()
		[
			SAssignNew(HistoryListView, SListView<TSharedPtr<FTileInspectorHistoryRow>>)
			.ListItemsSource(&HistoryRows)
			.SelectionMode(ESelectionMode::None)
			.OnGenerateRow(this, &STileInspectorWindow::GenerateHistoryRow)
		];
}

TSharedRef<ITableRow> STileInspectorWindow::GenerateHistoryRow(TSharedPtr<FTileInspectorHistoryRow> InRow, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SVerticalBox> Changes = SNew(SVerticalBox);
	for (const FText& FieldChange : InRow->FieldChanges)
	{
		Changes->AddSlot().AutoHeight().Padding(16.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock).Text(FieldChange)
		];
	}

	return SNew(STableRow<TSharedPtr<FTileInspectorHistoryRow>>, OwnerTable)
	.Padding(FMargin(0.f, 3.f))
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(InRow->Header)
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		]

		+ SVerticalBox::Slot().AutoHeight()
		[
			Changes
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(16.f, 1.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(InRow->Instigator)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
	];
}

EActiveTimerReturnType STileInspectorWindow::RefreshIfNeeded(double InCurrentTime, float InDeltaTime)
{
	UWorld* World = GetEditorWorld();
	const UTileHistoryEditorSubsystem* History = World ? World->GetSubsystem<UTileHistoryEditorSubsystem>() : nullptr;
	const uint32 HistoryVersion = History ? History->GetVersion() : 0;

	if (CachedWorld.Get() != World || CachedTile != SelectedTile || CachedHistoryVersion != HistoryVersion)
	{
		Refresh();
	}
	return EActiveTimerReturnType::Continue;
}

void STileInspectorWindow::Refresh()
{
	UWorld* World = GetEditorWorld();
	const UTileWorldSubsystem* TileWorld = World ? World->GetSubsystem<UTileWorldSubsystem>() : nullptr;
	const UTileHistoryEditorSubsystem* History = World ? World->GetSubsystem<UTileHistoryEditorSubsystem>() : nullptr;

	CachedWorld = World;
	CachedTile = SelectedTile;
	CachedHistoryVersion = History ? History->GetVersion() : 0;

	OverviewValues.Init(LOCTEXT("NoValue", "-"), TileInspector::Count);
	HistoryRows.Reset();

	const FTileData* Tile = TileWorld ? TileWorld->GetGrid().FindTile(SelectedTile) : nullptr;
	if (SelectedTile != FIntPoint::NoneValue)
	{
		OverviewValues[TileInspector::Coordinates] = FText::Format(LOCTEXT("CoordinatesValue", "({0}, {1})"), FText::AsNumber(SelectedTile.X), FText::AsNumber(SelectedTile.Y));
	}

	if (Tile)
	{
		OverviewValues[TileInspector::Type] = TileInspector::FormatField(ETileField::Type, *Tile);
		OverviewValues[TileInspector::Biome] = TileInspector::FormatField(ETileField::Biome, *Tile);
		OverviewValues[TileInspector::Height] = TileInspector::FormatField(ETileField::Height, *Tile);
		OverviewValues[TileInspector::Fertility] = TileInspector::FormatField(ETileField::Fertility, *Tile);
		OverviewValues[TileInspector::Moisture] = TileInspector::FormatField(ETileField::Moisture, *Tile);
		OverviewValues[TileInspector::Flags] = TileInspector::FormatField(ETileField::Flags, *Tile);
	}
	else if (SelectedTile != FIntPoint::NoneValue)
	{
		OverviewValues[TileInspector::Type] = LOCTEXT("OutsideMap", "Outside the map, or the world is not generated");
	}

	if (History && Tile)
	{
		const TConstArrayView<FTileChange> Changes = History->GetHistory(SelectedTile);
		for (int32 Index = 0; Index < Changes.Num(); ++Index)
		{
			HistoryRows.Add(MakeHistoryRow(Index + 1, Changes[Index]));
		}
		OverviewValues[TileInspector::Changes] = FText::AsNumber(Changes.Num());
	}

	if (HistoryListView.IsValid()) HistoryListView->RequestListRefresh();
}

FText STileInspectorWindow::GetOverviewValue(int32 InFieldIndex) const
{
	return OverviewValues.IsValidIndex(InFieldIndex) ? OverviewValues[InFieldIndex] : FText::GetEmpty();
}

TSharedPtr<FTileInspectorHistoryRow> STileInspectorWindow::MakeHistoryRow(int32 InIndex, const FTileChange& InChange)
{
	TSharedPtr<FTileInspectorHistoryRow> Row = MakeShared<FTileInspectorHistoryRow>();
	Row->Index = InIndex;

	const FTileChangeContext& Context = InChange.Context;
	const FText Source = UEnum::GetDisplayValueAsText(Context.Source);
	const FText Step = Context.Step.IsNone() ? LOCTEXT("UnknownStep", "Unknown step") : FText::FromName(Context.Step);

	Row->Header = Context.Reason.IsNone()
		? FText::Format(LOCTEXT("HeaderNoReason", "#{0}  {1} / {2}"), FText::AsNumber(InIndex), Source, Step)
		: FText::Format(LOCTEXT("HeaderWithReason", "#{0}  {1} / {2}: {3}"), FText::AsNumber(InIndex), Source, Step, FText::FromName(Context.Reason));

	for (const ETileField Field : TileInspector::AllFields)
	{
		if (!EnumHasAnyFlags(InChange.ChangedFields, Field)) continue;

		Row->FieldChanges.Add(FText::Format(LOCTEXT("FieldChange", "{0}: {1}  ->  {2}"),
			FText::FromString(TileData::GetFieldName(Field)),
			TileInspector::FormatField(Field, InChange.Before),
			TileInspector::FormatField(Field, InChange.After)));
	}

	const UObject* Instigator = Context.Instigator.Get();
	Row->Instigator = Instigator
		? FText::Format(LOCTEXT("ByInstigator", "By {0}"), FText::FromString(Instigator->GetName()))
		: (Context.Instigator.IsExplicitlyNull() ? FText::GetEmpty() : LOCTEXT("ByDestroyed", "By an object that no longer exists"));

	return Row;
}

void STileInspectorWindow::OnTileCoordinatesCommitted(const FText& InText, ETextCommit::Type InType)
{
	if (InType != ETextCommit::OnEnter && InType != ETextCommit::OnUserMovedFocus) return;

	TArray<FString> Parts;
	InText.ToString().ParseIntoArray(Parts, TEXT(","), true);
	if (Parts.Num() != 2 || !Parts[0].TrimStartAndEnd().IsNumeric() || !Parts[1].TrimStartAndEnd().IsNumeric()) return;

	const FIntPoint Tile(FCString::Atoi(*Parts[0].TrimStartAndEnd()), FCString::Atoi(*Parts[1].TrimStartAndEnd()));

	if (GEditor)
	{
		if (UTileDebugSelectionEditorSubsystem* Selection = GEditor->GetEditorSubsystem<UTileDebugSelectionEditorSubsystem>())
		{
			// Goes through the selection subsystem so the viewport highlight follows the typed tile too.
			Selection->SelectTile(GetEditorWorld(), Tile);
			return;
		}
	}

	OnSelectedTileChanged(Tile);
}

void STileInspectorWindow::OnSelectedTileChanged(const FIntPoint& InCoordinates)
{
	SelectedTile = InCoordinates;

	if (TileEditTextBox.IsValid())
	{
		TileEditTextBox->SetText(SelectedTile == FIntPoint::NoneValue
			? FText::GetEmpty()
			: FText::FromString(FString::Printf(TEXT("%d,%d"), SelectedTile.X, SelectedTile.Y)));
	}

	Refresh();
}

UWorld* STileInspectorWindow::GetEditorWorld()
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

#undef LOCTEXT_NAMESPACE
