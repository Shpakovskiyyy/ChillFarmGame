#include "TilesVisualDataSet.h"

#include "Misc/EnumRange.h"

DEFINE_LOG_CATEGORY_STATIC(LogTileVisualDataSet, All, All)

UTilesVisualDataSet::UTilesVisualDataSet()
{
	for (auto TileType : TEnumRange<ETileType>())
	{
		if (!TileTypeVisuals.Contains(TileType)) UE_LOG(LogTileVisualDataSet, Warning, TEXT("%s - missing TileType %s in TileVisualDataSet. Fix this to enable this TileType visual representation"), *GetName(), *UEnum::GetValueAsString(TileType))
	}
}