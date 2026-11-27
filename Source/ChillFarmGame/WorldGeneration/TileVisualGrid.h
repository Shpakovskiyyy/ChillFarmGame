#pragma once

#include "CoreMinimal.h"
#include "TileVisualKey.h"
#include "TileChunkVisualData.h"

#include "GameFramework/Actor.h"
#include "TileVisualGrid.generated.h"

class ANavMeshBoundsVolume;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class FTileGrid;
class UTileWorldSubsystem;
class UTilesVisualDataSet;


// Draws the tile map of the world's UTileWorldSubsystem.
//
// Holds no tile data of its own: everything it draws is read from the subsystem's grid, and it
// redraws the affected chunks when tiles change. The components it creates are transient, the map
// is regenerated from the seed rather than saved as thousands of instances.
UCLASS()
class CHILLFARMGAME_API ATileVisualGrid : public AActor
{
	GENERATED_BODY()

public:
	ATileVisualGrid();

public:
	// Rebuilds every chunk from the subsystem's grid and starts following its changes.
	void RebuildAll(UTileWorldSubsystem& InTileWorld, int32 InChunkSize);

	// Rebuilds the given chunks only.
	void RebuildChunks(const TSet<FIntPoint>& InChunks);

	void ClearAllChunks();

public:
	// Tells the navigation system that the geometry inside the box changed.
	void FinalizeNavigation(const FBox& InDirtyArea);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BuildChunk(const FTileGrid& InGrid, const FIntPoint& InChunk);
	void BuildChunkCliffs(const FTileGrid& InGrid, const FIntPoint& InChunk, const FIntPoint& InFirst, const FIntPoint& InEnd);

	// Water is drawn like any surface, so on its own a character could stand on it. Every water tile
	// gets an invisible box from the water up WaterBlockerLevels levels that blocks only characters
	// (traces for clicks and the camera pass through) and keeps the navmesh off the water.
	void BuildChunkWaterBlockers(const FTileGrid& InGrid, const FIntPoint& InChunk, const FIntPoint& InFirst, const FIntPoint& InEnd);
	UHierarchicalInstancedStaticMeshComponent* CreateChunkComponent(const FString& InBaseName, UStaticMesh* InMesh, UMaterialInterface* InMaterial);
	void DestroyChunk(const FIntPoint& InChunk);
	FBox GetChunkBounds(const FTileGrid& InGrid, const FIntPoint& InChunk) const;

	void HandleTilesChanged(TConstArrayView<FIntPoint> InTiles);
	void HandleWorldCleared();
	void UnbindFromTileWorld();

public:
	UPROPERTY(EditAnywhere, Category = "Tiles")
	UTilesVisualDataSet* TilesVisualSet = nullptr;

private:
	UPROPERTY(Transient)
	TMap<FIntPoint, FTileChunkVisualData> Chunks;

	UPROPERTY(Transient)
	TWeakObjectPtr<UTileWorldSubsystem> TileWorld;

	TWeakObjectPtr<ANavMeshBoundsVolume> NavMeshBoundsVolume;

	int32 ChunkSize = 64;
};
