// Specs for world generation: the context, the pipeline runner, every pass, and the full pipeline.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TileTestUtils.h"
#include "Generation/TileGenerationContext.h"
#include "Generation/TileGenerationPass.h"
#include "Generation/TileGenerationPasses.h"

using namespace TileTestUtils;

namespace TileGenerationSpecPrivate
{
	// The default pipeline, in the order the generator uses.
	struct FDefaultPasses
	{
		UTerrainNoisePass* Terrain = nullptr;
		UBiomeAssignmentPass* Biomes = nullptr;
		UTileTypePass* TileTypes = nullptr;
		ULevelConstraintPass* Terraces = nullptr;
		UShorePass* Shores = nullptr;
		UTileSmoothingPass* Smoothing = nullptr;
		UFertilityPass* Fertility = nullptr;

		TArray<const UTileGenerationPass*> List() const
		{
			return { Terrain, Biomes, TileTypes, Terraces, Shores, Smoothing, Fertility };
		}
	};

	FDefaultPasses MakeDefaultPasses(int32 InMaxLevel)
	{
		FDefaultPasses Passes;
		Passes.Terrain = NewObject<UTerrainNoisePass>();
		Passes.Terrain->MaxLevel = InMaxLevel;
		// Higher than the game default so even small test maps have both water and land.
		Passes.Terrain->WaterLevel = 0.45f;
		Passes.Biomes = NewObject<UBiomeAssignmentPass>();
		Passes.TileTypes = NewObject<UTileTypePass>();
		Passes.Terraces = NewObject<ULevelConstraintPass>();
		Passes.Shores = NewObject<UShorePass>();
		Passes.Smoothing = NewObject<UTileSmoothingPass>();
		Passes.Fertility = NewObject<UFertilityPass>();
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
			const TArray<FName> Pipeline = { TEXT("Terrain"), TEXT("Biomes"), TEXT("Tile Types"), TEXT("Terraces"), TEXT("Shores"), TEXT("Smoothing"), TEXT("Fertility") };

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

		It("keeps the world flat when MaxLevel is 0", [this]()
		{
			FTileGrid Grid(MakeLayout(32, 32));
			FTileGenerationContext Context(Grid, 9);
			NewObject<UTerrainNoisePass>()->Execute(Context);

			bool bFlat = true;
			for (const FTileData& Tile : Grid.GetTiles()) bFlat &= Tile.Height == 0;
			TestTrue(TEXT("All tiles on level 0"), bFlat);
		});

		It("assigns levels from height, water level and level step", [this]()
		{
			UTerrainNoisePass* Terrain = NewObject<UTerrainNoisePass>();
			Terrain->MaxLevel = 3;
			Terrain->LevelStep = 0.05f;

			FTileGrid Grid(MakeLayout(96, 96));
			FTileGenerationContext Context(Grid, 11);
			Terrain->Execute(Context);

			int32 Wrong = 0;
			uint8 HighestLevel = 0;
			for (int32 Index = 0; Index < Grid.GetLayout().NumTiles(); ++Index)
			{
				const FTileData& Tile = Grid.GetTile(Grid.GetLayout().ToTile(Index));
				if (Tile.IsWater()) continue;

				const int32 Expected = FMath::Clamp(FMath::FloorToInt32((Context.HeightField[Index] - Terrain->WaterLevel) / Terrain->LevelStep), 0, 3);
				if (Tile.Height != Expected) ++Wrong;
				HighestLevel = FMath::Max(HighestLevel, Tile.Height);
			}
			TestEq(*this, TEXT("Tiles on the wrong level"), Wrong, 0);
			TestTrue(TEXT("Several levels are used"), HighestLevel >= 2);
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

			NewObject<ULevelConstraintPass>()->Execute(Context);

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
			Pass->Execute(Context);

			TestEq(*this, TEXT("Last tile kept"), Grid.GetTile(FIntPoint(3, 0)).Height, static_cast<uint8>(6));

			Pass->MaxStep = 1;
			Pass->Execute(Context);
			TestEq(*this, TEXT("Now cut to steps of one"), Grid.GetTile(FIntPoint(3, 0)).Height, static_cast<uint8>(3));
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

		It("uses the biome's override when it has one", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGG"), TEXT("GRG"), TEXT("GGG") });
			UBiomeTypeData* Plains = MakeBiome(EBiomeType::Plains, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0));
			Plains->SmoothingOverrides.Add(ETileType::Rock, ETileType::Dirt);

			FTileGenerationContext Context(Grid, 1);
			Context.Biomes = MakeBiomeSet({ Plains });

			NewObject<UTileSmoothingPass>()->Execute(Context);
			TestEq(*this, TEXT("Override"), Grid.GetTile(FIntPoint(1, 1)).Type, ETileType::Dirt);
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

			int32 NotGenerated = 0, LandWithoutBiome = 0, WaterAboveZero = 0, SteepSteps = 0, BareShores = 0, BadFertility = 0;
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
					if (Data.Height != 0) ++WaterAboveZero;
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
			TestEq(*this, TEXT("Water tiles above level 0"), WaterAboveZero, 0);
			TestEq(*this, TEXT("Neighbours more than one level apart"), SteepSteps, 0);
			TestEq(*this, TEXT("Land next to water that is not sand"), BareShores, 0);
			TestEq(*this, TEXT("Fertility or moisture above 100"), BadFertility, 0);
			TestEq(*this, TEXT("Water or rock with fertility"), FertileBarrenTiles, 0);
			// Every tile used to get its biome's base value. Moisture must now make grass differ.
			TestTrue(TEXT("Grass fertility varies"), GrassFertility.Num() > 1);
			TestTrue(TEXT("Some water and some land"), WaterTiles > 0 && WaterTiles < Layout.NumTiles());
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
