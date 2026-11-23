#include "TileGrid.h"


FTileGrid::FTileGrid(const FTileGridLayout& InLayout)
	: Layout(InLayout)
{
	Tiles.SetNum(Layout.NumTiles());
}

const FTileData& FTileGrid::GetTile(const FIntPoint& InTile) const
{
	check(Contains(InTile));
	return Tiles[Layout.ToIndex(InTile)];
}

const FTileData* FTileGrid::FindTile(const FIntPoint& InTile) const
{
	return Contains(InTile) ? &Tiles[Layout.ToIndex(InTile)] : nullptr;
}

ETileField FTileGrid::ApplyMutation(const FIntPoint& InTile, const FTileMutation& InMutation, FTileData* OutBefore)
{
	if (!Contains(InTile) || InMutation.IsEmpty()) return ETileField::None;

	FTileData& Tile = Tiles[Layout.ToIndex(InTile)];
	if (OutBefore) *OutBefore = Tile;

	return InMutation.ApplyTo(Tile);
}

bool FTileGrid::HasNeighborOfType(const FIntPoint& InTile, ETileType InType) const
{
	for (const FIntPoint& Offset : GetCardinalOffsets())
	{
		const FTileData* Neighbor = FindTile(InTile + Offset);
		if (Neighbor && Neighbor->Type == InType) return true;
	}
	return false;
}

const FIntPoint (&FTileGrid::GetCardinalOffsets())[4]
{
	static const FIntPoint Offsets[4] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
	return Offsets;
}
