#pragma once

#include "TileVisualKey.h"
#include "TileChunkVisualRepresentation.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

USTRUCT()
struct FTileChunkVisualRepresentation
{
	GENERATED_BODY()
	
	FIntPoint ChunkCoordinate;

	UPROPERTY()
	TMap<FTileVisualKey, UHierarchicalInstancedStaticMeshComponent*> Meshes;
};