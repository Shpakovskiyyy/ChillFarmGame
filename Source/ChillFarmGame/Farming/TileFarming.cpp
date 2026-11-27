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

	FTileMutation MakeDry()
	{
		return FTileMutation().SetFlag(ETileFlags::Watered, false);
	}

	EFarmAction ChooseAction(const FTileData& InTile, const FFarmActionInput& InInput)
	{
		if (InInput.bHasCrop && InInput.bCropRipe) return EFarmAction::Harvest;
		if (CanTill(InTile)) return EFarmAction::Till;

		// Plant before watering, so a fresh bed takes the seed first and the next click waters it.
		if (InTile.Type == ETileType::Farmland && !InInput.bHasCrop && InInput.bHasSeed) return EFarmAction::Plant;

		return CanWater(InTile) ? EFarmAction::Water : EFarmAction::None;
	}

	bool MakeActionMutation(EFarmAction InAction, FTileMutation& OutMutation, FName& OutStep)
	{
		switch (InAction)
		{
			case EFarmAction::Till:  OutMutation = MakeTill();  OutStep = TillStep;  return true;
			case EFarmAction::Water: OutMutation = MakeWater(); OutStep = WaterStep; return true;
			default: return false;
		}
	}
}
