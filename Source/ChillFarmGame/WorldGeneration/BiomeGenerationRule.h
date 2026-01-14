#pragma once

#include "EBiomeType.h"
#include "BiomeGenerationRule.generated.h"


USTRUCT(BlueprintType)
struct FBiomeGenerationRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float MinHeight;

	UPROPERTY(EditAnywhere)
	float MaxHeight;

	UPROPERTY(EditAnywhere)
	float MinMoisture;

	UPROPERTY(EditAnywhere)
	float MaxMoisture;

	UPROPERTY(EditAnywhere)
	EBiomeType Biome;
};
