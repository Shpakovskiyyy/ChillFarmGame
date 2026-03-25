#include "TileDebugRender/TileDebugHighlightComponent.h"
#include "TileDebugRender/TileDebugSceneProxy.h"
#include "TileDebugRender/TileDebugDrawTile.h"


UTileDebugHighlightComponent::UTileDebugHighlightComponent()
{
	bIsEditorOnly = true;
	bHiddenInGame = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UTileDebugHighlightComponent::AddHighlightedTiles(const TArray<FTileDebugDrawTile>& InTiles)
{
	Tiles = InTiles;
	MarkRenderStateDirty();
}

void UTileDebugHighlightComponent::AddHighlightedTile(int32 TileIndex)
{
	SelectedTileIndex = TileIndex;
	MarkRenderStateDirty();
}

FPrimitiveSceneProxy* UTileDebugHighlightComponent::CreateSceneProxy()
{
	return new FTileDebugSceneProxy(this, TArray(Tiles), SelectedTileIndex);
}

FBoxSphereBounds UTileDebugHighlightComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox Box(ForceInit);

	for (const auto& Tile : Tiles)
	{
		Box += FBox(
			Tile.Center - Tile.Extent,
			Tile.Center + Tile.Extent
		);
	}

	return FBoxSphereBounds(Box);
}