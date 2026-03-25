#include "STileInspectorWindow.h"

#include "TileMutationHistory/TileMutationHistorySubsystem.h"
#include "TileDebugSelectionEditorSubsystem.h"

#include "BiomeTypeData.h"

#include "Editor.h"

#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SGridPanel.h"


static constexpr int32 sDefaultWidgetPadding = 6;

#define LOCTEXT_NAMESPACE "TileInspector"


void STileInspectorWindow::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SScrollBox)
		
		+ SScrollBox::Slot()
		.Padding(sDefaultWidgetPadding)
		[
			BuildTileSelectorSection()
		]
		
		+ SScrollBox::Slot()
		.Padding(6)
		[
			BuildOverviewSection()
		]
		
		+ SScrollBox::Slot()
		.Padding(sDefaultWidgetPadding)
		[
			BuildHistorySection()
		]
	];

	if(!GEditor) return;
	
	if (auto* SelectionSubsystem = GEditor->GetEditorSubsystem<UTileDebugSelectionEditorSubsystem>())
	{
		SelectionSubsystem->OnTileSelected.AddSP(this, &STileInspectorWindow::OnSelectedTileChanged);
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
			.Text(LOCTEXT("TileCoordinatesLabel", "Tile Grid-Space Coordinates (X,Y):"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(4, 0)
		[
			SAssignNew(TileEditTextBox, SEditableTextBox)
			.Text(FText::FromString(TEXT("0,0")))
			.OnTextCommitted(this, &STileInspectorWindow::OnTileCoordinatesCommitted)
		];
}

TSharedRef<SWidget> STileInspectorWindow::BuildHistorySection()
{
	return SNew(SExpandableArea)
		.InitiallyCollapsed(false)
		.HeaderContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HistoryHeader", "Mutation History"))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		]
		.BodyContent()
		[
			SAssignNew(HistoryListView,
				SListView<TSharedPtr<const FTileMutationHistoryEntry>>)
			.ListItemsSource(&CachedHistory)
			.SelectionMode(ESelectionMode::None)
			.OnGenerateRow(this, &STileInspectorWindow::GenerateHistoryRow)
		];
}

TSharedRef<SWidget> STileInspectorWindow::BuildOverviewSection()
{
	return SNew(SExpandableArea)
		.InitiallyCollapsed(false)
		.HeaderContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("OverviewHeader", "Tile Overview"))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		]
		.BodyContent()
		[
			SNew(SGridPanel)
			.FillColumn(1, 1.f)
			
			+ SGridPanel::Slot(0, 0).Padding(2)
			[
				SNew(STextBlock).Text(LOCTEXT("CoordLabel", "Coordinates"))
			]
			+ SGridPanel::Slot(1, 0).Padding(2)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return FText::FromString(
						FString::Printf(
							TEXT("(%d, %d)"),
							SelectedTileCoordinates.X,
							SelectedTileCoordinates.Y
						));
				})
			]
			
			+ SGridPanel::Slot(0, 1).Padding(2)
			[
				SNew(STextBlock).Text(LOCTEXT("TypeLabel", "Tile Type"))
			]
			+ SGridPanel::Slot(1, 1).Padding(2)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TypeValue", "<todo>"))
			]
		];
}

void STileInspectorWindow::RefreshHistory()
{
	CachedHistory.Reset();

	if (!GEditor) return;
	
	auto World = GEditor->GetEditorWorldContext().World();
	if (!World) return;
	if ( auto HistorySubsystem = World->GetSubsystem<UTileMutationHistorySubsystem>() )
	{
		CachedHistory = HistorySubsystem->GetHistory(SelectedTileCoordinates);
	}

	if (HistoryListView.IsValid()) HistoryListView->RequestListRefresh();
}

TSharedRef<ITableRow> STileInspectorWindow::GenerateHistoryRow(TSharedPtr<const FTileMutationHistoryEntry> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(Item.IsValid());

	// TODO Make a useful list with expandable categories, add flags and before and after comparison
	return SNew(STableRow<TSharedPtr<const FTileMutationHistoryEntry>>, OwnerTable)
	[
		SNew(SVerticalBox)
		
		+SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("Instigator: %s"),
					*FString(Item->Instigator->GetName())
					)
				)
			)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(
				FString::Printf(
					TEXT("[%s] %s"),
					*UEnum::GetValueAsString(Item->Source),
					*Item->Phase.ToString()
				)
			))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromName(Item->Reason))
			.WrapTextAt(500.f)
		]
	];
}

void STileInspectorWindow::OnTileCoordinatesCommitted(const FText& InText, ETextCommit::Type InType)
{
	int32 X = 0, Y = 0;

	if (FString(InText.ToString()).Split(TEXT(","), nullptr, nullptr))
	{
		TArray<FString> Parts;
		InText.ToString().ParseIntoArray(Parts, TEXT(","), true);

		if (Parts.Num() == 2)
		{
			X = FCString::Atoi(*Parts[0]);
			Y = FCString::Atoi(*Parts[1]);

			SelectedTileCoordinates = FIntPoint(X, Y);
			RefreshHistory();
		}
	}
}

void STileInspectorWindow::OnSelectedTileChanged(const FIntPoint& InCoordinates)
{
	SelectedTileCoordinates = InCoordinates;
	auto NewCoordinatesText = FText::FromString(FString::Printf(TEXT("%d,%d"), SelectedTileCoordinates.X, SelectedTileCoordinates.Y));
	TileEditTextBox->SetText(NewCoordinatesText);
	RefreshHistory();
}

#undef LOCTEXT_NAMESPACE