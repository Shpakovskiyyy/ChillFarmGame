// Specs for the terrain numbers (flat land, cliffs, farm sites, terrace width) used to tune generation.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TileTestUtils.h"
#include "TileTerrainStats.h"

using namespace TileTestUtils;


BEGIN_DEFINE_SPEC(FTileTerrainStatsSpec, "ChillFarmGame.TileWorld.TerrainStats", TileTestUtils::SpecFlags)
END_DEFINE_SPEC(FTileTerrainStatsSpec)

void FTileTerrainStatsSpec::Define()
{
	Describe("TileTerrainStats::Compute", [this]()
	{
		It("measures a flat map as one big flat area", [this]()
		{
			const FTileGrid Grid = MakeGrid({ TEXT("GGGGG"), TEXT("GGGGG"), TEXT("GGGGG"), TEXT("GGGGG"), TEXT("GGGGG") });
			const FTileTerrainStats Stats = TileTerrainStats::Compute(Grid, 5);

			TestEq(*this, TEXT("Land"), Stats.LandTiles, 25);
			TestEq(*this, TEXT("Flat"), Stats.FlatTiles, 25);
			TestEq(*this, TEXT("Raised"), Stats.RaisedTiles, 0);
			TestEq(*this, TEXT("Cliff edges"), Stats.CliffEdges, 0);
			TestEq(*this, TEXT("Flat areas"), Stats.FlatAreas, 1);
			TestEq(*this, TEXT("Largest square"), Stats.LargestFlatSquare, 5);
			TestEq(*this, TEXT("Farm sites"), Stats.FarmSites, 1);

			// 16 tiles on the border (1), 8 one ring in (2), the centre (3).
			TestTrue(TEXT("Mean edge distance"), FMath::IsNearlyEqual(Stats.MeanEdgeDistance, 35.f / 25.f));
		});

		It("splits areas at cliffs and counts the cliff edges", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGGG"), TEXT("GGGG"), TEXT("GGGG") });
			SetHeights(Grid, { TEXT("0011"), TEXT("0011"), TEXT("0011") });
			const FTileTerrainStats Stats = TileTerrainStats::Compute(Grid);

			TestEq(*this, TEXT("Raised"), Stats.RaisedTiles, 6);
			TestEq(*this, TEXT("Cliff edges"), Stats.CliffEdges, 3);
			TestEq(*this, TEXT("Flat areas"), Stats.FlatAreas, 2);
			TestEq(*this, TEXT("Largest square"), Stats.LargestFlatSquare, 2);
		});

		It("leaves slopes out of the flat land and joins the levels they connect", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGGG"), TEXT("GGGG"), TEXT("GGGG") });
			SetHeights(Grid, { TEXT("0011"), TEXT("0011"), TEXT("0011") });
			Grid.ApplyMutation(FIntPoint(1, 1), FTileMutation().SetSlope(TileData::MakeEdgeSlope(FIntPoint(1, 0))));
			const FTileTerrainStats Stats = TileTerrainStats::Compute(Grid);

			TestEq(*this, TEXT("Flat"), Stats.FlatTiles, 11);
			TestEq(*this, TEXT("Cliff edges"), Stats.CliffEdges, 2);
			TestEq(*this, TEXT("Flat areas"), Stats.FlatAreas, 2);
			// The slope breaks the left 2 x 2 square; the right side still has one.
			TestEq(*this, TEXT("Largest square"), Stats.LargestFlatSquare, 2);
		});

		It("counts plots that do not overlap, several in one big area", [this]()
		{
			// One flat 7 x 4 area: three 2 x 2 plots across, two rows of them, six in all.
			const FTileGrid Grid = MakeGrid({ TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG"), TEXT("GGGGGGG") });
			const FTileTerrainStats Stats = TileTerrainStats::Compute(Grid, 2);
			TestEq(*this, TEXT("One site"), Stats.FarmSites, 1);
			TestEq(*this, TEXT("Six plots"), Stats.FarmPlots, 6);
		});

		It("counts farm sites against the plot size and ignores water", [this]()
		{
			const FTileGrid Grid = MakeGrid({ TEXT("GGGWGGG"), TEXT("GGGWGGG"), TEXT("GGGWGGG") });

			const FTileTerrainStats Fits = TileTerrainStats::Compute(Grid, 3);
			TestEq(*this, TEXT("Land"), Fits.LandTiles, 18);
			TestEq(*this, TEXT("Two 3 x 3 sites"), Fits.FarmSites, 2);
			TestEq(*this, TEXT("One plot each"), Fits.FarmPlots, 2);

			TestEq(*this, TEXT("Too small for 4 x 4"), TileTerrainStats::Compute(Grid, 4).FarmSites, 0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
