#include "TileReachability.h"

#include "TileGrid.h"


bool TileReachability::IsWalkable(const FTileData& InTile)
{
	return InTile.IsGenerated() && !InTile.IsWater();
}

bool TileReachability::CanStep(const FTileData& From, const FTileData& To, const FIntPoint& InOffset)
{
	if (!IsWalkable(From) || !IsWalkable(To)) return false;

	// The shared edge, seen from both sides: the corners come out in the same order.
	ETileCorner FromFirst, FromSecond, ToFirst, ToSecond;
	TileData::GetEdgeCorners(InOffset, FromFirst, FromSecond);
	TileData::GetEdgeCorners(FIntPoint::ZeroValue - InOffset, ToFirst, ToSecond);

	return TileData::GetCornerLevel(From, FromFirst) == TileData::GetCornerLevel(To, ToFirst)
		|| TileData::GetCornerLevel(From, FromSecond) == TileData::GetCornerLevel(To, ToSecond);
}

namespace
{
	// Flood fill that labels every walkable tile; Connected decides which neighbour steps count.
	template <typename ConnectedType>
	int32 LabelAreas(const FTileGrid& InGrid, TArray<int32>& OutLabels, ConnectedType&& Connected)
	{
		const FTileGridLayout& Layout = InGrid.GetLayout();
		OutLabels.Init(INDEX_NONE, Layout.NumTiles());

		int32 NumLabels = 0;
		TArray<int32> Queue;
		for (int32 Start = 0; Start < Layout.NumTiles(); ++Start)
		{
			if (OutLabels[Start] != INDEX_NONE || !TileReachability::IsWalkable(InGrid.GetTiles()[Start])) continue;

			const int32 Label = NumLabels++;
			OutLabels[Start] = Label;
			Queue.Reset();
			Queue.Add(Start);

			// Breadth-first: the queue only grows, so reading by index visits every tile once.
			for (int32 Head = 0; Head < Queue.Num(); ++Head)
			{
				const FIntPoint Tile = Layout.ToTile(Queue[Head]);
				const FTileData& Data = InGrid.GetTile(Tile);

				for (const FIntPoint& Offset : FTileGrid::GetCardinalOffsets())
				{
					const FIntPoint Next = Tile + Offset;
					if (!Layout.Contains(Next)) continue;

					const int32 NextIndex = Layout.ToIndex(Next);
					if (OutLabels[NextIndex] != INDEX_NONE || !Connected(Data, InGrid.GetTile(Next), Offset)) continue;

					OutLabels[NextIndex] = Label;
					Queue.Add(NextIndex);
				}
			}
		}
		return NumLabels;
	}
}

int32 TileReachability::ComputeWalkRegions(const FTileGrid& InGrid, TArray<int32>& OutRegions)
{
	return LabelAreas(InGrid, OutRegions, &TileReachability::CanStep);
}

int32 TileReachability::ComputeLandIslands(const FTileGrid& InGrid, TArray<int32>& OutIslands)
{
	return LabelAreas(InGrid, OutIslands, [](const FTileData& From, const FTileData& To, const FIntPoint&)
	{
		return IsWalkable(From) && IsWalkable(To);
	});
}
