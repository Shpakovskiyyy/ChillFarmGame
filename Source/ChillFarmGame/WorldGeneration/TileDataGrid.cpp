#include "TileDataGrid.h"

#include "TileVisualKey.h"


void UTileDataGrid::Initialize(const FIntPoint& InGridSize, float InTileSize)
{
	GridSizeX = InGridSize.X;
	GridSizeY = InGridSize.Y;
	GridTileSize = InTileSize;

	Tiles.SetNum(GridSizeX * GridSizeY);
}

void UTileDataGrid::ClearAllChunks()
{
	Tiles.Empty();
}

FTileData* UTileDataGrid::GetTileChecked(int32 InX, int32 InY)
{
	if (!IsValidTile(InX, InY)) return nullptr;
	return &Tiles[ToIndex(InX, InY)];
}

FTileData* UTileDataGrid::GetTileChecked(const FIntPoint& InCoordinate)
{
	check(IsValidTile(InCoordinate.X, InCoordinate.Y));
	return GetTileChecked(InCoordinate.X, InCoordinate.Y);
}

FIntPoint UTileDataGrid::GetSize() const { return FIntPoint(GridSizeX, GridSizeY); }

void UTileDataGrid::RecalculateNeighbors()
{
	for (int32 X = 0; X < GridSizeX; X++)
	{
		for (int32 Y = 0; Y < GridSizeY; Y++)
		{
			GetTileChecked(X, Y)->NeighborMask = GetNeighborMask(X, Y);
		}
	}
}

uint8 UTileDataGrid::GetNeighborMask(int32 InX, int32 InY)
{
	auto Center = GetTileChecked(InX, InY);
	uint8 Mask = 0;

	auto Check = [Center, &Mask, this](int32 NX, int32 NY, ETileNeighborSide Bit)
	{
		if (!IsValidTile(NX, NY)) return;
		if (GetTileChecked(NX, NY)->Type == Center->Type) Mask |= static_cast<uint8>(Bit);
	};

	Check(InX,   InY-1, ETileNeighborSide::N);
	Check(InX+1, InY-1, ETileNeighborSide::NE);
	Check(InX+1, InY,   ETileNeighborSide::E);
	Check(InX+1, InY+1, ETileNeighborSide::SE);
	Check(InX,   InY+1, ETileNeighborSide::S);
	Check(InX-1, InY+1, ETileNeighborSide::SW);
	Check(InX-1, InY,   ETileNeighborSide::W);
	Check(InX-1, InY-1, ETileNeighborSide::NW);

	return Mask;
}

bool UTileDataGrid::HasNeighborOfType(int32 InX, int32 InY, ETileType InTileType)
{
	static const FIntPoint Offsets[] = {
		{ 1, 0 }, { -1, 0 },
		{ 0, 1 }, { 0, -1 },
	};

	for (const auto& Offset : Offsets)
	{
		int32 NX = InX + Offset.X;
		int32 NY = InY + Offset.Y;

		if (!IsValidTile(NX, NY)) continue;

		if (GetTileChecked(NX, NY)->Type == InTileType)
			return true;
	}

	return false;
}

bool UTileDataGrid::IsValidTile(int32 InX, int32 InY) const
{
	return InX >= 0 && InY >= 0 && InX < GridSizeX && InY < GridSizeY;
}

FIntPoint UTileDataGrid::WorldToTile(const FVector& InWorldLocation) const
{
	return FIntPoint(
		FMath::FloorToInt(InWorldLocation.X / GridTileSize),
		FMath::FloorToInt(InWorldLocation.Y / GridTileSize)
	);
}

FVector UTileDataGrid::TileToWorld(int32 InX, int32 InY) const
{
	return FVector(
		InX * GridTileSize + GridTileSize * 0.5f,
		InY * GridTileSize + GridTileSize * 0.5f,
		0.f
	);
}

void UTileDataGrid::SetTile(int32 InX, int32 InY, const FTileData& InTileData)
{
	check(IsValidTile(InX, InY))
	if (!IsValidTile(InX, InY)) return;
	auto Tile = GetTileChecked(InX, InY);
	*Tile = InTileData;
}

void UTileDataGrid::SetTileType(int32 InX, int32 InY, ETileType InNewType)
{
	check(IsValidTile(InX, InY));
	
	auto Tile = GetTileChecked(InX, InY);
	if (!Tile) return;
	Tile->Type = InNewType;
}

void UTileDataGrid::SetOccupiedTile(int32 InX, int32 InY, bool bInIsOccupied)
{
	check(IsValidTile(InX, InY));

	auto Tile = GetTileChecked(InX, InY);
	if (!Tile) return;
	Tile->bIsOccupied = bInIsOccupied;
}

int32 UTileDataGrid::ToIndex(int32 InX, int32 InY) const { return InX + InY * GridSizeX; }