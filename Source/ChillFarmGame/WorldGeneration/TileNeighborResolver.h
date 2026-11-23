#pragma once

#include "CoreMinimal.h"
#include "TileVisualKey.h"

class FTileGrid;


// Picks the autotile piece (centre, edge, corner...) and its rotation for a tile.
//
// Visual only: the neighbour mask is derived from the grid when visuals are built and is not stored
// in FTileData, so it can never be stale after a tile changes.
struct CHILLFARMGAME_API FTileNeighborResolver
{
	// Bitmask of ETileNeighborSide for the eight neighbours that have the same type as the tile.
	static uint8 ComputeSameTypeMask(const FTileGrid& InGrid, const FIntPoint& InTile);

	static FTileVisualKey Resolve(ETileType TileType, uint8 NeighborMask);
};
