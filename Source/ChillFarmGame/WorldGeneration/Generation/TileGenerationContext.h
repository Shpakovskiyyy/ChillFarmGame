#pragma once

#include "CoreMinimal.h"
#include "TileGrid.h"
#include "TileMutation.h"

class UBiomeTypeDataSet;
class UTilesGenerationRuleSet;


// Everything a generation pass may read or write.
//
// Passes never touch a world, an actor or the subsystem: they work on the grid in this context.
// That keeps generation deterministic, testable without a level, and movable to a worker thread later.
struct FTileGenerationContext
{
	explicit FTileGenerationContext(FTileGrid& InGrid, int32 InSeed)
		: Grid(InGrid)
		, Seed(InSeed)
	{
		const int32 NumTiles = Grid.GetLayout().NumTiles();
		HeightField.SetNumZeroed(NumTiles);
		MoistureField.SetNumZeroed(NumTiles);
		DetailField.SetNumZeroed(NumTiles);
	}

	FTileGrid& Grid;
	const int32 Seed;

	// Continuous 0..1 values per tile, written by the terrain pass and read by later passes.
	// They are generation-only intermediate data and are not stored in FTileData.
	TArray<float> HeightField;
	TArray<float> MoistureField;
	TArray<float> DetailField;

	// Height noise at and below which a tile is water, set by the terrain pass. Elevation is measured
	// from here.
	float WaterLevel = 0.f;

	// Assets shared by several passes.
	const UBiomeTypeDataSet* Biomes = nullptr;
	UTilesGenerationRuleSet* DefaultRuleSet = nullptr;

	// Reported in every change this context makes.
	const UObject* Instigator = nullptr;

	// Called for every applied change when set. Building the change costs a copy of the tile,
	// so leave it empty when nobody records history.
	TFunction<void(const FTileChange&)> OnChange;

	// Set by the pipeline before each pass so changes can say which step made them.
	FName CurrentStep = NAME_None;

	const FTileGridLayout& GetLayout() const { return Grid.GetLayout(); }
	int32 ToIndex(const FIntPoint& InTile) const { return Grid.GetLayout().ToIndex(InTile); }

	// A random stream that depends only on the seed and the salt, so each pass gets its own
	// numbers and adding or removing a pass does not reshuffle the others.
	FRandomStream MakeStream(uint32 InSalt) const
	{
		const uint32 Mixed = static_cast<uint32>(Seed) * 2654435761u ^ (InSalt + 0x9E3779B9u);
		return FRandomStream(static_cast<int32>(Mixed));
	}

	// The only way passes change tiles.
	ETileField Mutate(const FIntPoint& InTile, const FTileMutation& InMutation, FName InReason = NAME_None)
	{
		if (!OnChange)
		{
			return Grid.ApplyMutation(InTile, InMutation);
		}

		FTileChange Change;
		Change.ChangedFields = Grid.ApplyMutation(InTile, InMutation, &Change.Before);
		if (Change.ChangedFields == ETileField::None) return ETileField::None;

		Change.Coordinates = InTile;
		Change.After = Grid.GetTile(InTile);
		Change.Context.Source = ETileMutationSource::WorldGeneration;
		Change.Context.Step = CurrentStep;
		Change.Context.Reason = InReason;
		Change.Context.Instigator = Instigator;

		OnChange(Change);
		return Change.ChangedFields;
	}
};
