#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "EBiomeType.h"
#include "ETileType.h"
#include "TileData.h"

#include "BiomeTypeData.generated.h"


class UTilesGenerationRuleSet;


UCLASS()
class CHILLFARMGAME_API UBiomeTypeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// TODO Maybe move to some other place
	bool IsShore (float Height) const;

public:
	ETileType ResolveBorderTile(float Height, ETileType BaseTileType, float WaterLevel) const;
	ETileType ResolveSmoothingOverride(ETileType InTileType) const;

public:
	void FillRuntimeTileData(FTileData& OutTileData, float Height, float Moisture) const;

public:
	UPROPERTY(EditDefaultsOnly)
	EBiomeType BiomeType;
	
	UPROPERTY(EditDefaultsOnly)
	FVector2D HeightRange;

	UPROPERTY(EditDefaultsOnly)
	FVector2D MoistureRange;

	UPROPERTY(EditDefaultsOnly)
	UTilesGenerationRuleSet* TilesGenerationRuleSet;
	
	UPROPERTY(EditDefaultsOnly)
	uint8 BaseFertility = 50;

	UPROPERTY(EditDefaultsOnly)
	bool bAllowWater = false;

	UPROPERTY(EditDefaultsOnly)
	TMap<ETileType, ETileType> SmoothingOverrides;

	UPROPERTY(EditDefaultsOnly)
	ETileType DefaultSmoothingOverride;

	/* Biome shore properties */
	UPROPERTY(EditDefaultsOnly)
	ETileType ShoreTile = ETileType::Sand;

	UPROPERTY(EditDefaultsOnly)
	float ShoreHeight = 0.35f;

	UPROPERTY(EditDefaultsOnly)
	float ShoreWidth = 0.06f;
	/* Biome shore properties */
};