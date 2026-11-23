#pragma once

#include "TileData.h"
#include "TileNoiseContext.h"
#include "TileGenerationRule.generated.h"

// Rule that defines generation for a single ETileType.
USTRUCT(BlueprintType)
struct FTileGenerationRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETileType TileType = ETileType::Grass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETileNoiseSource NoiseSource = ETileNoiseSource::Height;

	/* Noise thresholds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
	float MinNoise = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
	float MaxNoise = 1.f;
	/* Noise thresholds*/
	
};