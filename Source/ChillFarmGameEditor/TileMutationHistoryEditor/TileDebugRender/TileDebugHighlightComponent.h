#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"

#include "TileDebugRender/TileDebugDrawTile.h"

#include "TileDebugHighlightComponent.generated.h"


UCLASS()
class UTileDebugHighlightComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UTileDebugHighlightComponent();

public:
	void AddHighlightedTiles(const TArray<FTileDebugDrawTile>& InTiles);
	void AddHighlightedTile(int32 InSelectedTileIndex);

	// UPrimitiveComponent
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

private:
	TArray<FTileDebugDrawTile> Tiles;
	int32 SelectedTileIndex = INDEX_NONE;
	
};