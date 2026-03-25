#pragma once

#include "CoreMinimal.h"
#include "TileVisualKey.h"
#include "TileChunkVisualData.h"

#include "GameFramework/Actor.h"
#include "TileVisualGrid.generated.h"

class ANavMeshBoundsVolume;
class UTileDataGrid;
class UTilesVisualDataSet;
class UTileTypeVisualData;
class UHierarchicalInstancedStaticMeshComponent;


struct FTileInstancesBatch
{
	TArray<FTransform> Transforms;
	TArray<FIntPoint> Coordinates;
};


UCLASS()
class CHILLFARMGAME_API ATileVisualGrid : public AActor
{
	GENERATED_BODY()

public:
	ATileVisualGrid();
	void Initialize(uint32 InChunkSize, float InTileSize);

public:
	void CreateChunk(const FIntPoint& ChunkIndex, const TMap<FTileVisualKey, FTileInstancesBatch>& InInstancesBatch);

public:
	void ClearAllChunks();
	void RebuildChunk(const FIntPoint& ChunkCoordinates, UTileDataGrid* TileGridData, uint32 InChunkSize, float InTileSize);

public:
	void FinalizeNavigation();

public:
	const FIntPoint& GetTileCoordinatesFromHISMC(UHierarchicalInstancedStaticMeshComponent* InHISMC, uint32 InInstanceIndex);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	UPROPERTY(EditAnywhere, Category="Tiles")
	UTilesVisualDataSet* TilesVisualSet;

private:
	UPROPERTY()
	TMap<FIntPoint, FTileChunkVisualData> Chunks;
	// TODO Rename
	TMap<UHierarchicalInstancedStaticMeshComponent*, TArray<FIntPoint>> InstanceToTile;

private:
	TWeakObjectPtr<ANavMeshBoundsVolume> NavMeshBoundsVolume;

	private:
	UPROPERTY()
	uint32 ChunkSize = INDEX_NONE;

	UPROPERTY()
	float TileSize = 0.0f;
	
};