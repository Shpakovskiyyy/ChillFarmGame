// Terrain report: generates maps with the game's real pipeline and data for a range of seeds and
// settings, and writes their terrain numbers (TileTerrainStats) as a table. Not a check: it is how the
// generation settings are tuned. Run "ChillFarmGame.Reports.TerrainStats" from the Session Frontend
// or the command line; the table is in the log and in Saved/Reports/TerrainStats.md.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TileTestUtils.h"
#include "TileTerrainStats.h"
#include "TileWorldGenerator.h"
#include "BiomeTypeDataSet.h"
#include "TilesGenerationRuleSet.h"
#include "Generation/TileGenerationContext.h"
#include "Generation/TileGenerationPass.h"
#include "Generation/TileGenerationPasses.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"


namespace TerrainReportPrivate
{
	const TCHAR* BiomeSetPath = TEXT("/Game/ChillFarmGame/GenerationTest/DA_BiomesSet.DA_BiomesSet");
	const TCHAR* DefaultRulesPath = TEXT("/Game/ChillFarmGame/GenerationTest/TileGenerationRules/DA_Tiles_GenerationRules_Default.DA_Tiles_GenerationRules_Default");

	constexpr int32 NumSeeds = 12;
	constexpr int32 PlotSize = 12;

	// One table per variant: the default pipeline with a few settings changed. A value below 0 keeps
	// the pipeline's default.
	struct FReportVariant
	{
		const TCHAR* Label;
		float LevelStep;
		float MinLakeDepth;
	};
	const FReportVariant Variants[] = {
		{ TEXT("Defaults"),          -1.f,  -1.f },
		{ TEXT("LevelStep 0.10"),    0.10f, -1.f },
		{ TEXT("LevelStep 0.20"),    0.20f, -1.f },
		{ TEXT("Lake depth 0.01"),   -1.f,  0.01f },
		{ TEXT("Lake depth 0.005"),  -1.f,  0.005f },
	};

	// Changes counted by the step and reason that made them.
	struct FReportRow
	{
		int32 Seed = 0;
		FTileTerrainStats Stats;
		float Ramps = 0.f;
		float LakeTiles = 0.f;
		float RiverTiles = 0.f;
	};

	FString FormatRow(const FString& InLabel, const FTileTerrainStats& Stats, const FReportRow& InCounts)
	{
		return FString::Printf(TEXT("| %s | %.0f%% | %.0f%% | %d | %d | %d | %d | %d | %.1f | %.1f | %.0f | %.0f |"),
			*InLabel, Stats.FlatShare() * 100.f, Stats.RaisedShare() * 100.f, Stats.CliffEdges, Stats.FlatAreas,
			Stats.FarmSites, Stats.FarmPlots, Stats.LargestFlatSquare, Stats.MeanEdgeDistance,
			InCounts.Ramps, InCounts.LakeTiles, InCounts.RiverTiles);
	}

	const TCHAR* ReportTableHeader =
		TEXT("| | Flat | Raised | Cliff edges | Flat areas | Farm sites | Farm plots | Largest flat square | Mean edge distance | Ramps | Lake tiles | River tiles |\n")
		TEXT("|---|---|---|---|---|---|---|---|---|---|---|---|");
}

using namespace TerrainReportPrivate;


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainReport, "ChillFarmGame.Reports.TerrainStats", TileTestUtils::SpecFlags)

bool FTerrainReport::RunTest(const FString& Parameters)
{
	const UBiomeTypeDataSet* Biomes = LoadObject<UBiomeTypeDataSet>(nullptr, BiomeSetPath);
	UTilesGenerationRuleSet* DefaultRules = LoadObject<UTilesGenerationRuleSet>(nullptr, DefaultRulesPath);
	if (!Biomes || !DefaultRules)
	{
		AddError(TEXT("Could not load the biome set or the default rule set of GenerationTest"));
		return false;
	}

	const ATileWorldGenerator* Generator = GetDefault<ATileWorldGenerator>();
	const FTileGridLayout Layout = Generator->MakeLayout();

	FString Report = FString::Printf(TEXT("# Terrain report\n\n%d x %d tiles, seeds 1..%d, farm site = flat area holding a %d x %d plot. ")
		TEXT("The game's default pipeline and the GenerationTest biome data, with the settings named in each table changed.\n"),
		Layout.Size.X, Layout.Size.Y, NumSeeds, PlotSize, PlotSize);
	FString Summary = FString::Printf(TEXT("\n## Averages per variant\n\n%s\n"), ReportTableHeader);

	for (const FReportVariant& Variant : Variants)
	{
		// A private copy of the pipeline, so the settings can change without touching the defaults.
		TArray<UTileGenerationPass*> Copies;
		for (const UTileGenerationPass* Pass : Generator->GetPasses())
		{
			if (!Pass) continue;
			UTileGenerationPass* Copy = DuplicateObject<UTileGenerationPass>(Pass, GetTransientPackage());
			if (UElevationPass* Elevation = Cast<UElevationPass>(Copy); Elevation && Variant.LevelStep > 0.f) Elevation->LevelStep = Variant.LevelStep;
			if (ULakesAndRiversPass* Water = Cast<ULakesAndRiversPass>(Copy); Water && Variant.MinLakeDepth > 0.f) Water->MinLakeDepth = Variant.MinLakeDepth;
			Copies.Add(Copy);
		}
		const TArray<const UTileGenerationPass*> PassList(Copies);

		TArray<FReportRow> Rows;
		Rows.Reserve(NumSeeds);
		for (int32 Seed = 1; Seed <= NumSeeds; ++Seed)
		{
			FTileGrid Grid(Layout);
			FTileGenerationContext Context(Grid, Seed);
			Context.Biomes = Biomes;
			Context.DefaultRuleSet = DefaultRules;

			FReportRow& Row = Rows.AddDefaulted_GetRef();
			Row.Seed = Seed;
			Context.OnChange = [&Row](const FTileChange& Change)
			{
				if (Change.Context.Step == FName(TEXT("Ramps"))) ++Row.Ramps;
				if (Change.Context.Reason == FName(TEXT("Fills a basin"))) ++Row.LakeTiles;
				if (Change.Context.Reason == FName(TEXT("River"))) ++Row.RiverTiles;
			};

			if (!TileGeneration::RunPasses(PassList, Context))
			{
				AddError(FString::Printf(TEXT("Generation failed for seed %d"), Seed));
				return false;
			}
			Row.Stats = TileTerrainStats::Compute(Grid, PlotSize);
		}

		Report += FString::Printf(TEXT("\n## %s\n\n%s\n"), Variant.Label, ReportTableHeader);
		FTileTerrainStats Sum;
		FReportRow Counts;
		int32 FewestSites = MAX_int32;
		int32 FewestPlots = MAX_int32;
		for (const FReportRow& Row : Rows)
		{
			Report += FormatRow(FString::Printf(TEXT("Seed %d"), Row.Seed), Row.Stats, Row) + TEXT("\n");
			Sum.LandTiles += Row.Stats.LandTiles;
			Sum.FlatTiles += Row.Stats.FlatTiles;
			Sum.RaisedTiles += Row.Stats.RaisedTiles;
			Sum.CliffEdges += Row.Stats.CliffEdges;
			Sum.FlatAreas += Row.Stats.FlatAreas;
			Sum.FarmSites += Row.Stats.FarmSites;
			Sum.FarmPlots += Row.Stats.FarmPlots;
			Sum.LargestFlatSquare += Row.Stats.LargestFlatSquare;
			Sum.MeanEdgeDistance += Row.Stats.MeanEdgeDistance;
			Counts.Ramps += Row.Ramps / NumSeeds;
			Counts.LakeTiles += Row.LakeTiles / NumSeeds;
			Counts.RiverTiles += Row.RiverTiles / NumSeeds;
			FewestSites = FMath::Min(FewestSites, Row.Stats.FarmSites);
			FewestPlots = FMath::Min(FewestPlots, Row.Stats.FarmPlots);
		}

		// Averages: the shares come from the summed tiles, the rest is a plain mean over the seeds.
		FTileTerrainStats Mean = Sum;
		Mean.CliffEdges = FMath::RoundToInt32(static_cast<float>(Sum.CliffEdges) / NumSeeds);
		Mean.FlatAreas = FMath::RoundToInt32(static_cast<float>(Sum.FlatAreas) / NumSeeds);
		Mean.FarmSites = FMath::RoundToInt32(static_cast<float>(Sum.FarmSites) / NumSeeds);
		Mean.FarmPlots = FMath::RoundToInt32(static_cast<float>(Sum.FarmPlots) / NumSeeds);
		Mean.LargestFlatSquare = FMath::RoundToInt32(static_cast<float>(Sum.LargestFlatSquare) / NumSeeds);
		Mean.MeanEdgeDistance = Sum.MeanEdgeDistance / NumSeeds;
		Summary += FormatRow(FString::Printf(TEXT("%s (fewest: %d sites, %d plots)"), Variant.Label, FewestSites, FewestPlots), Mean, Counts) + TEXT("\n");
	}

	Report += Summary;

	const FString Path = FPaths::ProjectSavedDir() / TEXT("Reports") / TEXT("TerrainStats.md");
	FFileHelper::SaveStringToFile(Report, *Path);

	TArray<FString> Lines;
	Report.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines) AddInfo(Line);
	AddInfo(FString::Printf(TEXT("Written to %s"), *FPaths::ConvertRelativePathToFull(Path)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
