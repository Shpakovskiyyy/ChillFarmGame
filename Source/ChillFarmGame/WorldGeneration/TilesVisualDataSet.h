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
	UTilesVisualDataSet();

public:
	UPROPERTY(EditDefaultsOnly)
	TMap<ETileType, UTileTypeVisualData*> TileTypeVisuals;
};