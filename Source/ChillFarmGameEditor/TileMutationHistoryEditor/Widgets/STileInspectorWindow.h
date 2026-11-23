#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"

class SEditableTextBox;
class UWorld;
struct FTileChange;


// One history entry, already formatted for display.
struct FTileInspectorHistoryRow
{
	int32 Index = 0;
	FText Header;
	FText Instigator;
	TArray<FText> FieldChanges;
};


// Dockable editor tab: shows the selected tile's current data and every change that led to it.
class STileInspectorWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STileInspectorWindow) {}
	SLATE_END_ARGS()

	// Nomad tab id. The module registers the spawner under it; the selection subsystem opens it on a tile click.
	inline static const FName TabName = TEXT("Tile Inspector");

	void Construct(const FArguments& InArgs);
	virtual ~STileInspectorWindow() override;

private:
	TSharedRef<SWidget> BuildTileSelectorSection();
	TSharedRef<SWidget> BuildOverviewSection();
	TSharedRef<SWidget> BuildHistorySection();

	TSharedRef<ITableRow> GenerateHistoryRow(TSharedPtr<FTileInspectorHistoryRow> InRow, const TSharedRef<STableViewBase>& OwnerTable);

	// Rebuilds the cached text when the tile, the world or the recorded history changed.
	EActiveTimerReturnType RefreshIfNeeded(double InCurrentTime, float InDeltaTime);
	void Refresh();

	FText GetOverviewValue(int32 InFieldIndex) const;

	void OnTileCoordinatesCommitted(const FText& InText, ETextCommit::Type InType);
	void OnSelectedTileChanged(const FIntPoint& InCoordinates);

	static UWorld* GetEditorWorld();
	static TSharedPtr<FTileInspectorHistoryRow> MakeHistoryRow(int32 InIndex, const FTileChange& InChange);

private:
	FIntPoint SelectedTile = FIntPoint::NoneValue;

	// What the cached data was built from. Any difference triggers a refresh.
	TWeakObjectPtr<UWorld> CachedWorld;
	FIntPoint CachedTile = FIntPoint::NoneValue;
	uint32 CachedHistoryVersion = MAX_uint32;

	TArray<FText> OverviewValues;
	TArray<TSharedPtr<FTileInspectorHistoryRow>> HistoryRows;

	TSharedPtr<SListView<TSharedPtr<FTileInspectorHistoryRow>>> HistoryListView;
	TSharedPtr<SEditableTextBox> TileEditTextBox;

	FDelegateHandle SelectionHandle;
};
