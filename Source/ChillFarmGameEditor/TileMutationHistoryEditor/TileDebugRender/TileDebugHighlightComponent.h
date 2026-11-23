#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"

#include "TileDebugRender/TileDebugDrawTile.h"

#include "TileDebugHighlightComponent.generated.h"


// Editor-only primitive that draws wire boxes over tiles through FTileDebugSceneProxy.
UCLASS()
class UTileDebugHighlightComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UTileDebugHighlightComponent();

public:
	void SetTiles(const TArray<FTileDebugDrawTile>& InTiles);

	// UPrimitiveComponent
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

private:
	TArray<FTileDebugDrawTile> Tiles;
};
