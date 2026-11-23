#pragma once

#include "CoreMinimal.h"
#include "TileData.h"

class FTileGrid;


// Where a character can walk on the tile map, from the tile data alone.
//
// A step goes to one of the four direct neighbours. Both tiles must be generated land (not water),
// and their surfaces must meet at the same height on at least one end of the edge they share (see
// FTileData::Slope for corner heights). Flat tiles on one level always meet; a slope or ramp meets
// the higher ground it rises to; flat tiles a level apart do not (a cliff).
//
// One shared corner is enough on purpose: beside a ramp, or where a slope runs out into a cliff, the
// drop next to the shared corner is lower than a step, so the character can walk there.
namespace TileReachability
{
	CHILLFARMGAME_API bool IsWalkable(const FTileData& InTile);

	// Whether a character can step from From to its neighbour To at the given cardinal offset.
	// Symmetric: walking down a slope is the same connection as walking up.
	CHILLFARMGAME_API bool CanStep(const FTileData& From, const FTileData& To, const FIntPoint& InOffset);

	// Labels every tile with the walkable area it belongs to (flood fill over CanStep). Water and tiles
	// that are not generated get INDEX_NONE. Areas are numbered in the order their first tile appears
	// in the grid, so the labelling is deterministic. Returns the number of areas.
	CHILLFARMGAME_API int32 ComputeWalkRegions(const FTileGrid& InGrid, TArray<int32>& OutRegions);

	// Same labelling, but levels are ignored: every piece of land joined to other land, cliffs or not.
	// Only water (and the map edge) separates islands. Returns the number of islands.
	CHILLFARMGAME_API int32 ComputeLandIslands(const FTileGrid& InGrid, TArray<int32>& OutIslands);
}
