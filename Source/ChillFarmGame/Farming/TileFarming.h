#pragma once

#include "CoreMinimal.h"
#include "TileData.h"
#include "TileMutation.h"


// What the player's tool does to a tile.
//
// Pure rules: they read one tile's data and describe the change, they never touch the world.
// The caller applies the result through UTileWorldSubsystem::MutateTile, so every gameplay edit
// goes through the same path as everything else (history, visuals, a future save).
namespace TileFarming
{
	// What the single "use" action does on a tile, in the order the farming loop goes through.
	enum class EFarmAction : uint8
	{
		None,
		Harvest,
		Till,
		Plant,
		Water,
	};

	// What the use action knows about the tile besides its data.
	struct FFarmActionInput
	{
		bool bHasCrop = false;
		bool bCropRipe = false;
		bool bHasSeed = false;
	};

	// Grass and dirt can be tilled into farmland, unless something stands on them or they are a slope.
	CHILLFARMGAME_API bool CanTill(const FTileData& InTile);
	CHILLFARMGAME_API FTileMutation MakeTill();

	// Farmland can be watered once a day; the day cycle clears the flag again.
	CHILLFARMGAME_API bool CanWater(const FTileData& InTile);
	CHILLFARMGAME_API FTileMutation MakeWater();

	// Overnight the land dries: the day cycle clears the Watered flag on every tile with this.
	CHILLFARMGAME_API FTileMutation MakeDry();

	// The single "use" action: harvests a ripe crop, tills what can be tilled, plants on empty
	// farmland, waters a dry tile. One click is one step of the loop.
	CHILLFARMGAME_API EFarmAction ChooseAction(const FTileData& InTile, const FFarmActionInput& InInput);

	// The tile change an action makes, if any (harvest and plant change crops, not tiles), and the
	// step name for the tile history.
	CHILLFARMGAME_API bool MakeActionMutation(EFarmAction InAction, FTileMutation& OutMutation, FName& OutStep);

	inline const FName TillStep = TEXT("Till");
	inline const FName WaterStep = TEXT("Water");
	inline const FName OvernightStep = TEXT("Overnight");
}
