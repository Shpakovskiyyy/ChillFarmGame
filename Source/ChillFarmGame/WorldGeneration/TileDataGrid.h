#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "TileData.h"
#include "TileMutationHistory/TileMutationContext.h"

#include "TileDataGrid.generated.h"


// Only represents the data of the generated tile grid.
UCLASS()
class CHILLFARMGAME_API UTileDataGrid : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const FIntPoint& InGridSize, float InTileSize);
	void ClearAllChunks();

public:
	const FTileData* const GetTileChecked(int32 InX, int32 InY);
	const FTileData* const GetTileChecked(const FIntPoint& InCoordinate);

	FIntPoint GetSize() const;

public:
	void RecalculateNeighbors();
	uint8 GetNeighborMask(int32 InX, int32 InY);

public:
	bool HasNeighborOfType(int32 InX, int32 InY, ETileType InTileType);
 
public:
	bool IsValidTile(int32 InX, int32 InY) const;

public:
	FIntPoint WorldToTile(const FVector& InWorldLocation) const;
	FVector TileToWorld(int32 InX, int32 InY) const;

public:
	void MutateTile(const FTileMutationContext& MutationContext, const FTileMutationHelper& Mutation); 

private:
	void RecordHistory(const FTileMutationContext& InContext, const FTileData& Before, const FTileData& After);

private:
	FTileData& GetTileMutableChecked(int32 InX, int32 InY);
	int32 ToIndex(int32 InX, int32 InY) const;

private:
	UPROPERTY()
	int32 GridSizeX;

	UPROPERTY()
	int32 GridSizeY;

	UPROPERTY()
	float GridTileSize;

	UPROPERTY()
	TArray<FTileData> Tiles;

};