#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "TileData.h"

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
	FTileData* GetTileChecked(int32 InX, int32 InY);
	FTileData* GetTileChecked(const FIntPoint& InCoordinate);

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
	void SetTile(int32 InX, int32 InY, const FTileData& InTileData);
	void SetTileType(int32 InX, int32 InY, ETileType InNewType);
	void SetOccupiedTile(int32 InX, int32 InY, bool bInIsOccupied);

	private:
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