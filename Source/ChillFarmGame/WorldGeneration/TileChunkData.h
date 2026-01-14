#pragma once

#include "TileData.h"
#include "TileChunkData.generated.h"


USTRUCT(BlueprintType)
struct FTileChunkData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FIntPoint ChunkCoordinate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 SizeX = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 SizeY = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FTileData> Tiles;

	FORCEINLINE int32 ToIndex(int32 X, int32 Y) const
	{
		return X + Y * SizeX;
	}

	FORCEINLINE const FTileData& GetTile(int32 X, int32 Y) const
	{
		return Tiles[ToIndex(X, Y)];
	}
};