#include "TileDebugSceneProxy.h"


FTileDebugSceneProxy::FTileDebugSceneProxy(const UTileDebugHighlightComponent* Component, TArray<FTileDebugDrawTile>&& InTiles, int32 InSelectedIndex)
	: FPrimitiveSceneProxy(Component),
	Tiles(MoveTemp(InTiles)),
	SelectedIndex(InSelectedIndex)
{
	UE_LOG(LogTemp, Warning, TEXT("FTileDebugSceneProxy created with %d tiles"), Tiles.Num());
}

void FTileDebugSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	FPrimitiveSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
	
	for (auto ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (!(VisibilityMap & (1 << ViewIndex))) continue;
		
		auto PDI = Collector.GetPDI(ViewIndex);
		for (auto TileIndex = 0; TileIndex < Tiles.Num(); ++TileIndex)
		{
			const auto& Tile = Tiles[TileIndex];

			const auto bSelected = TileIndex == SelectedIndex;
			const auto Color = bSelected ? FColor::Yellow : Tile.Color;

			DrawWireBox(
				PDI,
				FBox(Tile.Center - Tile.Extent, Tile.Center + Tile.Extent),
				Color,
				SDPG_Foreground
			);
		}
	}
}

FPrimitiveViewRelevance FTileDebugSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = true;
	Result.bDynamicRelevance = true;
	Result.bEditorPrimitiveRelevance = true;
	return Result;
}

uint32 FTileDebugSceneProxy::GetMemoryFootprint() const 
{
	return sizeof(*this) + Tiles.GetAllocatedSize();
}

SIZE_T FTileDebugSceneProxy::GetTypeHash() const {
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}