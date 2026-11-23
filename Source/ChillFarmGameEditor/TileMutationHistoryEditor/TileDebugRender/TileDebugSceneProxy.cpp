#include "TileDebugSceneProxy.h"


FTileDebugSceneProxy::FTileDebugSceneProxy(const UTileDebugHighlightComponent* Component, TArray<FTileDebugDrawTile>&& InTiles)
	: FPrimitiveSceneProxy(Component)
	, Tiles(MoveTemp(InTiles))
{
}

void FTileDebugSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		if (!(VisibilityMap & (1 << ViewIndex))) continue;

		FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
		for (const FTileDebugDrawTile& Tile : Tiles)
		{
			DrawWireBox(PDI, FBox(Tile.Center - Tile.Extent, Tile.Center + Tile.Extent), Tile.Color, SDPG_Foreground, 2.f);
		}
	}
}

FPrimitiveViewRelevance FTileDebugSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bDynamicRelevance = true;
	Result.bEditorPrimitiveRelevance = true;
	return Result;
}

uint32 FTileDebugSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize() + Tiles.GetAllocatedSize();
}

SIZE_T FTileDebugSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}
