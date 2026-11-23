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

	// Mesh stretched into the vertical sides between elevation levels. Any mesh works (a cube is
	// fine): it is scaled from its bounds. Empty means terraces are drawn without sides.
	UPROPERTY(EditDefaultsOnly, Category = "Cliffs")
	UStaticMesh* CliffMesh = nullptr;

	// Overrides the cliff mesh's own material when set.
	UPROPERTY(EditDefaultsOnly, Category = "Cliffs")
	UMaterialInterface* CliffMaterial = nullptr;

	// How deep the side reaches into the high tile, as a fraction of the tile size.
	UPROPERTY(EditDefaultsOnly, Category = "Cliffs", meta = (ClampMin = "0.01", ClampMax = "0.5"))
	float CliffThickness = 0.1f;
};
