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
	const float LevelSteps[] = { 0.10f, 0.15f, 0.20f };

	struct FRow
	{
		int32 Seed = 0;
		FTileTerrainStats Stats;
		int32 Ramps = 0;
	};

	FString FormatRow(const FString& InLabel, const FTileTerrainStats& Stats, float InRamps)
	{
		return FString::Printf(TEXT("| %s | %.0f%% | %.0f%% | %d | %d | %d | %d | %d | %.1f | %.1f |"),
			*InLabel, Stats.FlatShare() * 100.f, Stats.RaisedShare() * 100.f, Stats.CliffEdges, Stats.FlatAreas,
			Stats.FarmSites, Stats.FarmPlots, Stats.LargestFlatSquare, Stats.MeanEdgeDistance, InRamps);
	}

	const TCHAR* ReportTableHeader = TEXT("| | Flat | Raised | Cliff edges | Flat areas | Farm sites | Farm plots | Largest flat square | Mean edge distance | Ramps |\n|---|---|---|---|---|---|---|---|---|---|");
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
		TEXT("The game's default pipeline and the GenerationTest biome data; only LevelStep changes between the tables.\n"),
		Layout.Size.X, Layout.Size.Y, NumSeeds, PlotSize, PlotSize);
	FString Summary = FString::Printf(TEXT("\n## Averages per LevelStep\n\n%s\n"), ReportTableHeader);

	for (const float LevelStep : LevelSteps)
	{
		// A private copy of the pipeline, so the settings can change without touching the defaults.
		TArray<UTileGenerationPass*> Copies;
		for (const UTileGenerationPass* Pass : Generator->GetPasses())
		{
			if (!Pass) continue;
			UTileGenerationPass* Copy = DuplicateObject<UTileGenerationPass>(Pass, GetTransientPackage());
			if (UElevationPass* Elevation = Cast<UElevationPass>(Copy)) Elevation->LevelStep = LevelStep;
			Copies.Add(Copy);
		}
		const TArray<const UTileGenerationPass*> PassList(Copies);

		TArray<FRow> Rows;
		for (int32 Seed = 1; Seed <= NumSeeds; ++Seed)
		{
			FTileGrid Grid(Layout);
			FTileGenerationContext Context(Grid, Seed);
			Context.Biomes = Biomes;
			Context.DefaultRuleSet = DefaultRules;

			FRow& Row = Rows.AddDefaulted_GetRef();
			Row.Seed = Seed;
			Context.OnChange = [&Row](const FTileChange& Change)
			{
				if (Change.Context.Step == FName(TEXT("Ramps"))) ++Row.Ramps;
			};

			if (!TileGeneration::RunPasses(PassList, Context))
			{
				AddError(FString::Printf(TEXT("Generation failed for seed %d"), Seed));
				return false;
			}
			Row.Stats = TileTerrainStats::Compute(Grid, PlotSize);
		}

		Report += FString::Printf(TEXT("\n## LevelStep %.2f\n\n%s\n"), LevelStep, ReportTableHeader);
		FTileTerrainStats Sum;
		float FlatShare = 0.f, RaisedShare = 0.f, Ramps = 0.f;
		int32 FewestSites = MAX_int32;
		int32 FewestPlots = MAX_int32;
		for (const FRow& Row : Rows)
		{
			Report += FormatRow(FString::Printf(TEXT("Seed %d"), Row.Seed), Row.Stats, static_cast<float>(Row.Ramps)) + TEXT("\n");
			Sum.LandTiles += Row.Stats.LandTiles;
			Sum.FlatTiles += Row.Stats.FlatTiles;
			Sum.RaisedTiles += Row.Stats.RaisedTiles;
			Sum.CliffEdges += Row.Stats.CliffEdges;
			Sum.FlatAreas += Row.Stats.FlatAreas;
			Sum.FarmSites += Row.Stats.FarmSites;
			Sum.FarmPlots += Row.Stats.FarmPlots;
			Sum.LargestFlatSquare += Row.Stats.LargestFlatSquare;
			Sum.MeanEdgeDistance += Row.Stats.MeanEdgeDistance;
			Ramps += Row.Ramps;
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
		Summary += FormatRow(FString::Printf(TEXT("%.2f (fewest: %d sites, %d plots)"), LevelStep, FewestSites, FewestPlots), Mean, Ramps / NumSeeds) + TEXT("\n");
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
