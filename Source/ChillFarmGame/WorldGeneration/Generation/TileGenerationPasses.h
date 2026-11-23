#pragma once

#include "CoreMinimal.h"
#include "Generation/TileGenerationPass.h"
#include "TileGenerationPasses.generated.h"


// Samples the noise fields every later pass reads, marks water, and assigns elevation levels.
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

	// Highest elevation level. 0 keeps the world flat.
	UPROPERTY(EditAnywhere, Category = "Elevation", meta = (ClampMin = "0", ClampMax = "16"))
	int32 MaxLevel = 0;

	// Height noise span (above water) covered by one level. Smaller values give more, narrower terraces.
	UPROPERTY(EditAnywhere, Category = "Elevation", meta = (ClampMin = "0.01", ClampMax = "1"))
	float LevelStep = 0.15f;
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
