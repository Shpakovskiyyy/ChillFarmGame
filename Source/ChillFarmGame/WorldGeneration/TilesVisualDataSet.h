#pragma once

#include "CoreMinimal.h"

#include "TileData.h"
#include "TileTypeVisualData.h"

#include "Engine/DataAsset.h"
#include "TilesVisualDataSet.generated.h"


// A collection of visual data for each ETileType.
UCLASS()
class CHILLFARMGAME_API UTilesVisualDataSet : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	// Reports tile types that have no visual data. Runs on demand (asset save, "Validate Assets",
	// the data validation commandlet) when the asset's data is actually loaded.
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

public:
	UPROPERTY(EditDefaultsOnly)
	TMap<ETileType, UTileTypeVisualData*> TileTypeVisuals;
};
