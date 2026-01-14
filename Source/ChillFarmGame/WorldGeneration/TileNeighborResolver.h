#pragma once

#include "TileVisualKey.h"

struct FTileNeighborResolver
{
	static FTileVisualKey Resolve(ETileType TileType, uint8 NeighborMask);
};