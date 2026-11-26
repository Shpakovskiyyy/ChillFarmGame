#include "Generation/TileGenerationPasses.h"

#include "Generation/TileGenerationContext.h"
#include "TileAlgorithms.h"
#include "TileReachability.h"
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

	// Whether both corners of the tile's edge towards InOffset are at InLevel.
	bool IsEdgeAtLevel(const FTileData& InTile, const FIntPoint& InOffset, int32 InLevel)
	{
		ETileCorner First, Second;
		TileData::GetEdgeCorners(InOffset, First, Second);
		return TileData::GetCornerLevel(InTile, First) == InLevel && TileData::GetCornerLevel(InTile, Second) == InLevel;
	}

	bool CanChangeShape(const FTileData& InTile)
	{
		return TileReachability::IsWalkable(InTile) && !InTile.HasFlag(ETileFlags::Occupied);
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
	Context.WaterLevel = WaterLevel;

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
			}
		}
	}
}


/* Elevation */

void UElevationPass::Execute(FTileGenerationContext& Context) const
{
	const FTileGridLayout& Layout = Context.GetLayout();
	for (int32 Index = 0; Index < Layout.NumTiles(); ++Index)
	{
		const FIntPoint Tile = Layout.ToTile(Index);
		if (Context.Grid.GetTile(Tile).IsWater())
		{
			// The low lakes from the height noise: one level below the ground.
			Context.Mutate(Tile, FTileMutation().SetHeight(static_cast<uint8>(FMath::Max(BaseLevel - 1, 0))), TEXT("Water below the ground"));
			continue;
		}

		float Scale = 1.f;
		float BiomeCap = static_cast<float>(MaxLevel);
		if (Context.Biomes)
		{
			const SResolvedBiomeBlend Blend = Context.Biomes->ResolveBiomeBlend(Context.HeightField[Index], Context.MoistureField[Index]);
			if (Blend.Primary)
			{
				// The continuous blend, not the biome the tile ended up with: neighbours across a border
				// change elevation step by step instead of jumping where the dithered border runs.
				const UBiomeTypeData* Secondary = Blend.Secondary ? Blend.Secondary : Blend.Primary;
				Scale = FMath::Lerp(Blend.Primary->ElevationScale, Secondary->ElevationScale, Blend.BlendAlpha);
				BiomeCap = FMath::Lerp(static_cast<float>(Blend.Primary->MaxLevel), static_cast<float>(Secondary->MaxLevel), Blend.BlendAlpha);
			}
		}

		const int32 Cap = FMath::Min(MaxLevel, FMath::RoundToInt32(BiomeCap));
		const float AboveWater = FMath::Max(Context.HeightField[Index] - Context.WaterLevel, 0.f);
		const int32 Level = FMath::Clamp(FMath::FloorToInt32(AboveWater * Scale / LevelStep), 0, FMath::Max(Cap, 0));
		Context.Mutate(Tile, FTileMutation().SetHeight(static_cast<uint8>(BaseLevel + Level)), TEXT("Elevation from height noise and biome"));
	}
}


/* Lakes and rivers */

void ULakesAndRiversPass::Execute(FTileGenerationContext& Context) const
{
	const FTileGrid& Grid = Context.Grid;
	const FTileGridLayout& Layout = Context.GetLayout();
	const int32 NumTiles = Layout.NumTiles();
	if (NumTiles == 0 || Context.HeightField.Num() != NumTiles) return;

	// Priority flood: from the map edge and the existing water inwards, lowest first. Filled[i] is the
	// lowest height at which water on tile i could run off; Downstream[i] is where it runs to (none for
	// the starting tiles). A tiny rise per step keeps water running across flat filled ground.
	constexpr float FlatRise = 1e-5f;
	TArray<float> Filled;
	Filled.Init(MAX_flt, NumTiles);
	TArray<int32> Downstream;
	Downstream.Init(INDEX_NONE, NumTiles);

	struct FFloodNode
	{
		float Height;
		int32 Index;
		bool operator<(const FFloodNode& Other) const { return Height < Other.Height || (Height == Other.Height && Index < Other.Index); }
	};
	TArray<FFloodNode> Open;
	for (int32 Index = 0; Index < NumTiles; ++Index)
	{
		const FIntPoint Tile = Layout.ToTile(Index);
		const bool bEdge = Tile.X == 0 || Tile.Y == 0 || Tile.X == Layout.Size.X - 1 || Tile.Y == Layout.Size.Y - 1;
		if (!bEdge && !Grid.GetTile(Tile).IsWater()) continue;
		Filled[Index] = Context.HeightField[Index];
		Open.HeapPush({ Filled[Index], Index });
	}

	TArray<bool> Done;
	Done.Init(false, NumTiles);
	while (!Open.IsEmpty())
	{
		FFloodNode Node;
		Open.HeapPop(Node, EAllowShrinking::No);
		if (Done[Node.Index]) continue;
		Done[Node.Index] = true;

		const FIntPoint Tile = Layout.ToTile(Node.Index);
		for (const FIntPoint& Offset : FTileGrid::GetCardinalOffsets())
		{
			const FIntPoint Next = Tile + Offset;
			if (!Layout.Contains(Next)) continue;
			const int32 NextIndex = Layout.ToIndex(Next);
			if (Done[NextIndex]) continue;

			const float Height = FMath::Max(Context.HeightField[NextIndex], Filled[Node.Index] + FlatRise);
			if (Height >= Filled[NextIndex]) continue;
			Filled[NextIndex] = Height;
			Downstream[NextIndex] = Node.Index;
			Open.HeapPush({ Height, NextIndex });
		}
	}

	// Levels of the land as it is before any new water, for banks.
	TArray<int32> LandLevel;
	LandLevel.SetNumUninitialized(NumTiles);
	for (int32 Index = 0; Index < NumTiles; ++Index) LandLevel[Index] = Grid.GetTiles()[Index].Height;

	TArray<bool> IsWater;
	IsWater.SetNumUninitialized(NumTiles);
	for (int32 Index = 0; Index < NumTiles; ++Index) IsWater[Index] = Grid.GetTiles()[Index].IsWater();

	TMap<int32, uint8> NewWater;
	TMap<int32, FName> NewWaterReason;
	const auto LowestLandAround = [&](int32 Index)
	{
		int32 Lowest = MAX_int32;
		const FIntPoint Tile = Layout.ToTile(Index);
		for (const FIntPoint& Offset : FTileGrid::GetCardinalOffsets())
		{
			const FIntPoint Next = Tile + Offset;
			if (!Layout.Contains(Next)) continue;
			const int32 NextIndex = Layout.ToIndex(Next);
			if (!IsWater[NextIndex] && !NewWater.Contains(NextIndex)) Lowest = FMath::Min(Lowest, LandLevel[NextIndex]);
		}
		return Lowest;
	};

	// Lakes: groups of tiles the flood filled deep enough.
	TArray<bool> Visited;
	Visited.Init(false, NumTiles);
	TArray<int32> Group;
	for (int32 Start = 0; Start < NumTiles; ++Start)
	{
		if (Visited[Start] || IsWater[Start] || Filled[Start] - Context.HeightField[Start] < MinLakeDepth) continue;

		Group.Reset();
		Group.Add(Start);
		Visited[Start] = true;
		for (int32 Head = 0; Head < Group.Num(); ++Head)
		{
			const FIntPoint Tile = Layout.ToTile(Group[Head]);
			for (const FIntPoint& Offset : FTileGrid::GetCardinalOffsets())
			{
				const FIntPoint Next = Tile + Offset;
				if (!Layout.Contains(Next)) continue;
				const int32 NextIndex = Layout.ToIndex(Next);
				if (Visited[NextIndex] || IsWater[NextIndex] || Filled[NextIndex] - Context.HeightField[NextIndex] < MinLakeDepth) continue;
				Visited[NextIndex] = true;
				Group.Add(NextIndex);
			}
		}
		if (Group.Num() < MinLakeSize) continue;

		// One level below the lowest bank, so the surface is flat and never above the land around it.
		int32 Bank = MAX_int32;
		for (const int32 Index : Group) Bank = FMath::Min(Bank, LowestLandAround(Index));
		if (Bank == MAX_int32) Bank = LandLevel[Group[0]];
		const uint8 Surface = static_cast<uint8>(FMath::Max(Bank - 1, 0));
		for (const int32 Index : Group)
		{
			NewWater.Add(Index, Surface);
			NewWaterReason.Add(Index, TEXT("Fills a basin"));
		}
	}
	for (const TPair<int32, uint8>& Entry : NewWater) IsWater[Entry.Key] = true;

	// River sources: high land, in an order the seed picks, apart from each other. Not on the map edge,
	// where water runs off the map at once.
	FRandomStream Stream = Context.MakeStream(0x21FE4A11u);
	TArray<int32> Candidates;
	for (int32 Index = 0; Index < NumTiles; ++Index)
	{
		const FIntPoint Tile = Layout.ToTile(Index);
		const bool bEdge = Tile.X == 0 || Tile.Y == 0 || Tile.X == Layout.Size.X - 1 || Tile.Y == Layout.Size.Y - 1;
		if (!bEdge && !IsWater[Index] && Context.HeightField[Index] >= SourceMinHeight) Candidates.Add(Index);
	}
	for (int32 Index = Candidates.Num() - 1; Index > 0; --Index) Candidates.Swap(Index, Stream.RandRange(0, Index));

	TArray<int32> Sources;
	for (const int32 Candidate : Candidates)
	{
		if (Sources.Num() >= RiverCount) break;
		const FIntPoint Tile = Layout.ToTile(Candidate);
		const bool bFarEnough = !Sources.ContainsByPredicate([&](int32 Source)
		{
			const FIntPoint Other = Layout.ToTile(Source);
			return FMath::Max(FMath::Abs(Other.X - Tile.X), FMath::Abs(Other.Y - Tile.Y)) < MinSourceSpacing;
		});
		if (bFarEnough) Sources.Add(Candidate);
	}

	// Follow every river down to water. Flow counts the rivers through a tile (2 or more after a join),
	// Length the distance from the source, so the river can widen.
	TArray<int32> Flow;
	Flow.Init(0, NumTiles);
	TArray<int32> Length;
	Length.Init(0, NumTiles);
	TArray<int32> Surface;
	Surface.Init(MAX_int32, NumTiles);
	TArray<TArray<int32>> Rivers;
	for (const int32 Source : Sources)
	{
		TArray<int32> Path;
		int32 Current = Source;
		int32 OwnLength = 0;
		while (Current != INDEX_NONE && !IsWater[Current])
		{
			Path.Add(Current);
			if (Flow[Current] == 0) ++OwnLength;
			Current = Downstream[Current];
		}
		if (OwnLength < MinRiverLength) continue;

		int32 Level = MAX_int32;
		for (int32 Step = 0; Step < Path.Num(); ++Step)
		{
			const int32 Index = Path[Step];
			// Never rises downstream, one level below the ground here, never above a bank.
			Level = FMath::Min3(Level, LandLevel[Index] - 1, LowestLandAround(Index));
			Level = FMath::Max(Level, 0);
			Surface[Index] = FMath::Min(Surface[Index], Level);
			Level = Surface[Index];
			++Flow[Index];
			Length[Index] = FMath::Max(Length[Index], Step);
		}
		Rivers.Add(MoveTemp(Path));
	}

	for (const TArray<int32>& Path : Rivers)
	{
		for (int32 Step = 0; Step < Path.Num(); ++Step)
		{
			const int32 Index = Path[Step];
			const uint8 Level = static_cast<uint8>(Surface[Index]);
			if (!NewWater.Contains(Index))
			{
				NewWater.Add(Index, Level);
				NewWaterReason.Add(Index, TEXT("River"));
			}

			const int32 Width = FMath::Min(MaxRiverWidth, 1 + Length[Index] / WidenEvery + (Flow[Index] > 1 ? 1 : 0));
			if (Width <= 1) continue;

			// Widen across the direction of flow: the right bank first, then the left.
			const FIntPoint Tile = Layout.ToTile(Index);
			const int32 NextIndex = Step + 1 < Path.Num() ? Path[Step + 1] : Downstream[Index];
			const FIntPoint Direction = NextIndex != INDEX_NONE ? Layout.ToTile(NextIndex) - Tile : FIntPoint(1, 0);
			const FIntPoint Right(-Direction.Y, Direction.X);
			for (int32 Extra = 1; Extra < Width; ++Extra)
			{
				const int32 Side = (Extra + 1) / 2;
				const FIntPoint Bank = Tile + (Extra % 2 == 1 ? Right * Side : FIntPoint::ZeroValue - Right * Side);
				if (!Layout.Contains(Bank)) continue;
				const int32 BankIndex = Layout.ToIndex(Bank);
				if (IsWater[BankIndex] || NewWater.Contains(BankIndex)) continue;
				NewWater.Add(BankIndex, static_cast<uint8>(FMath::Max(FMath::Min(static_cast<int32>(Level), LandLevel[BankIndex] - 1), 0)));
				NewWaterReason.Add(BankIndex, TEXT("River"));
			}
		}
	}

	// Apply in grid order, so the history reads the same for the same seed.
	TArray<int32> Changed;
	NewWater.GetKeys(Changed);
	Changed.Sort();
	for (const int32 Index : Changed)
	{
		Context.Mutate(Layout.ToTile(Index),
			FTileMutation().SetType(ETileType::Water).SetBiome(EBiomeType::Water).SetMoisture(100).SetHeight(NewWater[Index]),
			NewWaterReason[Index]);
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

	TileAlgorithms::ClampLevelSteps(Levels, Layout.Size, static_cast<uint8>(MaxStep));

	// Merge terraces that are too small, then clamp again: a merge can put a tile two levels from a
	// neighbour, and a clamp can leave a new small piece behind, so repeat until both are settled.
	TArray<bool> IsLand;
	IsLand.SetNumUninitialized(Tiles.Num());
	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		IsLand[Index] = Tiles[Index].IsGenerated() && !Tiles[Index].IsWater();
	}

	TArray<bool> Merged;
	Merged.Init(false, Tiles.Num());
	TArray<uint8> BeforeMerge;
	constexpr int32 MaxRounds = 8;
	for (int32 Round = 0; Round < MaxRounds && MinTerraceSize > 1; ++Round)
	{
		BeforeMerge = Levels;
		const int32 MergedTiles = TileAlgorithms::MergeSmallAreas(Levels, IsLand, Layout.Size, MinTerraceSize);
		for (int32 Index = 0; Index < Levels.Num(); ++Index)
		{
			if (Levels[Index] != BeforeMerge[Index]) Merged[Index] = true;
		}
		const int32 ClampedTiles = TileAlgorithms::ClampLevelSteps(Levels, Layout.Size, static_cast<uint8>(MaxStep));
		if (MergedTiles == 0 && ClampedTiles == 0) break;
	}

	for (int32 Index = 0; Index < Levels.Num(); ++Index)
	{
		if (Levels[Index] == Tiles[Index].Height) continue;
		Context.Mutate(Layout.ToTile(Index), FTileMutation().SetHeight(Levels[Index]),
			Merged[Index] ? FName(TEXT("Too small for a terrace")) : FName(TEXT("Steeper than one level")));
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

		// The biome's override is a preference, not a command: it only wins when a neighbour already has
		// that type. Otherwise a lone tile would become a lone tile of another type, which is still an island.
		const UBiomeTypeData* Biome = FindBiome(Context, Data.Biome);
		const ETileType* Override = Biome ? Biome->SmoothingOverrides.Find(Data.Type) : nullptr;
		if (Override && *Override < ETileType::ETileType_MAX && TypeCounts[static_cast<int32>(*Override)] > 0)
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


/* Slopes */

void USlopePass::Execute(FTileGenerationContext& Context) const
{
	const FTileGrid& Grid = Context.Grid;
	const FTileGridLayout& Layout = Context.GetLayout();

	FRandomStream Stream = Context.MakeStream(0x51097E5Au);
	const FVector2D NoiseOffset(Stream.FRandRange(0.f, SeedOffsetRange), Stream.FRandRange(0.f, SeedOffsetRange));

	// Corners are numbered like tiles: corner (X, Y) is the north-west corner of tile (X, Y).
	const auto IsSoft = [this, &NoiseOffset](const FIntPoint& InCorner)
	{
		return Perlin01(FVector2D(InCorner.X * NoiseScale + NoiseOffset.X, InCorner.Y * NoiseScale + NoiseOffset.Y)) < SlopeAmount;
	};

	// Every tile is decided from the grid as it was before the pass: only Slope changes, and no
	// decision reads it, so the order does not matter.
	TArray<TPair<FIntPoint, uint8>> Slopes;
	for (int32 Index = 0; Index < Layout.NumTiles(); ++Index)
	{
		const FIntPoint Tile = Layout.ToTile(Index);
		const FTileData& Data = Grid.GetTile(Tile);
		if (!CanChangeShape(Data) || Data.IsSlope()) continue;

		uint8 Mask = 0;
		for (const ETileCorner Corner : TileData::AllCorners)
		{
			// The three other tiles that share this corner.
			const FIntPoint Toward = TileData::GetCornerOffset(Corner);
			const FIntPoint Around[] = { Tile + FIntPoint(Toward.X, 0), Tile + FIntPoint(0, Toward.Y), Tile + Toward };

			bool bNextToHigher = false;
			for (const FIntPoint& Other : Around)
			{
				const FTileData* OtherData = Grid.FindTile(Other);
				bNextToHigher |= OtherData && TileReachability::IsWalkable(*OtherData) && OtherData->Height == Data.Height + 1;
			}
			if (!bNextToHigher) continue;

			const FIntPoint CornerPoint(Tile.X + (Toward.X > 0 ? 1 : 0), Tile.Y + (Toward.Y > 0 ? 1 : 0));
			if (IsSoft(CornerPoint)) Mask |= static_cast<uint8>(Corner);
		}

		const ETileSlopeShape Shape = TileData::GetSlopeShape(Mask);
		if (Shape == ETileSlopeShape::Flat || Shape == ETileSlopeShape::Invalid) continue;
		Slopes.Add({ Tile, Mask });
	}

	for (const auto& [Tile, Mask] : Slopes)
	{
		const ETileSlopeShape Shape = TileData::GetSlopeShape(Mask);
		const TCHAR* Reason = Shape == ETileSlopeShape::Edge ? TEXT("Slopes up to the higher ground")
			: Shape == ETileSlopeShape::OuterCorner ? TEXT("Rounds the outside of a hill's corner")
			: TEXT("Fills the inside of a hill's corner");
		Context.Mutate(Tile, FTileMutation().SetSlope(Mask), Reason);
	}
}


/* Ramps */

void URampPass::Execute(FTileGenerationContext& Context) const
{
	const FTileGrid& Grid = Context.Grid;
	const FTileGridLayout& Layout = Context.GetLayout();

	TArray<int32> Regions;
	const int32 NumRegions = TileReachability::ComputeWalkRegions(Grid, Regions);
	if (NumRegions <= 1) return;

	// Every island starts from its largest area (the lowest index wins a tie, so it is deterministic).
	// Ramps never cross water, so an island that is not given a start of its own would never be joined.
	TArray<int32> Islands;
	const int32 NumIslands = TileReachability::ComputeLandIslands(Grid, Islands);

	TArray<int32> RegionSizes;
	RegionSizes.Init(0, NumRegions);
	TArray<int32> RegionIsland;
	RegionIsland.Init(INDEX_NONE, NumRegions);
	for (int32 Index = 0; Index < Regions.Num(); ++Index)
	{
		if (Regions[Index] == INDEX_NONE) continue;
		++RegionSizes[Regions[Index]];
		RegionIsland[Regions[Index]] = Islands[Index];
	}

	TArray<int32> IslandStart;
	IslandStart.Init(INDEX_NONE, NumIslands);
	for (int32 Region = 0; Region < NumRegions; ++Region)
	{
		int32& Start = IslandStart[RegionIsland[Region]];
		if (Start == INDEX_NONE || RegionSizes[Region] > RegionSizes[Start]) Start = Region;
	}

	// Every place a ramp could join two different areas: a flat land tile with higher ground in front
	// of it, whose edge is one level up, and ground behind it on the tile's own level, so the ramp
	// can be walked onto.
	struct FCandidate
	{
		FIntPoint Tile;
		FIntPoint Offset;
		int32 LowRegion;
		int32 HighRegion;
	};
	const auto CanPlace = [&Grid](const FIntPoint& InTile, const FIntPoint& InOffset)
	{
		const FTileData& Data = Grid.GetTile(InTile);
		const FTileData* High = Grid.FindTile(InTile + InOffset);
		const FTileData* Back = Grid.FindTile(InTile - InOffset);
		return CanChangeShape(Data) && !Data.IsSlope()
			&& High && TileReachability::IsWalkable(*High) && IsEdgeAtLevel(*High, FIntPoint::ZeroValue - InOffset, Data.Height + 1)
			&& Back && TileReachability::IsWalkable(*Back) && IsEdgeAtLevel(*Back, InOffset, Data.Height);
	};

	TArray<FCandidate> Candidates;
	for (int32 Index = 0; Index < Layout.NumTiles(); ++Index)
	{
		const FIntPoint Tile = Layout.ToTile(Index);
		if (Regions[Index] == INDEX_NONE) continue;

		for (const FIntPoint& Offset : FTileGrid::GetCardinalOffsets())
		{
			if (!CanPlace(Tile, Offset)) continue;

			const int32 HighRegion = Regions[Layout.ToIndex(Tile + Offset)];
			if (HighRegion == Regions[Index]) continue;
			Candidates.Add({ Tile, Offset, Regions[Index], HighRegion });
		}
	}

	TArray<bool> Reached;
	Reached.Init(false, NumRegions);
	for (const int32 Start : IslandStart) Reached[Start] = true;

	FRandomStream Stream = Context.MakeStream(0x4A3B9001u);
	TArray<int32> Valid;
	while (true)
	{
		Valid.Reset();
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const FCandidate& Candidate = Candidates[Index];
			if (Reached[Candidate.LowRegion] == Reached[Candidate.HighRegion]) continue;
			// An earlier ramp may have used the tile, or the ground in front of or behind it.
			if (!CanPlace(Candidate.Tile, Candidate.Offset)) continue;
			Valid.Add(Index);
		}
		if (Valid.IsEmpty()) break;

		// Any valid place joins one more area; picking from the seed spreads ramps around instead of
		// always using the first edge in grid order.
		const FCandidate& Chosen = Candidates[Valid[Stream.RandRange(0, Valid.Num() - 1)]];
		Context.Mutate(Chosen.Tile, FTileMutation().SetSlope(TileData::MakeEdgeSlope(Chosen.Offset)), TEXT("Joins a terrace to the rest of the land"));

		Reached[Chosen.LowRegion] = true;
		Reached[Chosen.HighRegion] = true;
	}
}
