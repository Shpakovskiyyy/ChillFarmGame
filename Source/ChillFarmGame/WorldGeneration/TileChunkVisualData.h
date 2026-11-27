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

	// Invisible boxes over water that only characters collide with, so water cannot be walked on.
	UPROPERTY()
	UHierarchicalInstancedStaticMeshComponent* WaterBlockers = nullptr;

	// The sides between elevation levels, see TileCliffs.
	UPROPERTY()
	UHierarchicalInstancedStaticMeshComponent* Cliffs = nullptr;

	// Slope pieces, one component per piece shape and tile type (and watered state), for the shapes
	// the visual set has a mesh for.
	UPROPERTY()
	TArray<UHierarchicalInstancedStaticMeshComponent*> Slopes;
};