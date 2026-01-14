#include "TileNeighborResolver.h"
#include "TileVisualKey.h"


FTileVisualKey FTileNeighborResolver::Resolve(ETileType TileType, uint8 NeighborMask)
{
	auto Has = [&](ETileNeighborSide N)
	{
		return NeighborMask & static_cast<uint8>(N);
	};

	auto N = (bool)Has(ETileNeighborSide::N);
	auto E = (bool)Has(ETileNeighborSide::E);
	auto S = (bool)Has(ETileNeighborSide::S);
	auto W = (bool)Has(ETileNeighborSide::W);
	
	if (!N && !E && !S && !W)
		return { TileType, ETileVisualPositionType::Isolated, 0 };
	
	if (N && E && S && W)
		return { TileType, ETileVisualPositionType::Center, 0 };
	
	if (!N) return { TileType, ETileVisualPositionType::Edge,   0 };
	if (!E) return { TileType, ETileVisualPositionType::Edge,  90 };
	if (!S) return { TileType, ETileVisualPositionType::Edge, 180 };
	if (!W) return { TileType, ETileVisualPositionType::Edge, 270 };
	
	if (!N && !E) return { TileType, ETileVisualPositionType::Corner,   0 };
	if (!E && !S) return { TileType, ETileVisualPositionType::Corner,  90 };
	if (!S && !W) return { TileType, ETileVisualPositionType::Corner, 180 };
	if (!W && !N) return { TileType, ETileVisualPositionType::Corner, 270 };
	
	if (N && E && !Has(ETileNeighborSide::NE))
		return { TileType, ETileVisualPositionType::InnerCorner,   0 };

	return { TileType, ETileVisualPositionType::Center, 0 };
}