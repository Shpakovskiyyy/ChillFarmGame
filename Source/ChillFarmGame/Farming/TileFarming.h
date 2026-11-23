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
	// Grass and dirt can be tilled into farmland, unless something stands on them.
	CHILLFARMGAME_API bool CanTill(const FTileData& InTile);
	CHILLFARMGAME_API FTileMutation MakeTill();

	// Farmland can be watered once a day; the day cycle clears the flag again.
	CHILLFARMGAME_API bool CanWater(const FTileData& InTile);
	CHILLFARMGAME_API FTileMutation MakeWater();

	// The single "use" action: tills what can be tilled, waters what can be watered.
	// Returns false when the tool does nothing on this tile. OutStep names the action for the tile history.
	CHILLFARMGAME_API bool MakeToolMutation(const FTileData& InTile, FTileMutation& OutMutation, FName& OutStep);

	inline const FName TillStep = TEXT("Till");
	inline const FName WaterStep = TEXT("Water");
}
