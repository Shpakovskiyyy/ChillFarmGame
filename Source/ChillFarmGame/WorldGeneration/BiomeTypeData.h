#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "EBiomeType.h"
#include "ETileType.h"

#include "BiomeTypeData.generated.h"


class UTilesGenerationRuleSet;


// Generation settings of one biome.
UCLASS()
class CHILLFARMGAME_API UBiomeTypeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Biome")
	EBiomeType BiomeType = EBiomeType::Plains;

	// Height noise range (0..1) where this biome is the natural choice.
	UPROPERTY(EditDefaultsOnly, Category = "Biome")
	FVector2D HeightRange = FVector2D(0.0, 1.0);

	// Moisture noise range (0..1) where this biome is the natural choice.
	UPROPERTY(EditDefaultsOnly, Category = "Biome")
	FVector2D MoistureRange = FVector2D(0.0, 1.0);

	// Rules that pick the tile type inside this biome. Falls back to the generator's default rule set.
	UPROPERTY(EditDefaultsOnly, Category = "Tiles")
	UTilesGenerationRuleSet* TilesGenerationRuleSet = nullptr;

	// 0..100
	UPROPERTY(EditDefaultsOnly, Category = "Tiles", meta = (ClampMin = "0", ClampMax = "100"))
	uint8 BaseFertility = 50;

	// Replacement type for isolated tiles of a given type (see the smoothing pass).
	UPROPERTY(EditDefaultsOnly, Category = "Tiles")
	TMap<ETileType, ETileType> SmoothingOverrides;

	// Tile type used for land next to water.
	UPROPERTY(EditDefaultsOnly, Category = "Shore")
	ETileType ShoreTile = ETileType::Sand;

	// Not used by any pass. Kept so existing assets keep their values while the shore rules are redesigned.
	UPROPERTY(EditDefaultsOnly, Category = "Unused")
	bool bAllowWater = false;

	UPROPERTY(EditDefaultsOnly, Category = "Unused")
	ETileType DefaultSmoothingOverride = ETileType::Grass;

	UPROPERTY(EditDefaultsOnly, Category = "Unused")
	float ShoreHeight = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Unused")
	float ShoreWidth = 0.06f;
};
