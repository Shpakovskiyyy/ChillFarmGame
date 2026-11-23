#pragma once

#include "TileVisualKey.h"
#include "TileChunkVisualData.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

USTRUCT()
struct FTileChunkVisualData
{
	GENERATED_BODY()
	
	FIntPoint ChunkCoordinate;

	UPROPERTY()
	TMap<FTileVisualKey, UHierarchicalInstancedStaticMeshComponent*> Meshes;

	// The sides between elevation levels, see TileCliffs.
	UPROPERTY()
	UHierarchicalInstancedStaticMeshComponent* Cliffs = nullptr;
};