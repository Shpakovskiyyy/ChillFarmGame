#include "BiomeTypeData.h"

#include "TileData.h"


bool UBiomeTypeData::IsShore(float Height) const
{
	return bAllowWater &&
		Height < ShoreHeight &&
		Height > (ShoreHeight - ShoreWidth);
}

ETileType UBiomeTypeData::ResolveBorderTile(float Height, ETileType BaseTileType, float WaterLevel) const
{
	if (BaseTileType == ETileType::Water)
		return BaseTileType;
	
	if (bAllowWater && Height < WaterLevel)
		return ETileType::Sand;
	
	if (BaseTileType == ETileType::Grass)
		return ETileType::Dirt;

	return BaseTileType;
}

ETileType UBiomeTypeData::ResolveSmoothingOverride(ETileType InTileType) const
{
	if (const auto* Found = SmoothingOverrides.Find(InTileType))
		return *Found;

	return DefaultSmoothingOverride;
}

void UBiomeTypeData::FillRuntimeTileData(FTileData& OutTileData, float Height, float Moisture) const
{
	//OutTileData.Moisture = Moisture;
}