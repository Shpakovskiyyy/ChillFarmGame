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

	// Slope pieces (see FTileData::Slope). Each is stretched to one tile by one level, turned to face
	// the slope and drawn with the tile type's material. They should be solid, so they close their
	// own sides where a slope runs out into a cliff.

	// Edge slope (also every ramp): the tile rises towards one neighbour. Empty means the tile type's
	// own mesh is tilted instead, which leaves the sides open.
	UPROPERTY(EditDefaultsOnly, Category = "Slopes")
	UStaticMesh* SlopeEdgeMesh = nullptr;

	// Which way SlopeEdgeMesh rises in its own space. North is -Y (LevelPrototyping's SM_Ramp rises that way).
	UPROPERTY(EditDefaultsOnly, Category = "Slopes")
	ETileDirection SlopeEdgeMeshRisesTowards = ETileDirection::North;

	// Outer corner: one corner raised, the outside of a hill's corner. Empty means these tiles are
	// drawn flat on their own level.
	UPROPERTY(EditDefaultsOnly, Category = "Slopes")
	UStaticMesh* SlopeOuterCornerMesh = nullptr;

	// Which corner of SlopeOuterCornerMesh is the raised one, in its own space.
	UPROPERTY(EditDefaultsOnly, Category = "Slopes")
	ETileCorner SlopeOuterCornerMeshRaisedCorner = ETileCorner::NorthEast;

	// Inner corner: three corners raised, the inside of a hill's corner. Empty means these tiles are
	// drawn as two edge pieces rising to the two raised edges (flat on the level above when the edge
	// slot is empty too).
	UPROPERTY(EditDefaultsOnly, Category = "Slopes")
	UStaticMesh* SlopeInnerCornerMesh = nullptr;

	// Which corner of SlopeInnerCornerMesh is the low one, in its own space.
	UPROPERTY(EditDefaultsOnly, Category = "Slopes")
	ETileCorner SlopeInnerCornerMeshLowCorner = ETileCorner::SouthWest;

	// The mesh for a slope shape (edge, outer or inner corner) and the way it faces in its own space,
	// as TileData::GetSlopeFacing gives it. Null when the slot is empty or the shape has no piece.
	UStaticMesh* GetSlopeMesh(ETileSlopeShape InShape, FIntPoint& OutMeshFacing) const;
};
