#pragma once

#include "CoreMinimal.h"
#include "ETileMutationSource.generated.h"

// The system a tile change came from.
UENUM()
enum class ETileMutationSource : uint8
{
	Unknown,

	// A generation pass. Reproduced from the seed, never saved.
	WorldGeneration,

	// Kept for old history entries; generation passes use WorldGeneration.
	PostProcess UMETA(Hidden),

	// An editor tool changed the tile by hand.
	Editor,

	// The player did it (tilling, watering, building). This is what a save game has to store.
	Gameplay,

	// A system acting over time (crops drying out, weather).
	Simulation
};
