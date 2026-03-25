#pragma once

#include "CoreMinimal.h"
#include "ETileType.h"
#include "TileData.h"
#include "TileMutationHelper.generated.h"

struct FTileData;
class UTileDataGrid;
class UBiomeTypeData;


#define DEFINE_TILE_PROPERTY_MUTATOR(PropertyType, PropertyName, PropertyDefaultValue) \
public: \
FORCEINLINE void Set##PropertyName(PropertyType In##PropertyName) \
{ \
	New##PropertyName = In##PropertyName; \
	b##PropertyName##Changed = true; \
} \
private: \
UPROPERTY() \
bool b##PropertyName##Changed = false; \
UPROPERTY() \
PropertyType New##PropertyName = PropertyDefaultValue; \


// TODO This is ugly
USTRUCT()
struct FTileMutationHelper
{
	GENERATED_BODY()

public:
	DEFINE_TILE_PROPERTY_MUTATOR(ETileType, Type, ETileType::ETileType_MAX)
	DEFINE_TILE_PROPERTY_MUTATOR(UBiomeTypeData*, BiomeTypeData, nullptr)
	DEFINE_TILE_PROPERTY_MUTATOR(ETileType, SmoothingOverride, ETileType::ETileType_MAX)
	DEFINE_TILE_PROPERTY_MUTATOR(uint8, NeighborMask, INDEX_NONE)
	DEFINE_TILE_PROPERTY_MUTATOR(int32, Moisture, INDEX_NONE)
	DEFINE_TILE_PROPERTY_MUTATOR(int32, Fertility, INDEX_NONE)
	DEFINE_TILE_PROPERTY_MUTATOR(bool, IsOccupied, false)

public:
	bool HasAnyChange() const
	{
		return bTypeChanged || bBiomeTypeDataChanged || bSmoothingOverrideChanged ||
				bNeighborMaskChanged || bMoistureChanged || bFertilityChanged || bIsOccupiedChanged;
	}

private:
	friend class UTileDataGrid;
	
	FORCEINLINE void Apply(FTileData& Tile) const
	{
		if (bTypeChanged)
		{
			Tile.Type = NewType;
		}
		if (bBiomeTypeDataChanged)
		{
			Tile.BiomeData = NewBiomeTypeData;
		}
		if (bSmoothingOverrideChanged)
		{
			Tile.Moisture = NewMoisture;
		}
		if (bNeighborMaskChanged)
		{
			Tile.Fertility = NewFertility;
		}
		if (bMoistureChanged)
		{
			Tile.Fertility = NewFertility;
		}
		if (bFertilityChanged)
		{
			Tile.Fertility = NewFertility;
		}
		if (bIsOccupiedChanged)
		{
			Tile.Fertility = NewFertility;
		}
	}
};