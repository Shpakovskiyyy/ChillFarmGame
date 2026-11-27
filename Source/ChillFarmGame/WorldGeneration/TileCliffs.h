#pragma once

#include "CoreMinimal.h"
#include "TileData.h"

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
// rebuilt together with the tile. On every edge the side reaches from the lower of the tile's two
// corners down to the lower of the neighbour's two corners (see FTileData::Slope), so:
// - flat ground a level apart gets a full side, and where a slope rises to higher ground there is none;
// - where a slope runs out into a cliff, the higher ground's side reaches down to the slope's low
//   corner and its lower part is hidden inside the slope; a slope above lower flat ground draws no
//   side, because slope pieces are solid and close their own sides.
// Tiles on the map edge get a side down to level 0, so a raised map does not show its underside.
namespace TileCliffs
{
	// Every cliff face of the generated tiles in [InFirst, InEnd).
	CHILLFARMGAME_API void CollectFaces(const FTileGrid& InGrid, const FIntPoint& InFirst, const FIntPoint& InEnd, TArray<FTileCliffFace>& OutFaces);

	// Instance transform that stretches a mesh with the given bounds into the face's wall: the
	// mesh's X becomes the width along the edge, Y the thickness (InThickness world units, inside
	// the high tile), Z the height of the drop. Works for any mesh pivot.
	CHILLFARMGAME_API FTransform MakeWallTransform(const FTileGridLayout& InLayout, const FTileCliffFace& InFace, const FBox& InMeshBounds, double InThickness);

	// Instance transform that lays a flat tile mesh on an edge slope: tilted so its top face rises
	// from the tile's level to the next one towards InRisesTowards (a cardinal offset), stretched along
	// the slope to cover the tile. Used when the visual set has no slope mesh; the sides stay open.
	CHILLFARMGAME_API FTransform MakeTiltedTileTransform(const FTileGridLayout& InLayout, const FIntPoint& InTile, uint8 InLevel, const FIntPoint& InRisesTowards, const FBox& InMeshBounds);

	// Instance transform that stretches a mesh with the given bounds into a box over one whole tile,
	// from InBottomLevel up to InTopLevel. Works for any mesh pivot. Used for the invisible blockers
	// that keep characters out of water.
	CHILLFARMGAME_API FTransform MakeColumnTransform(const FTileGridLayout& InLayout, const FIntPoint& InTile, int32 InBottomLevel, int32 InTopLevel, const FBox& InMeshBounds);

	// Yaw in degrees of a grid offset (see TileData::GetSlopeFacing), the angle UE turns X onto it by.
	CHILLFARMGAME_API double GetFacingYaw(const FIntPoint& InFacing);

	// Instance transform that fits a slope mesh (edge, outer or inner corner) into the tile's box:
	// one tile wide and long, from the tile's level to the next one. InMeshFacing is the way the mesh
	// faces in its own space and InFacing the way the slope faces, both as TileData::GetSlopeFacing
	// gives them; the mesh is turned from one to the other. Works for any mesh pivot.
	CHILLFARMGAME_API FTransform MakeSlopePieceTransform(const FTileGridLayout& InLayout, const FIntPoint& InTile, uint8 InLevel, const FIntPoint& InFacing, const FBox& InMeshBounds, const FIntPoint& InMeshFacing);
}
