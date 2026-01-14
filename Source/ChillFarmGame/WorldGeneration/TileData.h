#pragma once

#include "EBiomeType.h"
#include "ETileType.h"
#include "TileData.generated.h"

class UBiomeTypeData;


// Defines logical data for ETileType.
USTRUCT(BlueprintType)
struct FTileData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FIntPoint TileCoordinate = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	ETileType Type = ETileType::ETileType_MAX;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	UBiomeTypeData* BiomeData = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	ETileType SmoothingOverride = ETileType::ETileType_MAX; 

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	int32 NeighborMask;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	uint8 Fertility = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	uint8 Moisture = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	bool bIsOccupied = false;
};