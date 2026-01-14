#pragma once

#include "CoreMinimal.h"
#include "TileVisualKey.h"
#include "TileChunkVisualRepresentation.h"

#include "GameFramework/Actor.h"
#include "TileGrid.generated.h"

class UTileChunkNavComponent;
class ANavMeshBoundsVolume;
class UTileDataGrid;
class UTilesVisualDataSet;
class UTileTypeVisualData;
class UHierarchicalInstancedStaticMeshComponent;


UCLASS()
class CHILLFARMGAME_API ATileGrid : public AActor
{
	GENERATED_BODY()

public:
	ATileGrid();

public:
	void CreateChunk(const FIntPoint& ChunkIndex, const TMap<FTileVisualKey, TArray<FTransform>>& InTransforms);
	void ClearAllChunks();

public:
	void RebuildChunk(const FIntPoint& ChunkCoordinates, UTileDataGrid* TileGridData, int32 ChunkSize, float TileSize);

public:
	void FinalizeNavigation();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	UPROPERTY(EditAnywhere, Category="Tiles")
	UTilesVisualDataSet* TilesVisualSet;

private:
	UPROPERTY()
	TMap<FIntPoint, FTileChunkVisualRepresentation> Chunks;

private:
	TWeakObjectPtr<ANavMeshBoundsVolume> NavMeshBoundsVolume;
	
};