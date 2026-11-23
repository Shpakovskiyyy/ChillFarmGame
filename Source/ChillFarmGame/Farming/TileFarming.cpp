#include "TileFarming.h"


namespace TileFarming
{
	bool CanTill(const FTileData& InTile)
	{
		// Farmland stays flat: a field on a slope would be one nobody can stand level on.
		if (InTile.HasFlag(ETileFlags::Occupied) || InTile.IsSlope()) return false;
		return InTile.Type == ETileType::Grass || InTile.Type == ETileType::Dirt;
	}

	FTileMutation MakeTill()
	{
		// Only the surface changes. Fertility and moisture describe the land and stay as generated.
		return FTileMutation().SetType(ETileType::Farmland);
	}

	bool CanWater(const FTileData& InTile)
	{
		return InTile.Type == ETileType::Farmland && !InTile.HasFlag(ETileFlags::Watered);
	}

	FTileMutation MakeWater()
	{
		return FTileMutation().SetFlag(ETileFlags::Watered, true);
	}

	bool MakeToolMutation(const FTileData& InTile, FTileMutation& OutMutation, FName& OutStep)
	{
		if (CanTill(InTile))
		{
			OutMutation = MakeTill();
			OutStep = TillStep;
			return true;
		}
		if (CanWater(InTile))
		{
			OutMutation = MakeWater();
			OutStep = WaterStep;
			return true;
		}
		return false;
	}
}
