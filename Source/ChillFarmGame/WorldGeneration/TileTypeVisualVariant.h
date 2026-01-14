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
	ETileVisualPositionType VisualType;

	UPROPERTY(EditAnywhere)
	uint32 Rotation;

	UPROPERTY(EditAnywhere)
	UStaticMesh* Mesh;

	UPROPERTY(EditAnywhere)
	UMaterialInterface* Material;
	
};