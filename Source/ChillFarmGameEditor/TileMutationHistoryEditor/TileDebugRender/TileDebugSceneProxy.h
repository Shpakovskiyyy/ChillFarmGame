#pragma once

#include "TileDebugHighlightComponent.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"


class FTileDebugSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FTileDebugSceneProxy(const UTileDebugHighlightComponent* Component, TArray<FTileDebugDrawTile>&& InTiles, int32 InSelectedIndex);

protected:
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual uint32 GetMemoryFootprint() const override;

protected:
	virtual SIZE_T GetTypeHash() const override;

private:
	TArray<FTileDebugDrawTile> Tiles;
	int32 SelectedIndex;
	
};