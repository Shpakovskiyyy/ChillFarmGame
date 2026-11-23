#include "TileCliffs.h"

#include "TileGrid.h"
#include "TileGridLayout.h"


void TileCliffs::CollectFaces(const FTileGrid& InGrid, const FIntPoint& InFirst, const FIntPoint& InEnd, TArray<FTileCliffFace>& OutFaces)
{
	const FTileGridLayout& Layout = InGrid.GetLayout();

	for (int32 Y = FMath::Max(0, InFirst.Y); Y < FMath::Min(InEnd.Y, Layout.Size.Y); ++Y)
	{
		for (int32 X = FMath::Max(0, InFirst.X); X < FMath::Min(InEnd.X, Layout.Size.X); ++X)
		{
			const FIntPoint Tile(X, Y);
			const FTileData& Data = InGrid.GetTile(Tile);
			if (!Data.IsGenerated() || Data.Height == 0) continue;

			for (const FIntPoint& Offset : FTileGrid::GetCardinalOffsets())
			{
				// Past the map edge counts as level 0.
				const FTileData* Neighbor = InGrid.FindTile(Tile + Offset);
				const uint8 NeighborLevel = Neighbor && Neighbor->IsGenerated() ? Neighbor->Height : 0;
				if (NeighborLevel >= Data.Height) continue;

				OutFaces.Add({ Tile, Offset, Data.Height, NeighborLevel });
			}
		}
	}
}

FTransform TileCliffs::MakeWallTransform(const FTileGridLayout& InLayout, const FTileCliffFace& InFace, const FBox& InMeshBounds, double InThickness)
{
	// Stop just under the tile's top surface, so the wall's top face never fights with the tile mesh.
	constexpr double TopInset = 0.5;

	const double Top = InLayout.LevelToZ(InFace.TopLevel) - TopInset;
	const double Bottom = InLayout.LevelToZ(InFace.BottomLevel);
	const double Thickness = FMath::Clamp(InThickness, 0.1, InLayout.TileSize * 0.5);

	const FVector MeshSize = InMeshBounds.GetSize();
	const FVector Scale(
		InLayout.TileSize / FMath::Max(MeshSize.X, UE_KINDA_SMALL_NUMBER),
		Thickness / FMath::Max(MeshSize.Y, UE_KINDA_SMALL_NUMBER),
		FMath::Max(Top - Bottom, 0.1) / FMath::Max(MeshSize.Z, UE_KINDA_SMALL_NUMBER));

	// Turn the mesh so its +Y (the thickness axis) points along the face direction.
	const FVector2D Direction(InFace.Direction.X, InFace.Direction.Y);
	const FQuat Rotation(FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(-Direction.X, Direction.Y)), 0.0));

	// Centre of the wall: on the tile's edge, pulled inside the tile by half the thickness.
	const FVector TileCenter = InLayout.TileCenter(InFace.Tile, 0);
	const FVector Outward(Direction.X, Direction.Y, 0.0);
	FVector WallCenter = TileCenter + Outward * (InLayout.TileSize * 0.5 - Thickness * 0.5);
	WallCenter.Z = (Top + Bottom) * 0.5;

	// Place the scaled, rotated mesh so the centre of its bounds lands on the wall centre.
	const FVector MeshCenter = Rotation.RotateVector(InMeshBounds.GetCenter() * Scale);
	return FTransform(Rotation, WallCenter - MeshCenter, Scale);
}
