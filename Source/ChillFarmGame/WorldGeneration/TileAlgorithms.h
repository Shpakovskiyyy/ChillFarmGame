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
}
