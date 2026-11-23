#pragma once

#include "CoreMinimal.h"
#include "TileVisualKey.h"
#include "TileTypeVisualVariant.generated.h"

class UMaterialInterface;
class UStaticMesh;

// Represents a single variant of how an instance of the tile is visualized.
USTRUCT(BlueprintType)
struct FTileTypeVisualVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	ETileVisualPositionType VisualType = ETileVisualPositionType::Center;

	UPROPERTY(EditAnywhere)
	uint32 Rotation = 0;

	UPROPERTY(EditAnywhere)
	UStaticMesh* Mesh = nullptr;

	UPROPERTY(EditAnywhere)
	UMaterialInterface* Material = nullptr;

	// Used instead of Material while the tile is watered. Empty means watered tiles look like dry ones.
	UPROPERTY(EditAnywhere)
	UMaterialInterface* WateredMaterial = nullptr;

	UMaterialInterface* GetMaterial(bool bWatered) const
	{
		return bWatered && WateredMaterial ? WateredMaterial : Material;
	}
};