#pragma once

#include "TileDebugHighlightComponent.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"


// Render thread side of UTileDebugHighlightComponent. Owns its own copy of the tiles.
class FTileDebugSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FTileDebugSceneProxy(const UTileDebugHighlightComponent* Component, TArray<FTileDebugDrawTile>&& InTiles);

protected:
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual uint32 GetMemoryFootprint() const override;
	virtual SIZE_T GetTypeHash() const override;

private:
	TArray<FTileDebugDrawTile> Tiles;
};