#include "TileNeighborResolver.h"
#include "TileVisualKey.h"
#include "TileGrid.h"


uint8 FTileNeighborResolver::ComputeSameTypeMask(const FTileGrid& InGrid, const FIntPoint& InTile)
{
	const FTileData* Center = InGrid.FindTile(InTile);
	if (!Center) return 0;

	// Y grows towards the south, matching the order the grid is stored in.
	static const TPair<FIntPoint, ETileNeighborSide> Neighbors[] = {
		{ FIntPoint( 0, -1), ETileNeighborSide::N  },
		{ FIntPoint( 1, -1), ETileNeighborSide::NE },
		{ FIntPoint( 1,  0), ETileNeighborSide::E  },
		{ FIntPoint( 1,  1), ETileNeighborSide::SE },
		{ FIntPoint( 0,  1), ETileNeighborSide::S  },
		{ FIntPoint(-1,  1), ETileNeighborSide::SW },
		{ FIntPoint(-1,  0), ETileNeighborSide::W  },
		{ FIntPoint(-1, -1), ETileNeighborSide::NW },
	};

	uint8 Mask = 0;
	for (const auto& [Offset, Side] : Neighbors)
	{
		const FTileData* Neighbor = InGrid.FindTile(InTile + Offset);
		// A tile of the same type on another level is cut off by a cliff, so it does not continue the surface.
		if (Neighbor && Neighbor->Type == Center->Type && Neighbor->Height == Center->Height) Mask |= static_cast<uint8>(Side);
	}
	return Mask;
}


FTileVisualKey FTileNeighborResolver::Resolve(ETileType TileType, uint8 NeighborMask)
{
	const auto Has = [NeighborMask](ETileNeighborSide Side)
	{
		return (NeighborMask & static_cast<uint8>(Side)) != 0;
	};

	const bool N = Has(ETileNeighborSide::N);
	const bool E = Has(ETileNeighborSide::E);
	const bool S = Has(ETileNeighborSide::S);
	const bool W = Has(ETileNeighborSide::W);

	// The shape is decided by how many of the four sides touch a tile of the same kind.
	// Checking the count first keeps every case reachable: the previous version tested single
	// missing sides before pairs, so Corner and InnerCorner could never be returned.
	const int32 SameSides = (N ? 1 : 0) + (E ? 1 : 0) + (S ? 1 : 0) + (W ? 1 : 0);

	switch (SameSides)
	{
		case 4:
		{
			// Fully surrounded. If exactly one diagonal differs this is the inside of a corner.
			const bool NE = Has(ETileNeighborSide::NE);
			const bool SE = Has(ETileNeighborSide::SE);
			const bool SW = Has(ETileNeighborSide::SW);
			const bool NW = Has(ETileNeighborSide::NW);

			const int32 SameDiagonals = (NE ? 1 : 0) + (SE ? 1 : 0) + (SW ? 1 : 0) + (NW ? 1 : 0);
			if (SameDiagonals == 3)
			{
				if (!NE) return { TileType, ETileVisualPositionType::InnerCorner,   0 };
				if (!SE) return { TileType, ETileVisualPositionType::InnerCorner,  90 };
				if (!SW) return { TileType, ETileVisualPositionType::InnerCorner, 180 };
				return { TileType, ETileVisualPositionType::InnerCorner, 270 };
			}

			// Two or more open diagonals have no dedicated piece in the current visual set.
			return { TileType, ETileVisualPositionType::Center, 0 };
		}

		case 3:
		{
			// Exactly one open side: an edge facing that side.
			if (!N) return { TileType, ETileVisualPositionType::Edge,   0 };
			if (!E) return { TileType, ETileVisualPositionType::Edge,  90 };
			if (!S) return { TileType, ETileVisualPositionType::Edge, 180 };
			return { TileType, ETileVisualPositionType::Edge, 270 };
		}

		case 2:
		{
			// Two adjacent open sides: an outer corner.
			if (!N && !E) return { TileType, ETileVisualPositionType::Corner,   0 };
			if (!E && !S) return { TileType, ETileVisualPositionType::Corner,  90 };
			if (!S && !W) return { TileType, ETileVisualPositionType::Corner, 180 };
			if (!W && !N) return { TileType, ETileVisualPositionType::Corner, 270 };

			// Two opposite open sides: a one tile wide strip. There is no piece for it yet.
			return { TileType, ETileVisualPositionType::Isolated, 0 };
		}

		default:
			// No matching side (a single tile) or one matching side (the tip of a strip).
			return { TileType, ETileVisualPositionType::Isolated, 0 };
	}
}
