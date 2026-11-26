#pragma once

#include "CoreMinimal.h"


// Grid algorithms that only need plain arrays. Kept free of engine types beyond containers so
// they can be unit tested and reused by any pass.
namespace TileAlgorithms
{
	// Lowers levels until no two direct (4-way) neighbours differ by more than MaxStep.
	//
	// Only lowers, never raises: water and valleys stay where they are and peaks are cut down into
	// terraces. Two sweeps (forward, then backward) compute, for every tile, the minimum over all paths
	// of "neighbour level + distance * MaxStep", which is exactly the constraint. Runs in O(tiles).
	//
	// Returns the number of tiles whose level changed.
	inline int32 ClampLevelSteps(TArrayView<uint8> InOutLevels, const FIntPoint& InSize, uint8 InMaxStep = 1)
	{
		if (InSize.X <= 0 || InSize.Y <= 0 || InOutLevels.Num() != InSize.X * InSize.Y) return 0;

		const TArray<uint8> Original(InOutLevels.GetData(), InOutLevels.Num());

		const auto Relax = [&InOutLevels, InMaxStep](int32 Index, int32 NeighborIndex)
		{
			const int32 Limit = static_cast<int32>(InOutLevels[NeighborIndex]) + InMaxStep;
			if (InOutLevels[Index] > Limit) InOutLevels[Index] = static_cast<uint8>(Limit);
		};

		// Forward sweep: neighbours above and to the left are final for this direction.
		for (int32 Y = 0; Y < InSize.Y; ++Y)
		{
			for (int32 X = 0; X < InSize.X; ++X)
			{
				const int32 Index = X + Y * InSize.X;
				if (X > 0) Relax(Index, Index - 1);
				if (Y > 0) Relax(Index, Index - InSize.X);
			}
		}

		// Backward sweep: neighbours below and to the right.
		for (int32 Y = InSize.Y - 1; Y >= 0; --Y)
		{
			for (int32 X = InSize.X - 1; X >= 0; --X)
			{
				const int32 Index = X + Y * InSize.X;
				if (X < InSize.X - 1) Relax(Index, Index + 1);
				if (Y < InSize.Y - 1) Relax(Index, Index + InSize.X);
			}
		}

		int32 Changed = 0;
		for (int32 Index = 0; Index < InOutLevels.Num(); ++Index)
		{
			if (InOutLevels[Index] != Original[Index]) ++Changed;
		}
		return Changed;
	}

	// Merges every area smaller than InMinSize tiles into the land around it.
	//
	// An area is a group of tiles where InIsArea is true, on the same level, joined through their four
	// direct neighbours. A small area takes the level most of the area tiles around it have (the lower
	// one on a tie), so specks and thin bits of a level disappear into their surroundings. Tiles where
	// InIsArea is false (water) are never changed and never counted, so a small area surrounded only by
	// them stays. Areas are found once, before any change; call again (with ClampLevelSteps in between)
	// to clean up what a merge leaves behind.
	//
	// Returns the number of tiles whose level changed.
	inline int32 MergeSmallAreas(TArrayView<uint8> InOutLevels, TConstArrayView<bool> InIsArea, const FIntPoint& InSize, int32 InMinSize)
	{
		const int32 NumTiles = InSize.X * InSize.Y;
		if (InSize.X <= 0 || InSize.Y <= 0 || InOutLevels.Num() != NumTiles || InIsArea.Num() != NumTiles || InMinSize <= 1) return 0;

		const FIntPoint Offsets[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
		const auto Neighbor = [&InSize](int32 Index, const FIntPoint& Offset)
		{
			const int32 X = Index % InSize.X + Offset.X;
			const int32 Y = Index / InSize.X + Offset.Y;
			return (X >= 0 && Y >= 0 && X < InSize.X && Y < InSize.Y) ? X + Y * InSize.X : INDEX_NONE;
		};

		// Label the areas on the levels as they are now.
		TArray<int32> Labels;
		Labels.Init(INDEX_NONE, NumTiles);
		TArray<TArray<int32>> Areas;
		for (int32 Start = 0; Start < NumTiles; ++Start)
		{
			if (!InIsArea[Start] || Labels[Start] != INDEX_NONE) continue;

			const int32 Label = Areas.Num();
			TArray<int32>& Tiles = Areas.AddDefaulted_GetRef();
			Labels[Start] = Label;
			Tiles.Add(Start);
			for (int32 Head = 0; Head < Tiles.Num(); ++Head)
			{
				for (const FIntPoint& Offset : Offsets)
				{
					const int32 Next = Neighbor(Tiles[Head], Offset);
					if (Next == INDEX_NONE || !InIsArea[Next] || Labels[Next] != INDEX_NONE || InOutLevels[Next] != InOutLevels[Start]) continue;
					Labels[Next] = Label;
					Tiles.Add(Next);
				}
			}
		}

		int32 Changed = 0;
		TMap<uint8, int32> Around;
		for (int32 Label = 0; Label < Areas.Num(); ++Label)
		{
			const TArray<int32>& Tiles = Areas[Label];
			if (Tiles.Num() >= InMinSize) continue;

			// Levels of the area tiles that touch this one from outside.
			Around.Reset();
			for (const int32 Tile : Tiles)
			{
				for (const FIntPoint& Offset : Offsets)
				{
					const int32 Next = Neighbor(Tile, Offset);
					if (Next != INDEX_NONE && InIsArea[Next] && Labels[Next] != Label) ++Around.FindOrAdd(InOutLevels[Next]);
				}
			}
			if (Around.IsEmpty()) continue;

			uint8 Best = 0;
			int32 BestCount = -1;
			for (const TPair<uint8, int32>& Entry : Around)
			{
				if (Entry.Value > BestCount || (Entry.Value == BestCount && Entry.Key < Best))
				{
					Best = Entry.Key;
					BestCount = Entry.Value;
				}
			}

			for (const int32 Tile : Tiles)
			{
				if (InOutLevels[Tile] == Best) continue;
				InOutLevels[Tile] = Best;
				++Changed;
			}
		}
		return Changed;
	}
}
