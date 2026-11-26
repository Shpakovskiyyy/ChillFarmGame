// Specs for world generation: the context, the pipeline runner, every pass, and the full pipeline.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TileTestUtils.h"
#include "Generation/TileGenerationContext.h"
#include "Generation/TileGenerationPass.h"
#include "Generation/TileGenerationPasses.h"
#include "TileReachability.h"
#include "TileCliffs.h"
#include "TileAlgorithms.h"

using namespace TileTestUtils;

namespace TileGenerationSpecPrivate
{
	// The default pipeline, in the order the generator uses.
	struct FDefaultPasses
	{
		UTerrainNoisePass* Terrain = nullptr;
		UBiomeAssignmentPass* Biomes = nullptr;
		UElevationPass* Elevation = nullptr;
		ULakesAndRiversPass* Water = nullptr;
		UTileTypePass* TileTypes = nullptr;
		ULevelConstraintPass* Terraces = nullptr;
		UShorePass* Shores = nullptr;
		UTileSmoothingPass* Smoothing = nullptr;
		UFertilityPass* Fertility = nullptr;
		USlopePass* Slopes = nullptr;
		URampPass* Ramps = nullptr;

		TArray<const UTileGenerationPass*> List() const
		{
			return { Terrain, Biomes, Elevation, Water, TileTypes, Terraces, Shores, Smoothing, Fertility, Slopes, Ramps };
		}
	};

	FDefaultPasses MakeDefaultPasses(int32 InMaxLevel)
	{
		FDefaultPasses Passes;
		Passes.Terrain = NewObject<UTerrainNoisePass>();
		// Higher than the game default so even small test maps have both water and land.
		Passes.Terrain->WaterLevel = 0.45f;
		Passes.Biomes = NewObject<UBiomeAssignmentPass>();
		Passes.Elevation = NewObject<UElevationPass>();
		Passes.Elevation->MaxLevel = InMaxLevel;
		Passes.Water = NewObject<ULakesAndRiversPass>();
		Passes.TileTypes = NewObject<UTileTypePass>();
		Passes.Terraces = NewObject<ULevelConstraintPass>();
		Passes.Shores = NewObject<UShorePass>();
		Passes.Smoothing = NewObject<UTileSmoothingPass>();
		Passes.Fertility = NewObject<UFertilityPass>();
		Passes.Slopes = NewObject<USlopePass>();
		Passes.Ramps = NewObject<URampPass>();
		return Passes;
	}

	// Runs the whole default pipeline on a fresh grid and returns it.
	FTileGrid GenerateWorld(int32 InSeed, const FIntPoint& InSize, int32 InMaxLevel, TArray<FTileChange>* OutChanges = nullptr)
	{
		const FTestWorldData Data = MakeTestWorldData();
		const FDefaultPasses Passes = MakeDefaultPasses(InMaxLevel);

		FTileGrid Grid(MakeLayout(InSize.X, InSize.Y));
		FTileGenerationContext Context(Grid, InSeed);
		Context.Biomes = Data.Biomes;
		Context.DefaultRuleSet = Data.DefaultRules;
		if (OutChanges)
		{
			Context.OnChange = [OutChanges](const FTileChange& Change) { OutChanges->Add(Change); };
		}

		TileGeneration::RunPasses(Passes.List(), Context);
		return Grid;
	}

	// Fills the noise fields of a context with one value per tile.
	void FillField(TArray<float>& OutField, std::initializer_list<float> InValues)
	{
		int32 Index = 0;
		for (const float Value : InValues) OutField[Index++] = Value;
	}
}

using namespace TileGenerationSpecPrivate;


BEGIN_DEFINE_SPEC(FTileGenerationSpec, "ChillFarmGame.TileWorld.Generation", TileTestUtils::SpecFlags)
END_DEFINE_SPEC(FTileGenerationSpec)

void FTileGenerationSpec::Define()
{
	Describe("FTileGenerationContext", [this]()
	{
		It("sizes its noise fields to the grid", [this]()
		{
			FTileGrid Grid(MakeLayout(6, 4));
			const FTileGenerationContext Context(Grid, 1);
			TestEq(*this, TEXT("Height field"), Context.HeightField.Num(), 24);
			TestEq(*this, TEXT("Moisture field"), Context.MoistureField.Num(), 24);
			TestEq(*this, TEXT("Detail field"), Context.DetailField.Num(), 24);

			bool bAllZero = true;
			for (const float Value : Context.HeightField) bAllZero &= Value == 0.f;
			TestTrue(TEXT("Fields start at zero"), bAllZero);
		});

		It("applies mutations to its grid without a listener", [this]()
		{
			FTileGrid Grid(MakeLayout(3, 3));
			FTileGenerationContext Context(Grid, 1);
			TestEq(*this, TEXT("Changed"), Context.Mutate(FIntPoint(1, 1), FTileMutation().SetHeight(2)), ETileField::Height);
			TestEq(*this, TEXT("Applied"), Grid.GetTile(FIntPoint(1, 1)).Height, static_cast<uint8>(2));
		});

		It("describes every change to its listener", [this]()
		{
			FTileGrid Grid(MakeLayout(3, 3));
			FTileGenerationContext Context(Grid, 1);
			UObject* Instigator = NewObject<UTerrainNoisePass>();
			Context.Instigator = Instigator;
			Context.CurrentStep = TEXT("Test Step");

			TArray<FTileChange> Changes;
			Context.OnChange = [&Changes](const FTileChange& Change) { Changes.Add(Change); };

			Context.Mutate(FIntPoint(2, 1), FTileMutation().SetType(ETileType::Rock).SetFertility(5), TEXT("Test Reason"));
			Context.Mutate(FIntPoint(2, 1), FTileMutation().SetType(ETileType::Rock), TEXT("No change"));

			if (!TestEq(*this, TEXT("One change reported, the no-op is not"), Changes.Num(), 1)) return;

			const FTileChange& Change = Changes[0];
			TestEq(*this, TEXT("Coordinates"), Change.Coordinates, FIntPoint(2, 1));
			TestEq(*this, TEXT("Fields"), Change.ChangedFields, ETileField::Type | ETileField::Fertility);
			TestFalse(TEXT("Before"), Change.Before.IsGenerated());
			TestEq(*this, TEXT("After"), Change.After.Type, ETileType::Rock);
			TestEq(*this, TEXT("Source"), Change.Context.Source, ETileMutationSource::WorldGeneration);
			TestTrue(TEXT("Step"), Change.Context.Step == FName(TEXT("Test Step")));
			TestTrue(TEXT("Reason"), Change.Context.Reason == FName(TEXT("Test Reason")));
			TestTrue(TEXT("Instigator"), Change.Context.Instigator.Get() == Instigator);
		});

		It("gives each seed and salt its own random numbers", [this]()
		{
			FTileGrid Grid(MakeLayout(1, 1));
			const FTileGenerationContext Seed1(Grid, 1);
			const FTileGenerationContext Seed2(Grid, 2);

			FRandomStream A = Seed1.MakeStream(10);
			FRandomStream B = Seed1.MakeStream(10);
			FRandomStream OtherSalt = Seed1.MakeStream(11);
			FRandomStream OtherSeed = Seed2.MakeStream(10);

			const int32 First = A.RandRange(0, MAX_int32 - 1);
			TestEq(*this, TEXT("Same seed and salt"), B.RandRange(0, MAX_int32 - 1), First);
			TestTrue(TEXT("Other salt"), OtherSalt.RandRange(0, MAX_int32 - 1) != First);
			TestTrue(TEXT("Other seed"), OtherSeed.RandRange(0, MAX_int32 - 1) != First);
		});
	});

	Describe("TileGeneration::RunPasses", [this]()
	{
		It("runs the passes in order and names each change after its pass", [this]()
		{
			TArray<FTileChange> Changes;
			GenerateWorld(3, FIntPoint(48, 48), 4, &Changes);

			// A pass that finds nothing to do makes no changes, so only the order is checked:
			// steps must appear in pipeline order, each in one uninterrupted block.
			const TArray<FName> Pipeline = { TEXT("Terrain"), TEXT("Biomes"), TEXT("Elevation"), TEXT("Water"), TEXT("Tile Types"), TEXT("Terraces"), TEXT("Shores"), TEXT("Smoothing"), TEXT("Fertility"), TEXT("Slopes"), TEXT("Ramps") };

			int32 LastStepIndex = -1;
			bool bInOrder = true;
			bool bKnownSteps = true;
			for (const FTileChange& Change : Changes)
			{
				const int32 StepIndex = Pipeline.IndexOfByKey(Change.Context.Step);
				bKnownSteps &= StepIndex != INDEX_NONE;
				bInOrder &= StepIndex >= LastStepIndex;
				LastStepIndex = FMath::Max(LastStepIndex, StepIndex);
			}

			TestTrue(TEXT("Every change names a pipeline step"), bKnownSteps);
			TestTrue(TEXT("Steps run in pipeline order"), bInOrder);
			TestTrue(TEXT("The first three passes always change tiles"),
				Changes.ContainsByPredicate([](const FTileChange& C) { return C.Context.Step == FName(TEXT("Terrain")); }) &&
				Changes.ContainsByPredicate([](const FTileChange& C) { return C.Context.Step == FName(TEXT("Biomes")); }) &&
				Changes.ContainsByPredicate([](const FTileChange& C) { return C.Context.Step == FName(TEXT("Tile Types")); }));
		});

		It("skips disabled and null passes", [this]()
		{
			UTerrainNoisePass* Terrain = NewObject<UTerrainNoisePass>();
			Terrain->bEnabled = false;
			const UTileGenerationPass* Passes[] = { nullptr, Terrain };

			FTileGrid Grid(MakeLayout(8, 8));
			FTileGenerationContext Context(Grid, 1);
			TestTrue(TEXT("Runs"), TileGeneration::RunPasses(Passes, Context));

			bool bUntouched = true;
			for (const float Value : Context.HeightField) bUntouched &= Value == 0.f;
			TestTrue(TEXT("Disabled terrain pass did not run"), bUntouched);
		});

		It("runs nothing when a pass is not set up", [this]()
		{
			AddExpectedError(TEXT("cannot run"), EAutomationExpectedErrorFlags::Contains, 1);

			UTerrainNoisePass* Terrain = NewObject<UTerrainNoisePass>();
			UBiomeAssignmentPass* Biomes = NewObject<UBiomeAssignmentPass>();
			const UTileGenerationPass* Passes[] = { Terrain, Biomes };

			FTileGrid Grid(MakeLayout(8, 8));
			FTileGenerationContext Context(Grid, 1); // no biome set

			TestFalse(TEXT("Reports failure"), TileGeneration::RunPasses(Passes, Context));

			bool bUntouched = true;
			for (const FTileData& Tile : Grid.GetTiles()) bUntouched &= Tile == FTileData();
			TestTrue(TEXT("The terrain pass before it did not run either"), bUntouched);
		});

		It("clears the current step when done", [this]()
		{
			const UTileGenerationPass* Passes[] = { NewObject<ULevelConstraintPass>() };
			FTileGrid Grid(MakeLayout(2, 2));
			FTileGenerationContext Context(Grid, 1);
			TileGeneration::RunPasses(Passes, Context);
			TestTrue(TEXT("Step reset"), Context.CurrentStep.IsNone());
		});
	});

	Describe("UTerrainNoisePass", [this]()
	{
		It("fills every noise field with values between 0 and 1", [this]()
		{
			FTileGrid Grid(MakeLayout(96, 96));
			FTileGenerationContext Context(Grid, 5);
			NewObject<UTerrainNoisePass>()->Execute(Context);

			bool bInRange = true;
			float MinHeight = 1.f, MaxHeight = 0.f;
			for (int32 Index = 0; Index < Grid.GetLayout().NumTiles(); ++Index)
			{
				for (const float Value : { Context.HeightField[Index], Context.MoistureField[Index], Context.DetailField[Index] })
				{
					bInRange &= Value >= 0.f && Value <= 1.f;
				}
				MinHeight = FMath::Min(MinHeight, Context.HeightField[Index]);
				MaxHeight = FMath::Max(MaxHeight, Context.HeightField[Index]);
			}
			TestTrue(TEXT("All values in 0..1"), bInRange);
			TestTrue(TEXT("Height actually varies"), MaxHeight - MinHeight > 0.1f);
		});

		It("makes water exactly where the height is at or below the water level", [this]()
		{
			UTerrainNoisePass* Terrain = NewObject<UTerrainNoisePass>();
			Terrain->WaterLevel = 0.5f;

			FTileGrid Grid(MakeLayout(96, 96));
			FTileGenerationContext Context(Grid, 8);
			Terrain->Execute(Context);

			int32 Mismatches = 0;
			int32 WaterTiles = 0;
			bool bWaterIsComplete = true;
			for (int32 Index = 0; Index < Grid.GetLayout().NumTiles(); ++Index)
			{
				const FTileData& Tile = Grid.GetTile(Grid.GetLayout().ToTile(Index));
				const bool bShouldBeWater = Context.HeightField[Index] <= Terrain->WaterLevel;
				if (Tile.IsWater() != bShouldBeWater) ++Mismatches;
				if (!Tile.IsWater()) continue;

				++WaterTiles;
				bWaterIsComplete &= Tile.Biome == EBiomeType::Water && Tile.Moisture == 100 && Tile.Height == 0;
			}
			TestEq(*this, TEXT("Tiles where water and height disagree"), Mismatches, 0);
			TestTrue(TEXT("There is water and land"), WaterTiles > 0 && WaterTiles < Grid.GetLayout().NumTiles());
			TestTrue(TEXT("Water tiles have water biome, full moisture, level 0"), bWaterIsComplete);
		});

		It("leaves elevation to the Elevation pass", [this]()
		{
			FTileGrid Grid(MakeLayout(32, 32));
			FTileGenerationContext Context(Grid, 9);
			UTerrainNoisePass* Terrain = NewObject<UTerrainNoisePass>();
			Terrain->Execute(Context);

			bool bFlat = true;
			for (const FTileData& Tile : Grid.GetTiles()) bFlat &= Tile.Height == 0;
			TestTrue(TEXT("All tiles on level 0"), bFlat);
			TestTrue(TEXT("Water level handed on"), FMath::IsNearlyEqual(Context.WaterLevel, Terrain->WaterLevel));
		});

		It("produces the same fields for the same seed and different ones for another", [this]()
		{
			const auto HeightsFor = [](int32 Seed)
			{
				FTileGrid Grid(MakeLayout(16, 16));
				FTileGenerationContext Context(Grid, Seed);
				NewObject<UTerrainNoisePass>()->Execute(Context);
				return Context.HeightField;
			};

			TestTrue(TEXT("Same seed"), HeightsFor(4) == HeightsFor(4));
			TestFalse(TEXT("Other seed"), HeightsFor(4) == HeightsFor(5));
		});

		It("derives every noise field from the seed", [this]()
		{
			// The detail noise used to ignore the seed, so biome borders looked the same in every world.
			FTileGrid GridA(MakeLayout(16, 16));
			FTileGrid GridB(MakeLayout(16, 16));
			FTileGenerationContext SeedA(GridA, 4);
			FTileGenerationContext SeedB(GridB, 5);
			NewObject<UTerrainNoisePass>()->Execute(SeedA);
			NewObject<UTerrainNoisePass>()->Execute(SeedB);

			TestFalse(TEXT("Height depends on the seed"), SeedA.HeightField == SeedB.HeightField);
			TestFalse(TEXT("Moisture depends on the seed"), SeedA.MoistureField == SeedB.MoistureField);
			TestFalse(TEXT("Detail depends on the seed"), SeedA.DetailField == SeedB.DetailField);
		});

		It("does not make water when the water level is below every height", [this]()
		{
			UTerrainNoisePass* Terrain = NewObject<UTerrainNoisePass>();
			Terrain->WaterLevel = -1.f;
			FTileGrid Grid(MakeLayout(16, 16));
			FTileGenerationContext Context(Grid, 2);
			Terrain->Execute(Context);

			bool bNoWater = true;
			for (const FTileData& Tile : Grid.GetTiles()) bNoWater &= !Tile.IsWater();
			TestTrue(TEXT("No water"), bNoWater);
		});
	});

	Describe("UElevationPass", [this]()
	{
		It("keeps the world flat when MaxLevel is 0", [this]()
		{
			FTileGrid Grid(MakeLayout(32, 32));
			FTileGenerationContext Context(Grid, 9);
			NewObject<UTerrainNoisePass>()->Execute(Context);
			UElevationPass* Elevation = NewObject<UElevationPass>();
			Elevation->MaxLevel = 0;
			Elevation->Execute(Context);

			bool bFlat = true;
			for (const FTileData& Tile : Grid.GetTiles()) bFlat &= Tile.Height == (Tile.IsWater() ? Elevation->BaseLevel - 1 : Elevation->BaseLevel);
			TestTrue(TEXT("Land on the base level, water one below"), bFlat);
		});

		It("assigns levels from height, water level and level step without biomes", [this]()
		{
			UTerrainNoisePass* Terrain = NewObject<UTerrainNoisePass>();
			UElevationPass* Elevation = NewObject<UElevationPass>();
			Elevation->MaxLevel = 3;
			Elevation->LevelStep = 0.05f;
			Elevation->BaseLevel = 0;

			FTileGrid Grid(MakeLayout(96, 96));
			FTileGenerationContext Context(Grid, 11);
			Terrain->Execute(Context);
			Elevation->Execute(Context);

			int32 Wrong = 0;
			uint8 HighestLevel = 0;
			for (int32 Index = 0; Index < Grid.GetLayout().NumTiles(); ++Index)
			{
				const FTileData& Tile = Grid.GetTile(Grid.GetLayout().ToTile(Index));
				if (Tile.IsWater()) continue;

				const int32 Expected = FMath::Clamp(FMath::FloorToInt32((Context.HeightField[Index] - Terrain->WaterLevel) / Elevation->LevelStep), 0, 3);
				if (Tile.Height != Expected) ++Wrong;
				HighestLevel = FMath::Max(HighestLevel, Tile.Height);
			}
			TestEq(*this, TEXT("Tiles on the wrong level"), Wrong, 0);
			TestTrue(TEXT("Several levels are used"), HighestLevel >= 2);
		});

		// Three land tiles whose noise is set by hand: plains (flat, capped at 1) and a steep forest.
		const auto RunOnBiomes = [](int32 InMaxLevel)
		{
			UBiomeTypeData* Plains = MakeBiome(EBiomeType::Plains, FVector2D(0.3, 1.0), FVector2D(0.0, 0.5));
			Plains->ElevationScale = 1.f;
			Plains->MaxLevel = 1;
			UBiomeTypeData* Forest = MakeBiome(EBiomeType::Forest, FVector2D(0.3, 1.0), FVector2D(0.5, 1.0));
			Forest->ElevationScale = 2.f;
			Forest->MaxLevel = 4;

			FTileGrid Grid = MakeGrid({ TEXT("GGG") });
			FTileGenerationContext Context(Grid, 1);
			Context.Biomes = MakeBiomeSet({ Plains, Forest });
			Context.WaterLevel = 0.3f;
			Context.HeightField = { 0.8f, 0.47f, 0.6f };
			Context.MoistureField = { 0.1f, 0.95f, 0.95f };

			UElevationPass* Elevation = NewObject<UElevationPass>();
			Elevation->MaxLevel = InMaxLevel;
			Elevation->LevelStep = 0.1f;
			Elevation->BaseLevel = 0;
			Elevation->Execute(Context);
			return TArray<FTileData>(Grid.GetTiles());
		};

		It("scales and caps elevation per biome", [this, RunOnBiomes]()
		{
			const TArray<FTileData> Tiles = RunOnBiomes(4);
			// Plains: 0.5 above water would be level 5, but the biome stops at 1.
			TestEq(*this, TEXT("Plains capped"), Tiles[0].Height, static_cast<uint8>(1));
			// Forest: 0.17 above water, twice as steep: 3.4 steps.
			TestEq(*this, TEXT("Forest scaled"), Tiles[1].Height, static_cast<uint8>(3));
			// Forest: 6 steps, capped at the biome's 4.
			TestEq(*this, TEXT("Forest capped"), Tiles[2].Height, static_cast<uint8>(4));
		});

		It("caps every biome with the pass's MaxLevel", [this, RunOnBiomes]()
		{
			const TArray<FTileData> Tiles = RunOnBiomes(2);
			TestEq(*this, TEXT("Plains"), Tiles[0].Height, static_cast<uint8>(1));
			TestEq(*this, TEXT("Forest"), Tiles[1].Height, static_cast<uint8>(2));
			TestEq(*this, TEXT("Forest peak"), Tiles[2].Height, static_cast<uint8>(2));
		});

		It("keeps a biome with no elevation flat", [this]()
		{
			const FTestWorldData Data = MakeTestWorldData();
			for (UBiomeTypeData* Biome : Data.Biomes->BiomeData) Biome->ElevationScale = 0.f;

			FTileGrid Grid(MakeLayout(48, 48));
			FTileGenerationContext Context(Grid, 3);
			Context.Biomes = Data.Biomes;
			NewObject<UTerrainNoisePass>()->Execute(Context);
			UElevationPass* Elevation = NewObject<UElevationPass>();
			Elevation->BaseLevel = 0;
			Elevation->Execute(Context);

			bool bFlat = true;
			for (const FTileData& Tile : Grid.GetTiles()) bFlat &= Tile.Height == 0;
			TestTrue(TEXT("All tiles on level 0"), bFlat);
		});

		It("puts land on the base level and water one level below it", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GW") });
			FTileGenerationContext Context(Grid, 1);
			Context.WaterLevel = 0.3f;
			Context.HeightField = { 0.35f, 0.1f };
			Context.MoistureField = { 0.5f, 0.5f };

			UElevationPass* Elevation = NewObject<UElevationPass>();
			Elevation->BaseLevel = 2;
			Elevation->Execute(Context);

			TestEq(*this, TEXT("Land"), Grid.GetTile(FIntPoint(0, 0)).Height, static_cast<uint8>(2));
			TestEq(*this, TEXT("Water"), Grid.GetTile(FIntPoint(1, 0)).Height, static_cast<uint8>(1));
		});


	});

	Describe("UBiomeAssignmentPass", [this]()
	{
		It("cannot run without biomes", [this]()
		{
			FTileGrid Grid(MakeLayout(2, 2));
			FTileGenerationContext Context(Grid, 1);
			const UBiomeAssignmentPass* Pass = NewObject<UBiomeAssignmentPass>();

			TestFalse(TEXT("No set"), Pass->GetSetupError(Context).IsEmpty());

			Context.Biomes = MakeBiomeSet({});
			TestFalse(TEXT("Empty set"), Pass->GetSetupError(Context).IsEmpty());

			Context.Biomes = MakeBiomeSet({ MakeBiome(EBiomeType::Plains, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0)) });
			TestTrue(TEXT("With a biome"), Pass->GetSetupError(Context).IsEmpty());
		});

		It("gives land the biome whose ranges contain it and leaves water alone", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("...W") });
			FTileGenerationContext Context(Grid, 1);
			Context.Biomes = MakeBiomeSet({
				MakeBiome(EBiomeType::Desert, FVector2D(0.0, 1.0), FVector2D(0.0, 0.3), nullptr, 20),
				MakeBiome(EBiomeType::Forest, FVector2D(0.0, 1.0), FVector2D(0.3, 1.0), nullptr, 200),
			});
			FillField(Context.HeightField, { 0.5f, 0.5f, 0.5f, 0.5f });
			FillField(Context.MoistureField, { 0.1f, 0.8f, 0.456f, 0.8f });
			FillField(Context.DetailField, { 0.f, 0.f, 0.f, 0.f });

			NewObject<UBiomeAssignmentPass>()->Execute(Context);

			TestEq(*this, TEXT("Dry tile"), Grid.GetTile(FIntPoint(0, 0)).Biome, EBiomeType::Desert);
			TestEq(*this, TEXT("Wet tile"), Grid.GetTile(FIntPoint(1, 0)).Biome, EBiomeType::Forest);
			// Fertility depends on the final tile type, so it is left for the Fertility pass.
			TestEq(*this, TEXT("Fertility not set yet"), Grid.GetTile(FIntPoint(0, 0)).Fertility, static_cast<uint8>(0));
			TestEq(*this, TEXT("Moisture from the noise, in percent"), Grid.GetTile(FIntPoint(2, 0)).Moisture, static_cast<uint8>(46));
			TestEq(*this, TEXT("Water keeps its biome"), Grid.GetTile(FIntPoint(3, 0)).Biome, EBiomeType::Water);
			TestEq(*this, TEXT("Water keeps its moisture"), Grid.GetTile(FIntPoint(3, 0)).Moisture, static_cast<uint8>(0));
		});

		It("lets the detail noise decide between two biomes", [this]()
		{
			FTileGrid Grid(MakeLayout(2, 1));
			FTileGenerationContext Context(Grid, 1);
			Context.Biomes = MakeBiomeSet({
				MakeBiome(EBiomeType::Plains, FVector2D(0.0, 0.2), FVector2D(0.0, 1.0)),
				MakeBiome(EBiomeType::Mountains, FVector2D(0.8, 1.0), FVector2D(0.0, 1.0)),
			});
			// Both tiles sit halfway between the biomes, so the chance for the second one is 0.5.
			FillField(Context.HeightField, { 0.5f, 0.5f });
			FillField(Context.MoistureField, { 0.5f, 0.5f });
			FillField(Context.DetailField, { 0.4f, 0.6f });

			NewObject<UBiomeAssignmentPass>()->Execute(Context);

			TestEq(*this, TEXT("Detail below the chance -> second biome"), Grid.GetTile(FIntPoint(0, 0)).Biome, EBiomeType::Mountains);
			TestEq(*this, TEXT("Detail above the chance -> first biome"), Grid.GetTile(FIntPoint(1, 0)).Biome, EBiomeType::Plains);
		});
	});

	Describe("ULakesAndRiversPass", [this]()
	{
		// A 9 x 9 map on level 3 whose height noise has a basin in the middle 5 x 5.
		const auto MakeBasin = [](float InInnerHeight, int32 InInnerSize)
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGGGGGGGG"), TEXT("GGGGGGGGG"), TEXT("GGGGGGGGG"), TEXT("GGGGGGGGG"), TEXT("GGGGGGGGG"), TEXT("GGGGGGGGG"), TEXT("GGGGGGGGG"), TEXT("GGGGGGGGG"), TEXT("GGGGGGGGG") });
			SetHeights(Grid, { TEXT("333333333"), TEXT("333333333"), TEXT("333333333"), TEXT("333333333"), TEXT("333333333"), TEXT("333333333"), TEXT("333333333"), TEXT("333333333"), TEXT("333333333") });
			TArray<float> Heights;
			Heights.Init(0.6f, 81);
			const int32 First = (9 - InInnerSize) / 2;
			for (int32 Y = First; Y < First + InInnerSize; ++Y)
			{
				for (int32 X = First; X < First + InInnerSize; ++X) Heights[X + Y * 9] = InInnerHeight;
			}
			return TPair<FTileGrid, TArray<float>>(MoveTemp(Grid), MoveTemp(Heights));
		};
		const auto RunLakes = [](FTileGrid& Grid, const TArray<float>& Heights, int32 InMinLakeSize)
		{
			FTileGenerationContext Context(Grid, 1);
			Context.HeightField = Heights;
			ULakesAndRiversPass* Pass = NewObject<ULakesAndRiversPass>();
			Pass->RiverCount = 0;
			Pass->MinLakeSize = InMinLakeSize;
			Pass->Execute(Context);
		};

		It("fills a basin with a lake one level below its banks", [this, MakeBasin, RunLakes]()
		{
			auto [Grid, Heights] = MakeBasin(0.4f, 5);
			RunLakes(Grid, Heights, 4);

			const FTileData& Centre = Grid.GetTile(FIntPoint(4, 4));
			TestTrue(TEXT("Centre is water"), Centre.IsWater());
			TestEq(*this, TEXT("Lake level"), Centre.Height, static_cast<uint8>(2));
			TestEq(*this, TEXT("Lake biome"), Centre.Biome, EBiomeType::Water);
			TestTrue(TEXT("Corner of the basin is water"), Grid.GetTile(FIntPoint(2, 2)).IsWater());
			TestFalse(TEXT("The rim stays land"), Grid.GetTile(FIntPoint(1, 4)).IsWater());
		});

		It("keeps basins smaller than MinLakeSize dry", [this, MakeBasin, RunLakes]()
		{
			auto [Grid, Heights] = MakeBasin(0.4f, 1);
			RunLakes(Grid, Heights, 4);
			for (const FTileData& Tile : Grid.GetTiles()) TestFalse(TEXT("No lake"), Tile.IsWater());
		});

		It("keeps a dip that drains to the map edge dry", [this, MakeBasin, RunLakes]()
		{
			auto [Grid, Heights] = MakeBasin(0.4f, 5);
			// Open the basin to the west edge: a channel as low as the basin.
			for (int32 X = 0; X < 2; ++X) Heights[X + 4 * 9] = 0.4f;
			RunLakes(Grid, Heights, 4);
			for (const FTileData& Tile : Grid.GetTiles()) TestFalse(TEXT("No lake"), Tile.IsWater());
		});

		// A long slope falling to the east, a valley along the middle row; land levels fall too.
		const auto RunRiver = [](int32 InWidenEvery)
		{
			constexpr int32 Width = 40;
			constexpr int32 Height = 7;
			TArray<FString> Rows;
			TArray<FString> Levels;
			for (int32 Y = 0; Y < Height; ++Y)
			{
				Rows.Add(FString::ChrN(Width, TEXT('G')));
				FString Row;
				for (int32 X = 0; X < Width; ++X) Row.AppendChar(TEXT('0') + 5 - X / 10);
				Levels.Add(Row);
			}
			FTileGrid Grid(MakeLayout(Width, Height));
			for (int32 Y = 0; Y < Height; ++Y)
			{
				for (int32 X = 0; X < Width; ++X)
				{
					Grid.ApplyMutation(FIntPoint(X, Y), FTileMutation().SetType(ETileType::Grass).SetBiome(EBiomeType::Plains).SetHeight(static_cast<uint8>(Levels[Y][X] - TEXT('0'))));
				}
			}

			FTileGenerationContext Context(Grid, 3);
			Context.HeightField.SetNum(Width * Height);
			for (int32 Y = 0; Y < Height; ++Y)
			{
				for (int32 X = 0; X < Width; ++X)
				{
					Context.HeightField[X + Y * Width] = 0.9f - X * 0.015f + FMath::Abs(Y - Height / 2) * 0.01f;
				}
			}

			ULakesAndRiversPass* Pass = NewObject<ULakesAndRiversPass>();
			Pass->RiverCount = 1;
			Pass->SourceMinHeight = 0.87f;
			Pass->MinRiverLength = 5;
			Pass->WidenEvery = InWidenEvery;
			Pass->Execute(Context);
			return Grid;
		};

		It("runs a river downhill to the map edge, never rising and never above its banks", [this, RunRiver]()
		{
			const FTileGrid Grid = RunRiver(1000);
			const FTileGridLayout& Layout = Grid.GetLayout();

			int32 WaterTiles = 0;
			int32 AboveBank = 0;
			bool bReachesEdge = false;
			int32 Previous = MAX_int32;
			bool bNeverRises = true;
			for (int32 X = 0; X < Layout.Size.X; ++X)
			{
				int32 ColumnLevel = MAX_int32;
				for (int32 Y = 0; Y < Layout.Size.Y; ++Y)
				{
					const FIntPoint Tile(X, Y);
					const FTileData& Data = Grid.GetTile(Tile);
					if (!Data.IsWater()) continue;
					++WaterTiles;
					ColumnLevel = FMath::Min<int32>(ColumnLevel, Data.Height);
					if (X == Layout.Size.X - 1) bReachesEdge = true;
					for (const FIntPoint& Offset : FTileGrid::GetCardinalOffsets())
					{
						const FTileData* Neighbor = Grid.FindTile(Tile + Offset);
						if (Neighbor && !Neighbor->IsWater() && Neighbor->Height < Data.Height) ++AboveBank;
					}
				}
				if (ColumnLevel == MAX_int32) continue;
				bNeverRises &= ColumnLevel <= Previous;
				Previous = ColumnLevel;
			}

			TestTrue(TEXT("A river"), WaterTiles >= 30);
			TestTrue(TEXT("Reaches the east edge"), bReachesEdge);
			TestTrue(TEXT("Never rises downstream"), bNeverRises);
			TestEq(*this, TEXT("Water above a bank"), AboveBank, 0);
		});

		It("widens a river downstream", [this, RunRiver]()
		{
			const FTileGrid Narrow = RunRiver(1000);
			const FTileGrid Wide = RunRiver(8);

			const auto CountWater = [](const FTileGrid& Grid)
			{
				int32 Count = 0;
				for (const FTileData& Tile : Grid.GetTiles()) Count += Tile.IsWater() ? 1 : 0;
				return Count;
			};
			TestTrue(TEXT("More water when it widens"), CountWater(Wide) > CountWater(Narrow) + 10);

			// The last column holds the widest part.
			int32 LastColumn = 0;
			for (int32 Y = 0; Y < Wide.GetSize().Y; ++Y) LastColumn += Wide.GetTile(FIntPoint(Wide.GetSize().X - 1, Y)).IsWater() ? 1 : 0;
			TestTrue(TEXT("Three wide at the mouth"), LastColumn >= 3);
		});
	});

	Describe("UTileTypePass", [this]()
	{
		It("uses the biome's rules, then the default rules, then grass", [this]()
		{
			FTileGrid Grid(MakeLayout(4, 1));
			Grid.ApplyMutation(FIntPoint(0, 0), FTileMutation().SetBiome(EBiomeType::Desert));
			Grid.ApplyMutation(FIntPoint(1, 0), FTileMutation().SetBiome(EBiomeType::Plains));
			Grid.ApplyMutation(FIntPoint(2, 0), FTileMutation().SetBiome(EBiomeType::Forest));
			Grid.ApplyMutation(FIntPoint(3, 0), FTileMutation().SetType(ETileType::Water).SetBiome(EBiomeType::Water));

			FTileGenerationContext Context(Grid, 1);
			Context.Biomes = MakeBiomeSet({
				MakeBiome(EBiomeType::Desert, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0), MakeRuleSet({ MakeRule(ETileType::Sand, ETileNoiseSource::Height, 0.f, 1.01f) })),
				MakeBiome(EBiomeType::Plains, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0)),
				MakeBiome(EBiomeType::Water, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0), MakeRuleSet({ MakeRule(ETileType::Rock, ETileNoiseSource::Height, 0.f, 1.01f) })),
			});
			Context.DefaultRuleSet = MakeRuleSet({ MakeRule(ETileType::Dirt, ETileNoiseSource::Height, 0.f, 1.01f) });

			NewObject<UTileTypePass>()->Execute(Context);
			TestEq(*this, TEXT("Biome rules / default rules / unknown biome uses default / water untouched"),
				DescribeTypes(Grid), FString(TEXT("SDDW")));

			Context.DefaultRuleSet = nullptr;
			Grid.ApplyMutation(FIntPoint(1, 0), FTileMutation().SetType(ETileType::Rock));
			NewObject<UTileTypePass>()->Execute(Context);
			TestEq(*this, TEXT("Without any rules the tile becomes grass"), Grid.GetTile(FIntPoint(1, 0)).Type, ETileType::Grass);
		});
	});

	Describe("ULevelConstraintPass", [this]()
	{
		It("cuts steep slopes and explains the change", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGG"), TEXT("GGG"), TEXT("GGG") });
			SetHeights(Grid, { TEXT("000"), TEXT("050"), TEXT("000") });

			FTileGenerationContext Context(Grid, 1);
			Context.CurrentStep = TEXT("Terraces");
			TArray<FTileChange> Changes;
			Context.OnChange = [&Changes](const FTileChange& Change) { Changes.Add(Change); };

			// Only the step limit here: the merge would also flatten the one-tile peak.
			ULevelConstraintPass* Pass = NewObject<ULevelConstraintPass>();
			Pass->MinTerraceSize = 1;
			Pass->Execute(Context);

			TestEq(*this, TEXT("Peak"), Grid.GetTile(FIntPoint(1, 1)).Height, static_cast<uint8>(1));
			if (!TestEq(*this, TEXT("One change"), Changes.Num(), 1)) return;
			TestEq(*this, TEXT("Changed field"), Changes[0].ChangedFields, ETileField::Height);
			TestEq(*this, TEXT("From level"), Changes[0].Before.Height, static_cast<uint8>(5));
		});

		It("respects a larger MaxStep and leaves valid terrain alone", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGGG") });
			SetHeights(Grid, { TEXT("0246") });

			FTileGenerationContext Context(Grid, 1);
			ULevelConstraintPass* Pass = NewObject<ULevelConstraintPass>();
			Pass->MaxStep = 2;
			Pass->MinTerraceSize = 1;
			Pass->Execute(Context);

			TestEq(*this, TEXT("Last tile kept"), Grid.GetTile(FIntPoint(3, 0)).Height, static_cast<uint8>(6));

			Pass->MaxStep = 1;
			Pass->Execute(Context);
			TestEq(*this, TEXT("Now cut to steps of one"), Grid.GetTile(FIntPoint(3, 0)).Height, static_cast<uint8>(3));
		});

		It("merges terraces smaller than MinTerraceSize into the land around them", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGGGGG"), TEXT("GGGGGG"), TEXT("GGGGGG"), TEXT("GGGGGG") });
			SetHeights(Grid, { TEXT("000000"), TEXT("011000"), TEXT("000000"), TEXT("000000") });

			FTileGenerationContext Context(Grid, 1);
			TArray<FTileChange> Changes;
			Context.OnChange = [&Changes](const FTileChange& Change) { Changes.Add(Change); };
			ULevelConstraintPass* Pass = NewObject<ULevelConstraintPass>();
			Pass->MinTerraceSize = 4;
			Pass->Execute(Context);

			for (const FTileData& Tile : Grid.GetTiles()) TestEq(*this, TEXT("Flat"), Tile.Height, static_cast<uint8>(0));
			if (!TestEq(*this, TEXT("Two changes"), Changes.Num(), 2)) return;
			TestEq(*this, TEXT("Reason"), Changes[0].Context.Reason, FName(TEXT("Too small for a terrace")));
		});

		It("keeps terraces of MinTerraceSize and more", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGGGGG"), TEXT("GGGGGG"), TEXT("GGGGGG"), TEXT("GGGGGG") });
			SetHeights(Grid, { TEXT("000000"), TEXT("011000"), TEXT("011000"), TEXT("000000") });

			FTileGenerationContext Context(Grid, 1);
			ULevelConstraintPass* Pass = NewObject<ULevelConstraintPass>();
			Pass->MinTerraceSize = 4;
			Pass->Execute(Context);

			TestEq(*this, TEXT("Terrace kept"), Grid.GetTile(FIntPoint(1, 1)).Height, static_cast<uint8>(1));
		});
	});

	Describe("UShorePass", [this]()
	{
		It("turns land next to water into the biome's shore tile", [this]()
		{
			FTileGrid Grid = MakeGrid({
				TEXT("GGGGG"),
				TEXT("GGWGG"),
				TEXT("GGGGG"),
				TEXT("RGGGG"),
			});
			FTileGenerationContext Context(Grid, 1);
			Context.Biomes = MakeBiomeSet({ MakeBiome(EBiomeType::Plains, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0)) });

			NewObject<UShorePass>()->Execute(Context);

			// Diagonal neighbours of the water stay grass.
			TestEq(*this, TEXT("Map"), DescribeTypes(Grid), FString(TEXT("GGSGG/GSWSG/GGSGG/RGGGG")));
		});

		It("leaves tiles alone when their biome has no asset or a water shore", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GWG") });
			Grid.ApplyMutation(FIntPoint(2, 0), FTileMutation().SetBiome(EBiomeType::Desert));

			FTileGenerationContext Context(Grid, 1);
			Context.Biomes = MakeBiomeSet({ MakeBiome(EBiomeType::Desert, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0), nullptr, 50, ETileType::Water) });

			NewObject<UShorePass>()->Execute(Context);
			TestEq(*this, TEXT("Map"), DescribeTypes(Grid), FString(TEXT("GWG")));
		});
	});

	Describe("UTileSmoothingPass", [this]()
	{
		It("replaces an isolated tile with the type around it", [this]()
		{
			FTileGrid Grid = MakeGrid({
				TEXT("GGGGG"),
				TEXT("GGGGG"),
				TEXT("GGRGG"),
				TEXT("GGGGG"),
			});
			FTileGenerationContext Context(Grid, 1);
			Context.Biomes = MakeBiomeSet({ MakeBiome(EBiomeType::Plains, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0)) });

			NewObject<UTileSmoothingPass>()->Execute(Context);
			TestEq(*this, TEXT("Rock speck removed"), Grid.GetTile(FIntPoint(2, 2)).Type, ETileType::Grass);
		});

		It("uses the biome's override when a neighbour has that type", [this]()
		{
			// Grass is the most common neighbour, but the override picks the dirt next to the rock.
			FTileGrid Grid = MakeGrid({ TEXT("GGG"), TEXT("GRD"), TEXT("GGG") });
			UBiomeTypeData* Plains = MakeBiome(EBiomeType::Plains, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0));
			Plains->SmoothingOverrides.Add(ETileType::Rock, ETileType::Dirt);

			FTileGenerationContext Context(Grid, 1);
			Context.Biomes = MakeBiomeSet({ Plains });

			NewObject<UTileSmoothingPass>()->Execute(Context);
			TestEq(*this, TEXT("Override"), Grid.GetTile(FIntPoint(1, 1)).Type, ETileType::Dirt);
		});

		It("ignores the override when no neighbour has that type", [this]()
		{
			// The seed 5 case: lone grass in a sand field with a Grass -> Dirt override used to become lone dirt.
			FTileGrid Grid = MakeGrid({ TEXT("SSS"), TEXT("SGS"), TEXT("SSS") });
			UBiomeTypeData* Plains = MakeBiome(EBiomeType::Plains, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0));
			Plains->SmoothingOverrides.Add(ETileType::Grass, ETileType::Dirt);

			FTileGenerationContext Context(Grid, 1);
			Context.Biomes = MakeBiomeSet({ Plains });

			NewObject<UTileSmoothingPass>()->Execute(Context);
			TestEq(*this, TEXT("Most common neighbour"), Grid.GetTile(FIntPoint(1, 1)).Type, ETileType::Sand);
		});

		It("keeps strips, shores and water", [this]()
		{
			FTileGrid Grid = MakeGrid({
				TEXT("GGGGGG"),
				TEXT("GRRRGG"),
				TEXT("GGGGGG"),
				TEXT("GGGSWW"),
			});
			FTileGenerationContext Context(Grid, 1);
			Context.Biomes = MakeBiomeSet({ MakeBiome(EBiomeType::Plains, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0)) });

			NewObject<UTileSmoothingPass>()->Execute(Context);

			// The middle of the rock strip has two rock neighbours and stays. The strip ends have one
			// and are smoothed. The sand next to water is a shore and stays.
			TestEq(*this, TEXT("Map"), DescribeTypes(Grid), FString(TEXT("GGGGGG/GGRGGG/GGGGGG/GGGSWW")));
		});

		It("decides every tile on the map as it was before the pass", [this]()
		{
			// A checkerboard: every tile is isolated. Deciding in place would let early changes
			// affect later decisions; deciding first swaps every tile.
			FTileGrid Grid = MakeGrid({ TEXT("GRG"), TEXT("RGR"), TEXT("GRG") });
			FTileGenerationContext Context(Grid, 1);
			Context.Biomes = MakeBiomeSet({ MakeBiome(EBiomeType::Plains, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0)) });

			NewObject<UTileSmoothingPass>()->Execute(Context);
			TestEq(*this, TEXT("Map"), DescribeTypes(Grid), FString(TEXT("RGR/GRG/RGR")));
		});
	});

	Describe("USlopePass", [this]()
	{
		// A 3 x 3 terrace one level up in the middle of a 7 x 7 map.
		const auto MakeTerrace = []()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG") });
			SetHeights(Grid, { TEXT("0000000"), TEXT("0000000"), TEXT("0011100"), TEXT("0011100"), TEXT("0011100"), TEXT("0000000"), TEXT("0000000") });
			return Grid;
		};
		const auto RunSlopes = [](FTileGrid& Grid, float InAmount, int32 InSeed = 1)
		{
			FTileGenerationContext Context(Grid, InSeed);
			USlopePass* Pass = NewObject<USlopePass>();
			Pass->SlopeAmount = InAmount;
			Pass->Execute(Context);
		};

		It("rings a terrace with edges and outer corners where every corner is soft", [this, MakeTerrace, RunSlopes]()
		{
			FTileGrid Grid = MakeTerrace();
			RunSlopes(Grid, 1.f);

			int32 Edges = 0;
			int32 OuterCorners = 0;
			for (const FTileData& Tile : Grid.GetTiles())
			{
				const ETileSlopeShape Shape = TileData::GetSlopeShape(Tile.Slope);
				Edges += Shape == ETileSlopeShape::Edge ? 1 : 0;
				OuterCorners += Shape == ETileSlopeShape::OuterCorner ? 1 : 0;
			}
			TestEq(*this, TEXT("Edges, three per side"), Edges, 12);
			TestEq(*this, TEXT("Outer corners"), OuterCorners, 4);

			TestEq(*this, TEXT("West side rises east"), TileData::GetSlopeFacing(Grid.GetTile(FIntPoint(1, 3)).Slope), FIntPoint(1, 0));
			TestEq(*this, TEXT("North-west corner raised south-east"), TileData::GetSlopeFacing(Grid.GetTile(FIntPoint(1, 1)).Slope), FIntPoint(1, 1));
			TestFalse(TEXT("The terrace stays flat"), Grid.GetTile(FIntPoint(3, 3)).IsSlope());
			TestFalse(TEXT("Two tiles away stays flat"), Grid.GetTile(FIntPoint(0, 3)).IsSlope());

			TArray<int32> Regions;
			TestEq(*this, TEXT("Walkable areas"), TileReachability::ComputeWalkRegions(Grid, Regions), 1);

			TArray<FTileCliffFace> Faces;
			TileCliffs::CollectFaces(Grid, FIntPoint::ZeroValue, Grid.GetSize(), Faces);
			TestEq(*this, TEXT("No cliffs left"), Faces.Num(), 0);
		});

		It("fills the inside of a corner with an inner corner", [this, RunSlopes]()
		{
			// An L-shaped terrace: the low tile (1, 1) has higher ground north, west and north-west.
			FTileGrid Grid = MakeGrid({ TEXT("GGGG"), TEXT("GGGG"), TEXT("GGGG"), TEXT("GGGG") });
			SetHeights(Grid, { TEXT("1111"), TEXT("1000"), TEXT("1000"), TEXT("1000") });
			RunSlopes(Grid, 1.f);

			const uint8 Slope = Grid.GetTile(FIntPoint(1, 1)).Slope;
			TestEq(*this, TEXT("Shape"), TileData::GetSlopeShape(Slope), ETileSlopeShape::InnerCorner);
			TestEq(*this, TEXT("Low corner"), TileData::GetSlopeFacing(Slope), FIntPoint(1, 1));
		});

		It("keeps every cliff where no corner is soft", [this, MakeTerrace, RunSlopes]()
		{
			FTileGrid Grid = MakeTerrace();
			RunSlopes(Grid, 0.f);
			for (const FTileData& Tile : Grid.GetTiles()) TestFalse(TEXT("No slope"), Tile.IsSlope());
		});

		It("leaves a one-tile valley flat", [this, RunSlopes]()
		{
			// The middle column has higher ground on both sides: it would need opposite edges raised.
			FTileGrid Grid = MakeGrid({ TEXT("GGG"), TEXT("GGG"), TEXT("GGG") });
			SetHeights(Grid, { TEXT("101"), TEXT("101"), TEXT("101") });
			RunSlopes(Grid, 1.f);

			for (int32 Y = 0; Y < 3; ++Y) TestFalse(TEXT("Valley"), Grid.GetTile(FIntPoint(1, Y)).IsSlope());
		});

		It("never slopes water or a tile something stands on", [this, RunSlopes]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGG"), TEXT("WGG") });
			SetHeights(Grid, { TEXT("111"), TEXT("000") });
			Grid.ApplyMutation(FIntPoint(2, 1), FTileMutation().SetFlag(ETileFlags::Occupied, true));
			RunSlopes(Grid, 1.f);

			TestFalse(TEXT("Water"), Grid.GetTile(FIntPoint(0, 1)).IsSlope());
			TestTrue(TEXT("Land"), Grid.GetTile(FIntPoint(1, 1)).IsSlope());
			TestFalse(TEXT("Occupied"), Grid.GetTile(FIntPoint(2, 1)).IsSlope());
		});

		It("mixes slopes and cliffs from the seed", [this, RunSlopes]()
		{
			const auto SlopesFor = [RunSlopes](int32 InSeed)
			{
				// Two long terraces, so the noise has room to change along their edges.
				FTileGrid Grid = MakeGrid({ TEXT("GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG"), TEXT("GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG"), TEXT("GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG"), TEXT("GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG") });
				SetHeights(Grid, { TEXT("1111111111111111111111111111111111111111"), TEXT("0000000000000000000000000000000000000000"), TEXT("0000000000000000000000000000000000000000"), TEXT("1111111111111111111111111111111111111111") });
				RunSlopes(Grid, 0.5f, InSeed);
				return TArray<FTileData>(Grid.GetTiles());
			};

			const TArray<FTileData> Tiles = SlopesFor(4);
			int32 Slopes = 0;
			for (const FTileData& Tile : Tiles) Slopes += Tile.IsSlope() ? 1 : 0;
			TestTrue(TEXT("Some slopes"), Slopes > 0);
			TestTrue(TEXT("Some cliffs"), Slopes < 80);
			TestTrue(TEXT("Same seed"), Tiles == SlopesFor(4));
			TestFalse(TEXT("Another seed"), Tiles == SlopesFor(5));
		});
	});

	Describe("URampPass", [this]()
	{
		It("joins a raised terrace to the land below with one ramp", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG") });
			SetHeights(Grid, { TEXT("0000000"), TEXT("0000000"), TEXT("0011100"), TEXT("0011100"), TEXT("0011100"), TEXT("0000000"), TEXT("0000000") });
			FTileGenerationContext Context(Grid, 1);

			NewObject<URampPass>()->Execute(Context);

			int32 Ramps = 0;
			for (int32 Index = 0; Index < Grid.GetLayout().NumTiles(); ++Index)
			{
				const FIntPoint Tile = Grid.GetLayout().ToTile(Index);
				const FTileData& Data = Grid.GetTile(Tile);
				if (!Data.IsSlope()) continue;
				++Ramps;

				// On the low ring, rising into the terrace, with flat ground behind it.
				TestEq(*this, TEXT("Shape"), TileData::GetSlopeShape(Data.Slope), ETileSlopeShape::Edge);
				const FIntPoint Offset = TileData::GetSlopeFacing(Data.Slope);
				TestEq(*this, TEXT("Ramp level"), Data.Height, static_cast<uint8>(0));
				TestEq(*this, TEXT("Top level"), Grid.GetTile(Tile + Offset).Height, static_cast<uint8>(1));
				const FTileData* Back = Grid.FindTile(Tile - Offset);
				TestTrue(TEXT("Flat ground behind"), Back && Back->Height == 0);
			}
			TestEq(*this, TEXT("Ramps"), Ramps, 1);

			TArray<int32> Regions;
			TestEq(*this, TEXT("Walkable areas"), TileReachability::ComputeWalkRegions(Grid, Regions), 1);
		});

		It("joins every terrace, including one on top of another", [this]()
		{
			// Rings two tiles wide, so a ramp onto each level has flat ground behind it.
			FTileGrid Grid = MakeGrid({
				TEXT("GGGGGGGGGGG"), TEXT("GGGGGGGGGGG"), TEXT("GGGGGGGGGGG"), TEXT("GGGGGGGGGGG"), TEXT("GGGGGGGGGGG"), TEXT("GGGGGGGGGGG"),
				TEXT("GGGGGGGGGGG"), TEXT("GGGGGGGGGGG"), TEXT("GGGGGGGGGGG"), TEXT("GGGGGGGGGGG"), TEXT("GGGGGGGGGGG"),
			});
			SetHeights(Grid, {
				TEXT("00000000000"),
				TEXT("00000000000"),
				TEXT("00111111100"),
				TEXT("00111111100"),
				TEXT("00112221100"),
				TEXT("00112221100"),
				TEXT("00112221100"),
				TEXT("00111111100"),
				TEXT("00111111100"),
				TEXT("00000000000"),
				TEXT("00000000000"),
			});
			FTileGenerationContext Context(Grid, 5);
			NewObject<URampPass>()->Execute(Context);

			TArray<int32> Regions;
			TestEq(*this, TEXT("Walkable areas"), TileReachability::ComputeWalkRegions(Grid, Regions), 1);
		});

		It("joins the terraces of every island, not only the largest one", [this]()
		{
			// Left: a big flat island. Right, past the water: a small island with a terrace on it.
			FTileGrid Grid = MakeGrid({
				TEXT("GGGGGGGGWGGGGGG"),
				TEXT("GGGGGGGGWGGGGGG"),
				TEXT("GGGGGGGGWGGGGGG"),
				TEXT("GGGGGGGGWGGGGGG"),
				TEXT("GGGGGGGGWGGGGGG"),
			});
			SetHeights(Grid, {
				TEXT("000000000000000"),
				TEXT("000000000000000"),
				TEXT("000000000001100"),
				TEXT("000000000001100"),
				TEXT("000000000000000"),
			});
			FTileGenerationContext Context(Grid, 1);
			NewObject<URampPass>()->Execute(Context);

			TArray<int32> Regions;
			TestEq(*this, TEXT("Walkable areas: one per island"), TileReachability::ComputeWalkRegions(Grid, Regions), 2);
		});

		It("leaves land that only water separates apart", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGWGG") });
			FTileGenerationContext Context(Grid, 1);
			NewObject<URampPass>()->Execute(Context);

			TestEq(*this, TEXT("Map"), DescribeTypes(Grid), FString(TEXT("GGWGG")));
			for (const FTileData& Tile : Grid.GetTiles()) TestFalse(TEXT("No ramp"), Tile.IsSlope());
		});

		It("needs flat ground behind a ramp", [this]()
		{
			// The only low tile next to the terrace has the map edge behind it, so no ramp fits.
			FTileGrid Grid = MakeGrid({ TEXT("GG") });
			SetHeights(Grid, { TEXT("01") });
			FTileGenerationContext Context(Grid, 1);
			NewObject<URampPass>()->Execute(Context);

			TestFalse(TEXT("No ramp on (0, 0)"), Grid.GetTile(FIntPoint(0, 0)).IsSlope());
		});

		It("places the same ramps for the same seed", [this]()
		{
			const auto RampsFor = [](int32 InSeed)
			{
				FTileGrid Grid = MakeGrid({ TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG") });
				SetHeights(Grid, { TEXT("0000000"), TEXT("0000000"), TEXT("0011100"), TEXT("0011100"), TEXT("0011100"), TEXT("0000000"), TEXT("0000000") });
				FTileGenerationContext Context(Grid, InSeed);
				NewObject<URampPass>()->Execute(Context);
				return TArray<FTileData>(Grid.GetTiles());
			};
			TestTrue(TEXT("Same seed"), RampsFor(9) == RampsFor(9));
		});
	});

	Describe("UFertilityPass", [this]()
	{
		It("needs a biome set for the soil table", [this]()
		{
			FTileGrid Grid(MakeLayout(1, 1));
			FTileGenerationContext Context(Grid, 1);
			const UFertilityPass* Pass = NewObject<UFertilityPass>();

			TestFalse(TEXT("No set"), Pass->GetSetupError(Context).IsEmpty());

			Context.Biomes = MakeBiomeSet({});
			TestTrue(TEXT("With a set"), Pass->GetSetupError(Context).IsEmpty());
		});

		It("rates each tile by biome, soil and moisture", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GSRW") });
			Grid.ApplyMutation(FIntPoint(0, 0), FTileMutation().SetMoisture(50));
			Grid.ApplyMutation(FIntPoint(1, 0), FTileMutation().SetMoisture(100));
			Grid.ApplyMutation(FIntPoint(2, 0), FTileMutation().SetMoisture(50));
			Grid.ApplyMutation(FIntPoint(3, 0), FTileMutation().SetMoisture(100));

			FTileGenerationContext Context(Grid, 1);
			Context.Biomes = MakeBiomeSet({ MakeBiome(EBiomeType::Plains, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0), nullptr, 60) });

			NewObject<UFertilityPass>()->Execute(Context);

			// Default factors: grass 1.0, sand 0.3, rock and water 0; moisture factor 0.6 dry to 1.2 wet.
			TestEq(*this, TEXT("Grass, half wet: 60 x 1.0 x 0.9"), Grid.GetTile(FIntPoint(0, 0)).Fertility, static_cast<uint8>(54));
			TestEq(*this, TEXT("Sand, wet: 60 x 0.3 x 1.2"), Grid.GetTile(FIntPoint(1, 0)).Fertility, static_cast<uint8>(22));
			TestEq(*this, TEXT("Rock grows nothing"), Grid.GetTile(FIntPoint(2, 0)).Fertility, static_cast<uint8>(0));
			TestEq(*this, TEXT("Water grows nothing"), Grid.GetTile(FIntPoint(3, 0)).Fertility, static_cast<uint8>(0));
		});

		It("uses the soil table of the biome set and treats missing types as barren", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GD") });
			FTileGenerationContext Context(Grid, 1);
			UBiomeTypeDataSet* Set = MakeBiomeSet({ MakeBiome(EBiomeType::Plains, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0), nullptr, 60) });
			Set->SoilFertility = { { ETileType::Grass, 0.5f } };
			Context.Biomes = Set;

			NewObject<UFertilityPass>()->Execute(Context);

			TestEq(*this, TEXT("Grass, dry: 60 x 0.5 x 0.6"), Grid.GetTile(FIntPoint(0, 0)).Fertility, static_cast<uint8>(18));
			TestEq(*this, TEXT("Dirt is not in the table"), Grid.GetTile(FIntPoint(1, 0)).Fertility, static_cast<uint8>(0));
		});

		It("keeps fertility at 100 or below", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("G") });
			Grid.ApplyMutation(FIntPoint(0, 0), FTileMutation().SetMoisture(100));
			FTileGenerationContext Context(Grid, 1);
			Context.Biomes = MakeBiomeSet({ MakeBiome(EBiomeType::Plains, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0), nullptr, 200) });

			NewObject<UFertilityPass>()->Execute(Context);

			TestEq(*this, TEXT("200 x 1.0 x 1.2 is clamped"), Grid.GetTile(FIntPoint(0, 0)).Fertility, static_cast<uint8>(100));
		});
	});

	Describe("Full pipeline", [this]()
	{
		It("produces the same world for the same seed", [this]()
		{
			const FTileGrid First = GenerateWorld(42, FIntPoint(48, 48), 4);
			const FTileGrid Second = GenerateWorld(42, FIntPoint(48, 48), 4);
			const FTileGrid Other = GenerateWorld(43, FIntPoint(48, 48), 4);

			TestTrue(TEXT("Same seed"), TArray<FTileData>(First.GetTiles()) == TArray<FTileData>(Second.GetTiles()));
			TestFalse(TEXT("Other seed"), TArray<FTileData>(First.GetTiles()) == TArray<FTileData>(Other.GetTiles()));
		});

		It("leaves a complete, consistent world", [this]()
		{
			const FTileGrid Grid = GenerateWorld(7, FIntPoint(96, 96), 4);
			const FTileGridLayout& Layout = Grid.GetLayout();

			int32 NotGenerated = 0, LandWithoutBiome = 0, WaterAboveLand = 0, SteepSteps = 0, BareShores = 0, BadFertility = 0;
			int32 WaterTiles = 0, FertileBarrenTiles = 0;
			TSet<uint8> GrassFertility;
			for (int32 Index = 0; Index < Layout.NumTiles(); ++Index)
			{
				const FIntPoint Tile = Layout.ToTile(Index);
				const FTileData& Data = Grid.GetTile(Tile);

				if (!Data.IsGenerated()) ++NotGenerated;
				if (Data.Fertility > 100 || Data.Moisture > 100) ++BadFertility;
				if ((Data.Type == ETileType::Water || Data.Type == ETileType::Rock) && Data.Fertility > 0) ++FertileBarrenTiles;
				if (Data.Type == ETileType::Grass) GrassFertility.Add(Data.Fertility);

				if (Data.IsWater())
				{
					++WaterTiles;
					for (const FIntPoint& Offset : FTileGrid::GetCardinalOffsets())
					{
						const FTileData* Neighbor = Grid.FindTile(Tile + Offset);
						if (Neighbor && !Neighbor->IsWater() && Neighbor->Height < Data.Height) ++WaterAboveLand;
					}
				}
				else
				{
					if (Data.Biome == EBiomeType::EBiomeType_MAX || Data.Biome == EBiomeType::Water) ++LandWithoutBiome;
					if (Grid.HasNeighborOfType(Tile, ETileType::Water) && Data.Type != ETileType::Sand) ++BareShores;
				}

				for (const FIntPoint& Offset : FTileGrid::GetCardinalOffsets())
				{
					const FTileData* Neighbor = Grid.FindTile(Tile + Offset);
					if (Neighbor && FMath::Abs(Neighbor->Height - Data.Height) > 1) ++SteepSteps;
				}
			}

			TestEq(*this, TEXT("Tiles not generated"), NotGenerated, 0);
			TestEq(*this, TEXT("Land tiles without a land biome"), LandWithoutBiome, 0);
			TestEq(*this, TEXT("Water above the land next to it"), WaterAboveLand, 0);
			TestEq(*this, TEXT("Neighbours more than one level apart"), SteepSteps, 0);
			TestEq(*this, TEXT("Land next to water that is not sand"), BareShores, 0);
			TestEq(*this, TEXT("Fertility or moisture above 100"), BadFertility, 0);
			TestEq(*this, TEXT("Water or rock with fertility"), FertileBarrenTiles, 0);
			// Every tile used to get its biome's base value. Moisture must now make grass differ.
			TestTrue(TEXT("Grass fertility varies"), GrassFertility.Num() > 1);
			TestTrue(TEXT("Some water and some land"), WaterTiles > 0 && WaterTiles < Layout.NumTiles());
		});

		It("adds lakes and rivers on a full map", [this]()
		{
			for (const int32 Seed : { 3, 7, 11 })
			{
				TArray<FTileChange> Changes;
				GenerateWorld(Seed, FIntPoint(96, 96), 4, &Changes);

				int32 Lakes = 0, Rivers = 0;
				for (const FTileChange& Change : Changes)
				{
					if (Change.Context.Step != FName(TEXT("Water"))) continue;
					Lakes += Change.Context.Reason == FName(TEXT("Fills a basin")) ? 1 : 0;
					Rivers += Change.Context.Reason == FName(TEXT("River")) ? 1 : 0;
				}
				TestTrue(FString::Printf(TEXT("Seed %d: river tiles"), Seed), Rivers > 0);
				AddInfo(FString::Printf(TEXT("Seed %d: %d lake tiles, %d river tiles"), Seed, Lakes, Rivers));
			}
		});

		It("lets every piece of land be walked to unless water cuts it off", [this]()
		{
			for (const int32 Seed : { 3, 7, 11, 19, 42 })
			{
				const FTileGrid Grid = GenerateWorld(Seed, FIntPoint(96, 96), 4);
				const FTileGridLayout& Layout = Grid.GetLayout();

				TArray<int32> Regions;
				TileReachability::ComputeWalkRegions(Grid, Regions);

				// Land connected through land (any level) must be one walkable area. Islands are allowed.
				int32 CutOffSteps = 0;
				int32 Slopes = 0;
				int32 InvalidSlopes = 0;
				int32 Cliffs = 0;
				for (int32 Index = 0; Index < Layout.NumTiles(); ++Index)
				{
					const FIntPoint Tile = Layout.ToTile(Index);
					const FTileData& Data = Grid.GetTile(Tile);
					Slopes += Data.IsSlope() ? 1 : 0;
					InvalidSlopes += TileData::GetSlopeShape(Data.Slope) == ETileSlopeShape::Invalid ? 1 : 0;
					if (Regions[Index] == INDEX_NONE) continue;

					for (const FIntPoint& Offset : FTileGrid::GetCardinalOffsets())
					{
						const FIntPoint Next = Tile + Offset;
						if (!Layout.Contains(Next)) continue;
						const int32 NextRegion = Regions[Layout.ToIndex(Next)];
						if (NextRegion != INDEX_NONE && NextRegion != Regions[Index]) ++CutOffSteps;
						if (NextRegion != INDEX_NONE && !TileReachability::CanStep(Data, Grid.GetTile(Next), Offset)) ++Cliffs;
					}
				}
				TestEq(*this, FString::Printf(TEXT("Seed %d: land next to land it cannot be walked to"), Seed), CutOffSteps, 0);

				// No terrace smaller than the Terraces pass allows, unless only water surrounds it: a merge
				// on the final levels finds nothing to do.
				TArray<uint8> Levels;
				TArray<bool> IsLand;
				for (const FTileData& Tile : Grid.GetTiles())
				{
					Levels.Add(Tile.Height);
					IsLand.Add(TileReachability::IsWalkable(Tile));
				}
				const int32 MinSize = GetDefault<ULevelConstraintPass>()->MinTerraceSize;
				TestEq(*this, FString::Printf(TEXT("Seed %d: tiles in terraces that are too small"), Seed),
					TileAlgorithms::MergeSmallAreas(Levels, IsLand, Layout.Size, MinSize), 0);
				TestEq(*this, FString::Printf(TEXT("Seed %d: slopes with opposite or all corners raised"), Seed), InvalidSlopes, 0);
				TestTrue(FString::Printf(TEXT("Seed %d: has slopes"), Seed), Slopes > 0);
				TestTrue(FString::Printf(TEXT("Seed %d: some cliffs stay"), Seed), Cliffs > 0);
			}
		});

		It("records a history that rebuilds the final world exactly", [this]()
		{
			TArray<FTileChange> Changes;
			const FTileGrid Generated = GenerateWorld(11, FIntPoint(40, 40), 3, &Changes);

			// Replay every recorded change on an empty grid. If the history is complete and in order,
			// each change starts from the state the replay is in and the end result is the same world.
			TArray<FTileData> Replay;
			Replay.SetNum(Generated.GetLayout().NumTiles());

			int32 WrongBefore = 0;
			int32 EmptyChanges = 0;
			for (const FTileChange& Change : Changes)
			{
				FTileData& Tile = Replay[Generated.GetLayout().ToIndex(Change.Coordinates)];
				if (Tile != Change.Before) ++WrongBefore;
				if (Change.ChangedFields == ETileField::None || TileData::Diff(Change.Before, Change.After) != Change.ChangedFields) ++EmptyChanges;
				Tile = Change.After;
			}

			TestEq(*this, TEXT("Changes that did not start from the previous state"), WrongBefore, 0);
			TestEq(*this, TEXT("Changes whose field list is wrong"), EmptyChanges, 0);
			TestTrue(TEXT("Replay equals the generated world"), Replay == TArray<FTileData>(Generated.GetTiles()));
		});

		It("generates the same world whether or not history is recorded", [this]()
		{
			TArray<FTileChange> Changes;
			const FTileGrid WithHistory = GenerateWorld(21, FIntPoint(32, 32), 2, &Changes);
			const FTileGrid WithoutHistory = GenerateWorld(21, FIntPoint(32, 32), 2);
			TestTrue(TEXT("Same world"), TArray<FTileData>(WithHistory.GetTiles()) == TArray<FTileData>(WithoutHistory.GetTiles()));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
