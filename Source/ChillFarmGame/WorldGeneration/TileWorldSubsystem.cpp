#include "TileWorldSubsystem.h"


void UTileWorldSubsystem::Deinitialize()
{
	Grid = FTileGrid();
	Super::Deinitialize();
}

const FTileData* UTileWorldSubsystem::FindTileAt(const FVector& InWorldLocation, FIntPoint* OutCoordinates) const
{
	const FIntPoint Coordinates = Grid.GetLayout().WorldToTile(InWorldLocation);
	if (OutCoordinates) *OutCoordinates = Coordinates;
	return Grid.FindTile(Coordinates);
}

void UTileWorldSubsystem::ClearWorld()
{
	Grid = FTileGrid();
	OnWorldCleared.Broadcast();
}

void UTileWorldSubsystem::SetGrid(FTileGrid&& InGrid)
{
	Grid = MoveTemp(InGrid);
	OnWorldRebuilt.Broadcast();
}

ETileField UTileWorldSubsystem::MutateTile(const FIntPoint& InTile, const FTileMutation& InMutation, const FTileChangeContext& InContext)
{
	FTileChange Change;
	Change.ChangedFields = Grid.ApplyMutation(InTile, InMutation, &Change.Before);
	if (Change.ChangedFields == ETileField::None) return ETileField::None;

	Change.Coordinates = InTile;
	Change.After = Grid.GetTile(InTile);
	Change.Context = InContext;

	OnTileChanged.Broadcast(Change);

	const FIntPoint Changed[] = { InTile };
	OnTilesChanged.Broadcast(TConstArrayView<FIntPoint>(Changed));

	return Change.ChangedFields;
}

int32 UTileWorldSubsystem::MutateTiles(TConstArrayView<FIntPoint> InTiles, const FTileMutation& InMutation, const FTileChangeContext& InContext)
{
	TArray<FIntPoint> Changed;
	for (const FIntPoint& Tile : InTiles)
	{
		FTileChange Change;
		Change.ChangedFields = Grid.ApplyMutation(Tile, InMutation, &Change.Before);
		if (Change.ChangedFields == ETileField::None) continue;

		Change.Coordinates = Tile;
		Change.After = Grid.GetTile(Tile);
		Change.Context = InContext;
		OnTileChanged.Broadcast(Change);
		Changed.Add(Tile);
	}

	if (!Changed.IsEmpty()) OnTilesChanged.Broadcast(TConstArrayView<FIntPoint>(Changed));
	return Changed.Num();
}

void UTileWorldSubsystem::NotifyExternalChange(const FTileChange& InChange) const
{
	OnTileChanged.Broadcast(InChange);
}
