#pragma once

#include "CoreMinimal.h"
#include "TileData.h"
#include "TileGridLayout.h"
#include "TileMutation.h"


// The logical tile map: a layout plus one FTileData per tile.
//
// Plain C++ on purpose. It has no world, no garbage collector and no delegates, so generation can
// build one on any thread, tests can create one in a line, and a save game can copy it as a blob.
// Tiles can only be read as const; ApplyMutation is the single way to change one.
class CHILLFARMGAME_API FTileGrid
{
public:
	FTileGrid() = default;
	explicit FTileGrid(const FTileGridLayout& InLayout);

	const FTileGridLayout& GetLayout() const { return Layout; }
	FIntPoint GetSize() const { return Layout.Size; }

	bool IsEmpty() const { return Tiles.IsEmpty(); }
	bool Contains(const FIntPoint& InTile) const { return Layout.Contains(InTile); }

	// Asserts when the coordinate is outside the grid.
	const FTileData& GetTile(const FIntPoint& InTile) const;

	// nullptr when the coordinate is outside the grid.
	const FTileData* FindTile(const FIntPoint& InTile) const;

	TConstArrayView<FTileData> GetTiles() const { return Tiles; }

	// Applies the mutation and returns the fields that changed (None when nothing did).
	// OutBefore receives the tile as it was, which callers use to describe the change.
	ETileField ApplyMutation(const FIntPoint& InTile, const FTileMutation& InMutation, FTileData* OutBefore = nullptr);

public:
	// True when one of the four direct neighbours has the given type.
	bool HasNeighborOfType(const FIntPoint& InTile, ETileType InType) const;

	// Offsets of the four direct neighbours: north, east, south, west.
	static const FIntPoint (&GetCardinalOffsets())[4];

private:
	FTileGridLayout Layout;
	TArray<FTileData> Tiles;
};
