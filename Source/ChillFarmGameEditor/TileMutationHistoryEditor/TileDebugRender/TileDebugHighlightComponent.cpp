#include "TileDebugRender/TileDebugHighlightComponent.h"
#include "TileDebugRender/TileDebugSceneProxy.h"


UTileDebugHighlightComponent::UTileDebugHighlightComponent()
{
	bIsEditorOnly = true;
	bHiddenInGame = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCanEverAffectNavigation(false);
}

void UTileDebugHighlightComponent::SetTiles(const TArray<FTileDebugDrawTile>& InTiles)
{
	Tiles = InTiles;
	UpdateBounds();
	MarkRenderStateDirty();
}

FPrimitiveSceneProxy* UTileDebugHighlightComponent::CreateSceneProxy()
{
	if (Tiles.IsEmpty()) return nullptr;
	return new FTileDebugSceneProxy(this, TArray<FTileDebugDrawTile>(Tiles));
}

FBoxSphereBounds UTileDebugHighlightComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox Box(ForceInit);
	for (const auto& Tile : Tiles)
	{
		Box += FBox(Tile.Center - Tile.Extent, Tile.Center + Tile.Extent);
	}

	// Bounds must never be invalid, or the primitive is culled before it is drawn.
	if (!Box.IsValid) Box = FBox(FVector(-1.0), FVector(1.0));
	return FBoxSphereBounds(Box);
}
