#pragma once

#include "CoreMinimal.h"
#include "Generation/TileGenerationPass.h"
#include "TileGenerationPasses.generated.h"


// Samples the noise fields every later pass reads and marks water. Elevation levels are assigned
// later by the Elevation pass, once biomes are known.
UCLASS(meta = (DisplayName = "Terrain Noise"))
class CHILLFARMGAME_API UTerrainNoisePass : public UTileGenerationPass
{
	GENERATED_BODY()

public:
	virtual void Execute(FTileGenerationContext& Context) const override;
	virtual FName GetStepName() const override { return TEXT("Terrain"); }

public:
	// Perlin sampling scale for height and moisture. Smaller values give larger features.
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (ClampMin = "0.0001"))
	float NoiseScale = 0.025f;

	// How strongly the domain warp bends height and moisture shapes.
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (ClampMin = "0"))
	float WarpStrength = 0.1f;

	// Sampling scale of the high frequency detail noise used to break up borders.
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (ClampMin = "0.0001"))
	float DetailNoiseScale = 0.15f;

	// Range of the random offsets derived from the seed. Large enough that two seeds sample unrelated areas.
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (ClampMin = "1"))
	float SeedOffsetRange = 1000.f;

	// Tiles with a height noise at or below this become water.
	UPROPERTY(EditAnywhere, Category = "Water", meta = (ClampMin = "0", ClampMax = "1"))
	float WaterLevel = 0.3f;

};


// Picks a biome for every land tile from height and moisture, and stores the tile's moisture.
UCLASS(meta = (DisplayName = "Biomes"))
class CHILLFARMGAME_API UBiomeAssignmentPass : public UTileGenerationPass
{
	GENERATED_BODY()

public:
	virtual void Execute(FTileGenerationContext& Context) const override;
	virtual FName GetStepName() const override { return TEXT("Biomes"); }
	virtual FString GetSetupError(const FTileGenerationContext& Context) const override;
};


// Turns the height noise into elevation levels, per biome: the height above water is multiplied by
// the biome's ElevationScale, cut into levels of LevelStep, capped by the biome's MaxLevel and by
// MaxLevel here, and put on top of BaseLevel. Water from the Terrain pass sits one level below the
// base, so the ground has room below it for water and valleys. Between two biomes the scale and the cap are blended with the same weights that
// choose between the biomes, so elevation changes gradually across a border instead of jumping.
// Without a biome set every land tile uses a scale of 1. The Terraces pass then limits the steps.
UCLASS(meta = (DisplayName = "Elevation"))
class CHILLFARMGAME_API UElevationPass : public UTileGenerationPass
{
	GENERATED_BODY()

public:
	virtual void Execute(FTileGenerationContext& Context) const override;
	virtual FName GetStepName() const override { return TEXT("Elevation"); }

public:
	// Highest elevation above the base level anywhere. 0 keeps the world flat.
	UPROPERTY(EditAnywhere, Category = "Elevation", meta = (ClampMin = "0", ClampMax = "16"))
	int32 MaxLevel = 4;

	// The level of plain, low ground. Water sits one level below it, and lakes, rivers and valleys can
	// go down to level 0.
	UPROPERTY(EditAnywhere, Category = "Elevation", meta = (ClampMin = "0", ClampMax = "8"))
	int32 BaseLevel = 2;

	// Height noise span (above water) covered by one level at a biome scale of 1. Smaller values give
	// more, narrower terraces.
	UPROPERTY(EditAnywhere, Category = "Elevation", meta = (ClampMin = "0.01", ClampMax = "1"))
	float LevelStep = 0.15f;
};


// Adds water that follows the shape of the land: lakes in basins, and rivers that run downhill.
//
// The height noise is "filled" with a priority flood from the map edge and the existing water: every
// tile gets the lowest height at which water standing on it could run off, and the tile it would run
// off to. A group of tiles filled at least MinLakeDepth above the ground, and at least MinLakeSize big,
// is a basin and becomes a lake, at any height, one level below its lowest bank.
// Rivers start on high ground (picked by the seed, apart from each other) and follow the run-off
// directions to a lake, the sea or the map edge. Their surface sits one level below the banks and never
// rises downstream, so they drop where the land does; the Terraces pass then cuts the banks into valley
// walls. They start one tile wide and widen downstream and where rivers join.
// Rivers may cut land in two: crossing them is left to bridges.
UCLASS(meta = (DisplayName = "Lakes and Rivers"))
class CHILLFARMGAME_API ULakesAndRiversPass : public UTileGenerationPass
{
	GENERATED_BODY()

public:
	virtual void Execute(FTileGenerationContext& Context) const override;
	virtual FName GetStepName() const override { return TEXT("Water"); }

public:
	// How deep (in height noise, 0..1) a basin must be before it holds a lake.
	UPROPERTY(EditAnywhere, Category = "Lakes", meta = (ClampMin = "0.001", ClampMax = "1"))
	float MinLakeDepth = 0.02f;

	// Smallest lake, in tiles. Smaller basins stay dry.
	UPROPERTY(EditAnywhere, Category = "Lakes", meta = (ClampMin = "1"))
	int32 MinLakeSize = 16;

	// How many rivers to try to start. Fewer appear when there is not enough high ground.
	UPROPERTY(EditAnywhere, Category = "Rivers", meta = (ClampMin = "0", ClampMax = "64"))
	int32 RiverCount = 5;

	// A river starts where the height noise is at least this.
	UPROPERTY(EditAnywhere, Category = "Rivers", meta = (ClampMin = "0", ClampMax = "1"))
	float SourceMinHeight = 0.6f;

	// Smallest distance between two river sources, in tiles.
	UPROPERTY(EditAnywhere, Category = "Rivers", meta = (ClampMin = "1"))
	int32 MinSourceSpacing = 24;

	// Rivers shorter than this (in tiles, before reaching other water) are not made.
	UPROPERTY(EditAnywhere, Category = "Rivers", meta = (ClampMin = "1"))
	int32 MinRiverLength = 12;

	// A river gets one tile wider every this many tiles from its source.
	UPROPERTY(EditAnywhere, Category = "Rivers", meta = (ClampMin = "1"))
	int32 WidenEvery = 30;

	UPROPERTY(EditAnywhere, Category = "Rivers", meta = (ClampMin = "1", ClampMax = "5"))
	int32 MaxRiverWidth = 3;
};


// Chooses the surface type of every land tile with the rule set of its biome.
UCLASS(meta = (DisplayName = "Tile Types"))
class CHILLFARMGAME_API UTileTypePass : public UTileGenerationPass
{
	GENERATED_BODY()

public:
	virtual void Execute(FTileGenerationContext& Context) const override;
	virtual FName GetStepName() const override { return TEXT("Tile Types"); }
};


// Cuts elevation into terraces: direct neighbours never differ by more than MaxStep levels, and no
// terrace is smaller than MinTerraceSize tiles: smaller pieces of a level (specks, thin strips) take
// the level of the land around them. Pieces that only water surrounds stay.
UCLASS(meta = (DisplayName = "Terraces"))
class CHILLFARMGAME_API ULevelConstraintPass : public UTileGenerationPass
{
	GENERATED_BODY()

public:
	virtual void Execute(FTileGenerationContext& Context) const override;
	virtual FName GetStepName() const override { return TEXT("Terraces"); }

public:
	UPROPERTY(EditAnywhere, Category = "Elevation", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxStep = 1;

	// Smallest terrace, in tiles. 1 keeps every piece.
	UPROPERTY(EditAnywhere, Category = "Elevation", meta = (ClampMin = "1", ClampMax = "256"))
	int32 MinTerraceSize = 12;
};


// Turns land next to water into the biome's shore tile.
UCLASS(meta = (DisplayName = "Shores"))
class CHILLFARMGAME_API UShorePass : public UTileGenerationPass
{
	GENERATED_BODY()

public:
	virtual void Execute(FTileGenerationContext& Context) const override;
	virtual FName GetStepName() const override { return TEXT("Shores"); }
};


// Replaces isolated land tiles (at most one direct neighbour of the same type) so the map has no
// single-tile specks: with the biome's override for that type when one is set, otherwise with the
// type most of its neighbours have. Never creates or removes water.
UCLASS(meta = (DisplayName = "Tile Smoothing"))
class CHILLFARMGAME_API UTileSmoothingPass : public UTileGenerationPass
{
	GENERATED_BODY()

public:
	virtual void Execute(FTileGenerationContext& Context) const override;
	virtual FName GetStepName() const override { return TEXT("Smoothing"); }
};


// Rates how well every tile grows crops:
//   BaseFertility (biome) x SoilFertility (tile type, from the biome set) x a moisture factor.
// Runs last, so shores and smoothed tiles are rated by the type they end up with.
UCLASS(meta = (DisplayName = "Fertility"))
class CHILLFARMGAME_API UFertilityPass : public UTileGenerationPass
{
	GENERATED_BODY()

public:
	virtual void Execute(FTileGenerationContext& Context) const override;
	virtual FName GetStepName() const override { return TEXT("Fertility"); }
	virtual FString GetSetupError(const FTileGenerationContext& Context) const override;

public:
	// Moisture factor of a completely dry tile (moisture 0%).
	UPROPERTY(EditAnywhere, Category = "Moisture", meta = (ClampMin = "0"))
	float DryFactor = 0.6f;

	// Moisture factor of a completely wet tile (moisture 100%). Values in between are interpolated.
	UPROPERTY(EditAnywhere, Category = "Moisture", meta = (ClampMin = "0"))
	float WetFactor = 1.2f;
};

// Softens the terraces: where land meets land one level higher, the lower tile can slope up to it
// instead of ending in a cliff. Works on tile corners (see FTileData::Slope): a corner of a low tile is
// raised when one of the tiles around it is land one level higher and a noise field sampled at the
// corner says "soft" there. All tiles around a corner read the same noise, so they agree on its
// height and their surfaces meet. Runs of slope, with edge, inner and outer corner pieces, follow the
// noise; elsewhere cliffs stay.
// A tile that would end up with opposite corners raised (a one-tile valley or a saddle), or with all
// four, stays flat. Water, and tiles something stands on, never slope.
UCLASS(meta = (DisplayName = "Slopes"))
class CHILLFARMGAME_API USlopePass : public UTileGenerationPass
{
	GENERATED_BODY()

public:
	virtual void Execute(FTileGenerationContext& Context) const override;
	virtual FName GetStepName() const override { return TEXT("Slopes"); }

public:
	// Perlin sampling scale of the slope noise, per tile. Smaller values give longer runs of slope and of cliff.
	UPROPERTY(EditAnywhere, Category = "Slopes", meta = (ClampMin = "0.0001"))
	float NoiseScale = 0.12f;

	// A corner is soft where the noise (0..1) is below this. 0 keeps every cliff, 1 turns every cliff
	// that can be one into a slope.
	UPROPERTY(EditAnywhere, Category = "Slopes", meta = (ClampMin = "0", ClampMax = "1"))
	float SlopeAmount = 0.5f;

	// Range of the random noise offset derived from the seed.
	UPROPERTY(EditAnywhere, Category = "Slopes", meta = (ClampMin = "1"))
	float SeedOffsetRange = 1000.f;
};

// Joins the terraces so every piece of land can be walked to: finds the walkable areas (see
// TileReachability), starts from the largest one on each island and keeps placing a ramp between a
// reached and an unreached area until no more can be joined. A ramp is an edge slope on a flat lower
// tile, rising to the higher one, with flat ground behind it to walk on from. Land that only water
// separates stays apart.
// Runs after Slopes, so it only cuts into the cliffs that are left, and after Fertility: slopes and
// ramps keep the tile's type, so they do not change how it is rated.
UCLASS(meta = (DisplayName = "Ramps"))
class CHILLFARMGAME_API URampPass : public UTileGenerationPass
{
	GENERATED_BODY()

public:
	virtual void Execute(FTileGenerationContext& Context) const override;
	virtual FName GetStepName() const override { return TEXT("Ramps"); }
};
