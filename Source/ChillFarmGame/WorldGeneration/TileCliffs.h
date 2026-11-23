#pragma once

#include "CoreMinimal.h"

class FTileGrid;
struct FTileGridLayout;


// One vertical cliff side: the edge of Tile that faces Direction, from TopLevel down to BottomLevel.
struct FTileCliffFace
{
	FIntPoint Tile = FIntPoint::ZeroValue;

	// One of the four cardinal offsets, pointing from the high tile towards the lower side.
	FIntPoint Direction = FIntPoint::ZeroValue;

	uint8 TopLevel = 0;
	uint8 BottomLevel = 0;

	bool operator==(const FTileCliffFace& Other) const
	{
		return Tile == Other.Tile && Direction == Other.Direction && TopLevel == Other.TopLevel && BottomLevel == Other.BottomLevel;
	}
};


// Where the sides of the terraces go. Visual only, derived from tile heights whenever visuals are
// built, so cliffs can never disagree with the data.
//
// A cliff belongs to the higher tile and is drawn just inside its edge, so a tile's cliffs are
// rebuilt together with the tile. Tiles on the map edge get a side down to level 0, so a raised
// map does not show its underside.
namespace TileCliffs
{
	// Every cliff face of the generated tiles in [InFirst, InEnd).
	CHILLFARMGAME_API void CollectFaces(const FTileGrid& InGrid, const FIntPoint& InFirst, const FIntPoint& InEnd, TArray<FTileCliffFace>& OutFaces);

	// Instance transform that stretches a mesh with the given bounds into the face's wall: the
	// mesh's X becomes the width along the edge, Y the thickness (InThickness world units, inside
	// the high tile), Z the height of the drop. Works for any mesh pivot.
	CHILLFARMGAME_API FTransform MakeWallTransform(const FTileGridLayout& InLayout, const FTileCliffFace& InFace, const FBox& InMeshBounds, double InThickness);
}
