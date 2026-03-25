#pragma once

#include "ETileType.h"
#include "TileData.generated.h"

class UBiomeTypeData;
struct FTileMutationHelper;


// Defines logical data for a single tile instance.
USTRUCT(BlueprintType)
struct FTileData final
{
	GENERATED_BODY()

	const FIntPoint&		GetCoordinates() const;
	ETileType				GetType() const;
	UBiomeTypeData*			GetBiomeData() const;
	ETileType				GetSmoothingOverride() const;
	uint8					GetNeighborMask() const;
	uint8					GetFertility() const;
	uint8					GetMoisture() const;
	bool					IsOccupied() const;
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FIntPoint Coordinates = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	ETileType Type = ETileType::ETileType_MAX;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	UBiomeTypeData* BiomeData = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	ETileType SmoothingOverride = ETileType::ETileType_MAX; 

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	uint8 NeighborMask = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	uint8 Fertility = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	uint8 Moisture = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	bool bIsOccupied = false;

	friend struct FTileMutationHelper;
	friend class UTileDataGrid;
	
};

FORCEINLINE const FIntPoint& FTileData::GetCoordinates() const		{ return Coordinates; }

FORCEINLINE ETileType FTileData::GetType() const					{ return Type; }

FORCEINLINE UBiomeTypeData* FTileData::GetBiomeData() const			{ return BiomeData; }

FORCEINLINE ETileType FTileData::GetSmoothingOverride() const		{ return SmoothingOverride; }

FORCEINLINE uint8 FTileData::GetNeighborMask() const				{ return NeighborMask; }

FORCEINLINE uint8 FTileData::GetFertility() const					{ return Fertility; }

FORCEINLINE uint8 FTileData::GetMoisture() const					{ return Moisture; }

FORCEINLINE bool FTileData::IsOccupied() const						{ return bIsOccupied; }