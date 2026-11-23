#pragma once

#include "TileData.h"
#include "TileVisualKey.generated.h"


UENUM(BlueprintType)
enum class ETileVisualPositionType : uint8
{
	Center,
	Edge,
	Corner,
	InnerCorner,
	Isolated
};

enum class ETileNeighborSide : uint8
{
	N  = 1 << 0,
	NE = 1 << 1,
	E  = 1 << 2,
	SE = 1 << 3,
	S  = 1 << 4,
	SW = 1 << 5,
	W  = 1 << 6,
	NW = 1 << 7,
};


USTRUCT(BlueprintType)
struct FTileVisualKey
{
	GENERATED_BODY()

public:
	bool operator==(const FTileVisualKey& Other) const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETileType TileType = ETileType::ETileType_MAX;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETileVisualPositionType VisualPositionType = ETileVisualPositionType::Center;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	int32 Rotation = 0;

	// The tile has ETileFlags::Watered. Same mesh and autotile piece, drawn with the variant's watered material.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bWatered = false;

};

FORCEINLINE bool FTileVisualKey::operator==(const FTileVisualKey& Other) const
{
	return TileType == Other.TileType && VisualPositionType == Other.VisualPositionType && bWatered == Other.bWatered;
}

FORCEINLINE uint32 GetTypeHash(const FTileVisualKey& Key)
{
	return HashCombine(
		HashCombine(GetTypeHash(Key.TileType), GetTypeHash(Key.VisualPositionType)),
		GetTypeHash(Key.bWatered)
	);
}