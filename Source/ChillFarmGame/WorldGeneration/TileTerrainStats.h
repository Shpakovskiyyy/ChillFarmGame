#pragma once

#include "CoreMinimal.h"

class FTileGrid;


// Numbers that describe how a generated map plays, for tuning generation settings (see the terrain
// report test) and, later, for checking that every map has enough room for a farm.
//
// "Flat" means walkable land with no raised corner (FTileData::Slope is 0). A flat area is a group of
// flat tiles on the same level joined through their four direct neighbours.
struct FTileTerrainStats
{
	// Generated tiles that are not water.
	int32 LandTiles = 0;

	// Land that is flat.
	int32 FlatTiles = 0;

	// Land above level 0.
	int32 RaisedTiles = 0;

	// Pairs of neighbouring land tiles a character cannot step between (cliffs, see TileReachability).
	int32 CliffEdges = 0;

	int32 FlatAreas = 0;

	// Flat areas that hold a square plot of at least the requested size: how many different places
	// a farm could go.
	int32 FarmSites = 0;

	// How many non-overlapping plots of the requested size fit on flat land, packed row by row from
	// the north-west: how much room there is for farming. A big plain counts once as a site but
	// holds many plots.
	int32 FarmPlots = 0;

	// Side of the largest flat square on one level, anywhere on the map.
	int32 LargestFlatSquare = 0;

	// Average distance, in tiles, from a flat tile to the nearest tile outside its flat area (a drop,
	// a slope, water or the map edge). 1 for a tile on the border of its area. Roughly half the width
	// of a typical terrace.
	float MeanEdgeDistance = 0.f;

	float FlatShare() const { return LandTiles > 0 ? static_cast<float>(FlatTiles) / LandTiles : 0.f; }
	float RaisedShare() const { return LandTiles > 0 ? static_cast<float>(RaisedTiles) / LandTiles : 0.f; }
};


namespace TileTerrainStats
{
	// Measures a grid. InPlotSize is the side of the square plot a farm site must hold.
	CHILLFARMGAME_API FTileTerrainStats Compute(const FTileGrid& InGrid, int32 InPlotSize = 12);
}
