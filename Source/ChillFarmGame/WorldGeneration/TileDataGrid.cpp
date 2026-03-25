#include "TileDataGrid.h"
#include "TileVisualKey.h"
#include "TileMutationHistory/TileMutationHelper.h"

#include "Engine/World.h"
#include "TileMutationHistory/TileMutationHistorySubsystem.h"


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

const FTileData* const UTileDataGrid::GetTileChecked(int32 InX, int32 InY)
{
	//check(IsValidTile(InX, InY))
	if (!IsValidTile(InX, InY)) return nullptr;
	return &Tiles[ToIndex(InX, InY)];
}

const FTileData* const UTileDataGrid::GetTileChecked(const FIntPoint& InCoordinate)
{
	return GetTileChecked(InCoordinate.X, InCoordinate.Y);
}

FIntPoint UTileDataGrid::GetSize() const { return FIntPoint(GridSizeX, GridSizeY); }

void UTileDataGrid::RecalculateNeighbors()
{
	for (int32 X = 0; X < GridSizeX; X++)
	{
		for (int32 Y = 0; Y < GridSizeY; Y++)
		{
			auto Tile = GetTileChecked(X, Y);
			auto NeighborMask = Tile->GetNeighborMask();
			
			// TODO Mutation: Actual phases and reasons
			FTileMutationHelper TileMutation;
			auto MutationContext = FTileMutationContext::Make({X, Y}, ETileMutationSource::PostProcess, Tile->BiomeData, this, TEXT("Base World Generation"), TEXT("Neighbors Recalculating"));
			TileMutation.SetNeighborMask(NeighborMask);
			MutateTile(MutationContext, TileMutation);
		}
	}
}

uint8 UTileDataGrid::GetNeighborMask(int32 InX, int32 InY)
{
	const auto& Center = GetTileChecked(InX, InY);
	uint8 Mask = 0;

	auto Check = [Center, &Mask, this](int32 NX, int32 NY, ETileNeighborSide Bit)
	{
		if (!IsValidTile(NX, NY)) return;
		if (GetTileChecked(NX, NY)->GetType() == Center->GetType()) Mask |= static_cast<uint8>(Bit);
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

		if (GetTileChecked(NX, NY)->GetType() == InTileType)
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

void UTileDataGrid::MutateTile(const FTileMutationContext& MutationContext, const FTileMutationHelper& Mutation)
{
	if (!Mutation.HasAnyChange()) return; 

	const auto& Coordinates = MutationContext.Coordinates; 
	FTileData& Tile = GetTileMutableChecked(Coordinates.X, Coordinates.Y);
	const auto Before = Tile;

	Mutation.Apply(Tile);

	RecordHistory(MutationContext, Before, Tile);
}

void UTileDataGrid::RecordHistory(const FTileMutationContext& InContext, const FTileData& Before, const FTileData& After)
{
	if (auto HistorySubsystem = GetWorld()->GetSubsystem<UTileMutationHistorySubsystem>())
	{
		HistorySubsystem->RecordMutation(InContext, Before, After);
	}
}

int32 UTileDataGrid::ToIndex(int32 InX, int32 InY) const { return InX + InY * GridSizeX; }

FTileData& UTileDataGrid::GetTileMutableChecked(int32 InX, int32 InY)
{
	check(IsValidTile(InX, InY));
	return Tiles[ToIndex(InX, InY)];
}