#pragma once

#include "CoreMinimal.h"


// Where tiles are in the world. The one place that converts between tile coordinates and positions.
//
// Convention: tile (X, Y) covers the square from (X, Y) * TileSize to (X + 1, Y + 1) * TileSize,
// measured from Origin, and its centre is at (X + 0.5, Y + 0.5) * TileSize. The top surface of a tile
// on level L is at Origin.Z + L * LevelHeight. The grid is axis aligned: rotation is not supported.
//
// The data grid, the visual grid, the inspector and gameplay code all go through this struct, so
// "which tile is under this point" has exactly one answer.
struct FTileGridLayout
{
	// Number of tiles along X and Y.
	FIntPoint Size = FIntPoint::ZeroValue;

	// Edge length of one tile in world units.
	float TileSize = 100.f;

	// World height of one elevation level.
	float LevelHeight = 100.f;

	// World position of the outer corner of tile (0, 0) on level 0.
	FVector Origin = FVector::ZeroVector;

	bool IsEmpty() const { return Size.X <= 0 || Size.Y <= 0; }
	int32 NumTiles() const { return IsEmpty() ? 0 : Size.X * Size.Y; }

	bool Contains(const FIntPoint& InTile) const
	{
		return InTile.X >= 0 && InTile.Y >= 0 && InTile.X < Size.X && InTile.Y < Size.Y;
	}

	int32 ToIndex(const FIntPoint& InTile) const { return InTile.X + InTile.Y * Size.X; }
	FIntPoint ToTile(int32 InIndex) const { return FIntPoint(InIndex % Size.X, InIndex / Size.X); }

	// World Z of the top surface of a tile on the given level.
	double LevelToZ(uint8 InLevel) const { return Origin.Z + static_cast<double>(InLevel) * LevelHeight; }

	// World position of the middle of the tile's top surface.
	FVector TileCenter(const FIntPoint& InTile, uint8 InLevel = 0) const
	{
		return FVector(
			Origin.X + (static_cast<double>(InTile.X) + 0.5) * TileSize,
			Origin.Y + (static_cast<double>(InTile.Y) + 0.5) * TileSize,
			LevelToZ(InLevel));
	}

	// The tile whose square contains the point. The result may be outside the grid: check Contains().
	FIntPoint WorldToTile(const FVector& InWorldLocation) const
	{
		return FIntPoint(
			FMath::FloorToInt32((InWorldLocation.X - Origin.X) / TileSize),
			FMath::FloorToInt32((InWorldLocation.Y - Origin.Y) / TileSize));
	}

	// World-space box of the whole grid, from level 0 up to the given level.
	FBox GetWorldBounds(uint8 InMaxLevel = 0) const
	{
		const FVector Max(
			Origin.X + static_cast<double>(Size.X) * TileSize,
			Origin.Y + static_cast<double>(Size.Y) * TileSize,
			LevelToZ(InMaxLevel));
		return FBox(Origin, Max);
	}
};
