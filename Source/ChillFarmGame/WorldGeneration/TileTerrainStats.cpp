#include "TileTerrainStats.h"

#include "TileGrid.h"
#include "TileReachability.h"


FTileTerrainStats TileTerrainStats::Compute(const FTileGrid& InGrid, int32 InPlotSize)
{
	FTileTerrainStats Stats;
	const FTileGridLayout& Layout = InGrid.GetLayout();
	const int32 NumTiles = Layout.NumTiles();
	if (NumTiles == 0) return Stats;

	const auto IsFlat = [](const FTileData& InTile)
	{
		return TileReachability::IsWalkable(InTile) && !InTile.IsSlope();
	};

	// The base ground: the lowest level any land is on.
	int32 BaseLevel = MAX_int32;
	for (const FTileData& Tile : InGrid.GetTiles())
	{
		if (TileReachability::IsWalkable(Tile)) BaseLevel = FMath::Min<int32>(BaseLevel, Tile.Height);
	}

	// Counts and cliffs. Each pair of neighbours is looked at once, from the left or upper tile.
	for (int32 Index = 0; Index < NumTiles; ++Index)
	{
		const FIntPoint Tile = Layout.ToTile(Index);
		const FTileData& Data = InGrid.GetTile(Tile);
		if (!TileReachability::IsWalkable(Data)) continue;

		++Stats.LandTiles;
		if (!Data.IsSlope()) ++Stats.FlatTiles;
		if (Data.Height > BaseLevel) ++Stats.RaisedTiles;

		for (const FIntPoint& Offset : { FIntPoint(1, 0), FIntPoint(0, 1) })
		{
			const FTileData* Next = InGrid.FindTile(Tile + Offset);
			if (Next && TileReachability::IsWalkable(*Next) && !TileReachability::CanStep(Data, *Next, Offset)) ++Stats.CliffEdges;
		}
	}

	// Flat areas: flood fill over flat neighbours on the same level.
	TArray<int32> Areas;
	Areas.Init(INDEX_NONE, NumTiles);
	TArray<int32> Queue;
	for (int32 Start = 0; Start < NumTiles; ++Start)
	{
		if (Areas[Start] != INDEX_NONE || !IsFlat(InGrid.GetTiles()[Start])) continue;

		const int32 Area = Stats.FlatAreas++;
		Areas[Start] = Area;
		Queue.Reset();
		Queue.Add(Start);
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const FIntPoint Tile = Layout.ToTile(Queue[Head]);
			const uint8 Level = InGrid.GetTile(Tile).Height;
			for (const FIntPoint& Offset : FTileGrid::GetCardinalOffsets())
			{
				const FIntPoint Next = Tile + Offset;
				if (!Layout.Contains(Next)) continue;
				const int32 NextIndex = Layout.ToIndex(Next);
				const FTileData& NextData = InGrid.GetTile(Next);
				if (Areas[NextIndex] != INDEX_NONE || !IsFlat(NextData) || NextData.Height != Level) continue;
				Areas[NextIndex] = Area;
				Queue.Add(NextIndex);
			}
		}
	}

	// Largest flat square per area: the classic "largest square ending here" table. A square only
	// grows while its left, upper and upper-left neighbours are in the same flat area.
	TArray<int32> Squares;
	Squares.Init(0, NumTiles);
	TArray<int32> AreaSquare;
	AreaSquare.Init(0, Stats.FlatAreas);
	for (int32 Y = 0; Y < Layout.Size.Y; ++Y)
	{
		for (int32 X = 0; X < Layout.Size.X; ++X)
		{
			const int32 Index = Layout.ToIndex(FIntPoint(X, Y));
			const int32 Area = Areas[Index];
			if (Area == INDEX_NONE) continue;

			int32 Size = 1;
			if (X > 0 && Y > 0)
			{
				const int32 Left = Layout.ToIndex(FIntPoint(X - 1, Y));
				const int32 Up = Layout.ToIndex(FIntPoint(X, Y - 1));
				const int32 UpLeft = Layout.ToIndex(FIntPoint(X - 1, Y - 1));
				if (Areas[Left] == Area && Areas[Up] == Area && Areas[UpLeft] == Area)
				{
					Size = FMath::Min3(Squares[Left], Squares[Up], Squares[UpLeft]) + 1;
				}
			}
			Squares[Index] = Size;
			AreaSquare[Area] = FMath::Max(AreaSquare[Area], Size);
		}
	}
	for (const int32 Size : AreaSquare)
	{
		Stats.LargestFlatSquare = FMath::Max(Stats.LargestFlatSquare, Size);
		if (Size >= InPlotSize) ++Stats.FarmSites;
	}

	// Plots: a square of the plot size whose south-east corner has a large enough square ending there
	// lies inside one flat area; take it when none of its tiles is taken yet.
	if (InPlotSize > 0)
	{
		TArray<bool> Taken;
		Taken.Init(false, NumTiles);
		for (int32 Y = InPlotSize - 1; Y < Layout.Size.Y; ++Y)
		{
			for (int32 X = InPlotSize - 1; X < Layout.Size.X; ++X)
			{
				if (Squares[Layout.ToIndex(FIntPoint(X, Y))] < InPlotSize) continue;

				bool bFree = true;
				for (int32 PY = Y - InPlotSize + 1; PY <= Y && bFree; ++PY)
				{
					for (int32 PX = X - InPlotSize + 1; PX <= X && bFree; ++PX)
					{
						bFree = !Taken[Layout.ToIndex(FIntPoint(PX, PY))];
					}
				}
				if (!bFree) continue;

				for (int32 PY = Y - InPlotSize + 1; PY <= Y; ++PY)
				{
					for (int32 PX = X - InPlotSize + 1; PX <= X; ++PX)
					{
						Taken[Layout.ToIndex(FIntPoint(PX, PY))] = true;
					}
				}
				++Stats.FarmPlots;
			}
		}
	}

	// Distance to the edge of the flat area: breadth-first from the border tiles inwards.
	TArray<int32> Distances;
	Distances.Init(0, NumTiles);
	Queue.Reset();
	for (int32 Index = 0; Index < NumTiles; ++Index)
	{
		if (Areas[Index] == INDEX_NONE) continue;
		const FIntPoint Tile = Layout.ToTile(Index);
		for (const FIntPoint& Offset : FTileGrid::GetCardinalOffsets())
		{
			const FIntPoint Next = Tile + Offset;
			if (!Layout.Contains(Next) || Areas[Layout.ToIndex(Next)] != Areas[Index])
			{
				Distances[Index] = 1;
				Queue.Add(Index);
				break;
			}
		}
	}
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const FIntPoint Tile = Layout.ToTile(Queue[Head]);
		const int32 Distance = Distances[Queue[Head]];
		for (const FIntPoint& Offset : FTileGrid::GetCardinalOffsets())
		{
			const FIntPoint Next = Tile + Offset;
			if (!Layout.Contains(Next)) continue;
			const int32 NextIndex = Layout.ToIndex(Next);
			if (Areas[NextIndex] == INDEX_NONE || Distances[NextIndex] != 0) continue;
			Distances[NextIndex] = Distance + 1;
			Queue.Add(NextIndex);
		}
	}

	int64 DistanceSum = 0;
	for (int32 Index = 0; Index < NumTiles; ++Index)
	{
		if (Areas[Index] != INDEX_NONE) DistanceSum += Distances[Index];
	}
	Stats.MeanEdgeDistance = Stats.FlatTiles > 0 ? static_cast<float>(DistanceSum) / Stats.FlatTiles : 0.f;
	return Stats;
}
