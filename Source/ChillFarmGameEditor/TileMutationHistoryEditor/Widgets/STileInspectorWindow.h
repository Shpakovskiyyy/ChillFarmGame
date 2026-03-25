#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/ITableRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SEditableTextBox.h"

#include "Components/VerticalBox.h"

#include "TileMutationHistory/TileMutationHistoryEntry.h"


class STileInspectorWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STileInspectorWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	
private:
	TSharedRef<SWidget> BuildTileSelectorSection();
	TSharedRef<SWidget> BuildHistorySection();
	TSharedRef<SWidget> BuildOverviewSection();

	void RefreshHistory();
	
private:
	TSharedRef<ITableRow> GenerateHistoryRow(TSharedPtr<const FTileMutationHistoryEntry> Item, const TSharedRef<STableViewBase>& OwnerTable);

private:
	void OnTileCoordinatesCommitted(const FText& InText, ETextCommit::Type InType);
	void OnSelectedTileChanged(const FIntPoint& InCoordinates);

private:
	FIntPoint SelectedTileCoordinates = FIntPoint::ZeroValue;
	
private:	
	TArray<TSharedPtr<const FTileMutationHistoryEntry>> CachedHistory;

private:
	TSharedPtr<SListView<TSharedPtr<const FTileMutationHistoryEntry>>> HistoryListView;
	TSharedPtr<SEditableTextBox> TileEditTextBox;
	
};