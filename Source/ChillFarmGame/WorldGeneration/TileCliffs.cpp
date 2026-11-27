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
			if (!Data.IsGenerated()) continue;

			for (const FIntPoint& Offset : FTileGrid::GetCardinalOffsets())
			{
				ETileCorner First, Second;
				TileData::GetEdgeCorners(Offset, First, Second);
				const int32 Top = FMath::Min(TileData::GetCornerLevel(Data, First), TileData::GetCornerLevel(Data, Second));

				// The neighbour's side of the same edge. Past the map edge counts as level 0.
				int32 Bottom = 0;
				const FTileData* Neighbor = InGrid.FindTile(Tile + Offset);
				if (Neighbor && Neighbor->IsGenerated())
				{
					TileData::GetEdgeCorners(FIntPoint::ZeroValue - Offset, First, Second);
					Bottom = FMath::Min(TileData::GetCornerLevel(*Neighbor, First), TileData::GetCornerLevel(*Neighbor, Second));
				}
				if (Top <= Bottom) continue;

				OutFaces.Add({ Tile, Offset, static_cast<uint8>(Top), static_cast<uint8>(Bottom) });
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

FTransform TileCliffs::MakeColumnTransform(const FTileGridLayout& InLayout, const FIntPoint& InTile, int32 InBottomLevel, int32 InTopLevel, const FBox& InMeshBounds)
{
	const double Bottom = InLayout.LevelToZ(InBottomLevel);
	const double Top = InLayout.LevelToZ(FMath::Max(InTopLevel, InBottomLevel + 1));

	const FVector MeshSize = InMeshBounds.GetSize();
	const FVector Scale(
		InLayout.TileSize / FMath::Max(MeshSize.X, UE_KINDA_SMALL_NUMBER),
		InLayout.TileSize / FMath::Max(MeshSize.Y, UE_KINDA_SMALL_NUMBER),
		(Top - Bottom) / FMath::Max(MeshSize.Z, UE_KINDA_SMALL_NUMBER));

	FVector Center = InLayout.TileCenter(InTile, 0);
	Center.Z = (Top + Bottom) * 0.5;
	return FTransform(FQuat::Identity, Center - InMeshBounds.GetCenter() * Scale, Scale);
}

double TileCliffs::GetFacingYaw(const FIntPoint& InFacing)
{
	return FMath::RadiansToDegrees(FMath::Atan2(static_cast<double>(InFacing.Y), static_cast<double>(InFacing.X)));
}

FTransform TileCliffs::MakeSlopePieceTransform(const FTileGridLayout& InLayout, const FIntPoint& InTile, uint8 InLevel, const FIntPoint& InFacing, const FBox& InMeshBounds, const FIntPoint& InMeshFacing)
{
	const FVector MeshSize = InMeshBounds.GetSize();
	const FVector Scale(
		InLayout.TileSize / FMath::Max(MeshSize.X, UE_KINDA_SMALL_NUMBER),
		InLayout.TileSize / FMath::Max(MeshSize.Y, UE_KINDA_SMALL_NUMBER),
		InLayout.LevelHeight / FMath::Max(MeshSize.Z, UE_KINDA_SMALL_NUMBER));

	// Turn the way the mesh faces in its own space onto the way the slope faces. Both are multiples of
	// 90 degrees apart, and the tile is square, so scaling before turning fits either way.
	const FQuat Rotation(FRotator(0.0, GetFacingYaw(InFacing) - GetFacingYaw(InMeshFacing), 0.0));

	// The middle of the mesh's bottom goes to the middle of the tile on its own level.
	const FVector MeshAnchor(InMeshBounds.GetCenter().X, InMeshBounds.GetCenter().Y, InMeshBounds.Min.Z);
	const FVector TileBottom = InLayout.TileCenter(InTile, InLevel);
	return FTransform(Rotation, TileBottom - Rotation.RotateVector(MeshAnchor * Scale), Scale);
}

FTransform TileCliffs::MakeTiltedTileTransform(const FTileGridLayout& InLayout, const FIntPoint& InTile, uint8 InLevel, const FIntPoint& InRisesTowards, const FBox& InMeshBounds)
{
	const FVector2D Direction(InRisesTowards.X, InRisesTowards.Y);

	// The tile mesh's X runs along the slope, tilted up by the angle of one level over one tile.
	const double Rise = InLayout.LevelHeight;
	const double Run = InLayout.TileSize;
	const double SlopeLength = FMath::Sqrt(Rise * Rise + Run * Run);

	const FVector MeshSize = InMeshBounds.GetSize();
	const FVector Scale(
		SlopeLength / FMath::Max(MeshSize.X, UE_KINDA_SMALL_NUMBER),
		InLayout.TileSize / FMath::Max(MeshSize.Y, UE_KINDA_SMALL_NUMBER),
		1.0);

	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
	const double Pitch = FMath::RadiansToDegrees(FMath::Atan2(Rise, Run));
	const FQuat Rotation(FRotator(Pitch, Yaw, 0.0));

	// The middle of the mesh's top face goes to the middle of the slope, half a level up.
	const FVector MeshAnchor(InMeshBounds.GetCenter().X, InMeshBounds.GetCenter().Y, InMeshBounds.Max.Z);
	FVector SlopeCenter = InLayout.TileCenter(InTile, InLevel);
	SlopeCenter.Z += Rise * 0.5;
	return FTransform(Rotation, SlopeCenter - Rotation.RotateVector(MeshAnchor * Scale), Scale);
}
