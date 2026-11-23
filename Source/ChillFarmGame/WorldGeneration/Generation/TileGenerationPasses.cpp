#include "Generation/TileGenerationPasses.h"

#include "Generation/TileGenerationContext.h"
#include "TileAlgorithms.h"
#include "TileNoiseContext.h"
#include "BiomeTypeData.h"
#include "BiomeTypeDataSet.h"
#include "TilesGenerationRuleSet.h"


namespace
{
	float Perlin01(const FVector2D& InPoint)
	{
		return FMath::Clamp(FMath::PerlinNoise2D(InPoint) * 0.5f + 0.5f, 0.f, 1.f);
	}

	const UBiomeTypeData* FindBiome(const FTileGenerationContext& Context, EBiomeType InType)
	{
		return Context.Biomes ? Context.Biomes->FindBiome(InType) : nullptr;
	}
}


/* Terrain */

void UTerrainNoisePass::Execute(FTileGenerationContext& Context) const
{
	const FTileGridLayout& Layout = Context.GetLayout();

	// Every offset comes from the seed, including the detail noise (it used to ignore the seed, so
	// biome borders had the same pattern in every world).
	FRandomStream Stream = Context.MakeStream(0x5EED7E44u);
	const auto RandomOffset = [&Stream, this]()
	{
		return FVector2D(Stream.FRandRange(0.f, SeedOffsetRange), Stream.FRandRange(0.f, SeedOffsetRange));
	};
	const FVector2D NoiseOffset = RandomOffset();
	const FVector2D WarpOffset = RandomOffset();
	const FVector2D MoistureOffset = RandomOffset();
	const FVector2D DetailOffset = RandomOffset();

	for (int32 Y = 0; Y < Layout.Size.Y; ++Y)
	{
		for (int32 X = 0; X < Layout.Size.X; ++X)
		{
			const FIntPoint Tile(X, Y);
			const int32 Index = Context.ToIndex(Tile);

			const double PX = X * NoiseScale + NoiseOffset.X;
			const double PY = Y * NoiseScale + NoiseOffset.Y;

			const float WarpX = FMath::PerlinNoise2D(FVector2D(PX + WarpOffset.X, PY + WarpOffset.Y));
			const float WarpY = FMath::PerlinNoise2D(FVector2D(PX - WarpOffset.Y, PY + WarpOffset.X));
			const FVector2D Warped(PX + WarpX * WarpStrength, PY + WarpY * WarpStrength);

			const float Height = Perlin01(Warped);
			const float Moisture = Perlin01(Warped + MoistureOffset);
			const float Detail = Perlin01(FVector2D(X * DetailNoiseScale + DetailOffset.X, Y * DetailNoiseScale + DetailOffset.Y));

			Context.HeightField[Index] = Height;
			Context.MoistureField[Index] = Moisture;
			Context.DetailField[Index] = Detail;

			if (Height <= WaterLevel)
			{
				Context.Mutate(Tile,
					FTileMutation().SetType(ETileType::Water).SetBiome(EBiomeType::Water).SetMoisture(100).SetHeight(0),
					TEXT("Below water level"));
				continue;
			}

			int32 Level = 0;
			if (MaxLevel > 0)
			{
				Level = FMath::Clamp(FMath::FloorToInt32((Height - WaterLevel) / LevelStep), 0, MaxLevel);
			}
			Context.Mutate(Tile, FTileMutation().SetHeight(static_cast<uint8>(Level)), TEXT("Elevation from height noise"));
		}
	}
}


/* Biomes */

FString UBiomeAssignmentPass::GetSetupError(const FTileGenerationContext& Context) const
{
	if (!Context.Biomes || Context.Biomes->BiomeData.IsEmpty())
	{
		return TEXT("No biome data set is assigned, or it has no biomes");
	}
	return FString();
}

void UBiomeAssignmentPass::Execute(FTileGenerationContext& Context) const
{
	if (!Context.Biomes) return;

	const FTileGridLayout& Layout = Context.GetLayout();
	for (int32 Index = 0; Index < Layout.NumTiles(); ++Index)
	{
		const FIntPoint Tile = Layout.ToTile(Index);
		if (Context.Grid.GetTile(Tile).IsWater()) continue;

		const float Height = Context.HeightField[Index];
		const float Moisture = Context.MoistureField[Index];

		// Inside a biome's ranges the biome wins outright. Between two biomes the detail noise decides,
		// which gives ragged borders instead of straight lines along the range limits.
		const SResolvedBiomeBlend Blend = Context.Biomes->ResolveBiomeBlend(Height, Moisture);
		const UBiomeTypeData* Biome = (Blend.Secondary && Context.DetailField[Index] < Blend.BlendAlpha) ? Blend.Secondary : Blend.Primary;
		if (!Biome) continue;

		Context.Mutate(Tile,
			FTileMutation()
				.SetBiome(Biome->BiomeType)
				.SetMoisture(TileData::ToPercent(Moisture)),
			Biome == Blend.Primary ? FName(TEXT("Closest biome")) : FName(TEXT("Border blend")));
	}
}


/* Tile types */

void UTileTypePass::Execute(FTileGenerationContext& Context) const
{
	const FTileGridLayout& Layout = Context.GetLayout();
	for (int32 Index = 0; Index < Layout.NumTiles(); ++Index)
	{
		const FIntPoint Tile = Layout.ToTile(Index);
		const FTileData& Data = Context.Grid.GetTile(Tile);
		if (Data.IsWater()) continue;

		const UBiomeTypeData* Biome = FindBiome(Context, Data.Biome);
		const UTilesGenerationRuleSet* RuleSet = Biome && Biome->TilesGenerationRuleSet ? Biome->TilesGenerationRuleSet : Context.DefaultRuleSet;

		const STileNoiseContext Noise{ Context.HeightField[Index], Context.MoistureField[Index], Context.DetailField[Index] };
		const ETileType Type = RuleSet ? RuleSet->ResolveTileType(Noise) : ETileType::Grass;

		Context.Mutate(Tile, FTileMutation().SetType(Type), RuleSet ? FName(TEXT("Biome rules")) : FName(TEXT("No rule set, default")));
	}
}


/* Terraces */

void ULevelConstraintPass::Execute(FTileGenerationContext& Context) const
{
	const FTileGridLayout& Layout = Context.GetLayout();
	const TConstArrayView<FTileData> Tiles = Context.Grid.GetTiles();

	TArray<uint8> Levels;
	Levels.SetNumUninitialized(Tiles.Num());
	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		Levels[Index] = Tiles[Index].Height;
	}

	if (TileAlgorithms::ClampLevelSteps(Levels, Layout.Size, static_cast<uint8>(MaxStep)) == 0) return;

	for (int32 Index = 0; Index < Levels.Num(); ++Index)
	{
		if (Levels[Index] == Context.Grid.GetTiles()[Index].Height) continue;
		Context.Mutate(Layout.ToTile(Index), FTileMutation().SetHeight(Levels[Index]), TEXT("Steeper than one level"));
	}
}


/* Shores */

void UShorePass::Execute(FTileGenerationContext& Context) const
{
	const FTileGridLayout& Layout = Context.GetLayout();

	// Collect first, change after: otherwise a new shore tile could influence its neighbours' decision.
	TArray<TPair<FIntPoint, ETileType>> Changes;
	for (int32 Index = 0; Index < Layout.NumTiles(); ++Index)
	{
		const FIntPoint Tile = Layout.ToTile(Index);
		const FTileData& Data = Context.Grid.GetTile(Tile);
		if (Data.IsWater() || !Context.Grid.HasNeighborOfType(Tile, ETileType::Water)) continue;

		const UBiomeTypeData* Biome = FindBiome(Context, Data.Biome);
		if (!Biome || Biome->ShoreTile == ETileType::Water || Biome->ShoreTile == ETileType::ETileType_MAX) continue;

		Changes.Emplace(Tile, Biome->ShoreTile);
	}

	for (const auto& [Tile, Type] : Changes)
	{
		Context.Mutate(Tile, FTileMutation().SetType(Type), TEXT("Next to water"));
	}
}


/* Smoothing */

void UTileSmoothingPass::Execute(FTileGenerationContext& Context) const
{
	const FTileGridLayout& Layout = Context.GetLayout();
	constexpr int32 NumTypes = static_cast<int32>(ETileType::ETileType_MAX);

	TArray<TPair<FIntPoint, ETileType>> Changes;
	for (int32 Index = 0; Index < Layout.NumTiles(); ++Index)
	{
		const FIntPoint Tile = Layout.ToTile(Index);
		const FTileData& Data = Context.Grid.GetTile(Tile);

		// Water and shores keep their shape: they are handled by their own passes.
		if (Data.IsWater() || Context.Grid.HasNeighborOfType(Tile, ETileType::Water)) continue;

		int32 SameNeighbors = 0;
		int32 TotalNeighbors = 0;
		int32 TypeCounts[NumTypes] = {};

		for (const FIntPoint& Offset : FTileGrid::GetCardinalOffsets())
		{
			const FTileData* Neighbor = Context.Grid.FindTile(Tile + Offset);
			if (!Neighbor || !Neighbor->IsGenerated()) continue;

			++TotalNeighbors;
			if (Neighbor->Type == Data.Type) ++SameNeighbors;
			else ++TypeCounts[static_cast<int32>(Neighbor->Type)];
		}

		if (TotalNeighbors <= 1 || SameNeighbors > 1) continue;

		ETileType Replacement = ETileType::ETileType_MAX;

		const UBiomeTypeData* Biome = FindBiome(Context, Data.Biome);
		if (const ETileType* Override = Biome ? Biome->SmoothingOverrides.Find(Data.Type) : nullptr)
		{
			Replacement = *Override;
		}
		else
		{
			int32 BestCount = 0;
			for (int32 TypeIndex = 0; TypeIndex < NumTypes; ++TypeIndex)
			{
				if (TypeCounts[TypeIndex] > BestCount)
				{
					BestCount = TypeCounts[TypeIndex];
					Replacement = static_cast<ETileType>(TypeIndex);
				}
			}
		}

		if (Replacement == ETileType::ETileType_MAX || Replacement == ETileType::Water || Replacement == Data.Type) continue;
		Changes.Emplace(Tile, Replacement);
	}

	for (const auto& [Tile, Type] : Changes)
	{
		Context.Mutate(Tile, FTileMutation().SetType(Type), TEXT("Isolated tile"));
	}
}


/* Fertility */

FString UFertilityPass::GetSetupError(const FTileGenerationContext& Context) const
{
	if (!Context.Biomes)
	{
		return TEXT("No biome data set is assigned, so there is no soil fertility table");
	}
	return FString();
}

void UFertilityPass::Execute(FTileGenerationContext& Context) const
{
	if (!Context.Biomes) return;

	const FTileGridLayout& Layout = Context.GetLayout();
	for (int32 Index = 0; Index < Layout.NumTiles(); ++Index)
	{
		const FIntPoint Tile = Layout.ToTile(Index);
		const FTileData& Data = Context.Grid.GetTile(Tile);
		if (!Data.IsGenerated()) continue;

		const UBiomeTypeData* Biome = FindBiome(Context, Data.Biome);
		const float BaseFertility = Biome ? static_cast<float>(Biome->BaseFertility) : 0.f;
		const float Soil = Context.Biomes->GetSoilFertility(Data.Type);

		// The stored moisture, not the noise field, so the same formula works later for runtime changes.
		const float MoistureFactor = FMath::Lerp(DryFactor, WetFactor, Data.Moisture / 100.f);

		const float Fertility = BaseFertility * Soil * MoistureFactor;
		Context.Mutate(Tile, FTileMutation().SetFertility(TileData::ToPercent(Fertility / 100.f)),
			Soil > 0.f ? FName(TEXT("Biome, soil and moisture")) : FName(TEXT("Nothing grows on this surface")));
	}
}
