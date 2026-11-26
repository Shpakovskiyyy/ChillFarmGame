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
// the biome's ElevationScale, cut into levels of LevelStep, and capped by the biome's MaxLevel and by
// MaxLevel here. Between two biomes the scale and the cap are blended with the same weights that
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
	// Highest elevation level anywhere. 0 keeps the world flat.
	UPROPERTY(EditAnywhere, Category = "Elevation", meta = (ClampMin = "0", ClampMax = "16"))
	int32 MaxLevel = 4;

	// Height noise span (above water) covered by one level at a biome scale of 1. Smaller values give
	// more, narrower terraces.
	UPROPERTY(EditAnywhere, Category = "Elevation", meta = (ClampMin = "0.01", ClampMax = "1"))
	float LevelStep = 0.15f;
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


// Cuts elevation into terraces: direct neighbours never differ by more than MaxStep levels.
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
