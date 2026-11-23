// Specs for walkable areas: which neighbours a character can step between and the flood fill over them.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TileTestUtils.h"
#include "TileReachability.h"

using namespace TileTestUtils;

namespace TileReachabilitySpecPrivate
{
	FTileData MakeLand(uint8 InLevel, uint8 InSlope = 0)
	{
		FTileData Tile;
		Tile.Type = ETileType::Grass;
		Tile.Height = InLevel;
		Tile.Slope = InSlope;
		return Tile;
	}

	int32 RegionAt(const FTileGrid& InGrid, const TArray<int32>& InRegions, int32 X, int32 Y)
	{
		return InRegions[InGrid.GetLayout().ToIndex(FIntPoint(X, Y))];
	}
}

using namespace TileReachabilitySpecPrivate;


BEGIN_DEFINE_SPEC(FTileReachabilitySpec, "ChillFarmGame.TileWorld.Reachability", TileTestUtils::SpecFlags)
END_DEFINE_SPEC(FTileReachabilitySpec)

void FTileReachabilitySpec::Define()
{
	Describe("CanStep", [this]()
	{
		const FIntPoint East(1, 0);

		It("walks between land tiles on the same level", [this, East]()
		{
			TestTrue(TEXT("Level 2"), TileReachability::CanStep(MakeLand(2), MakeLand(2), East));
		});

		It("does not walk into water or onto tiles that are not generated", [this, East]()
		{
			FTileData Water = MakeLand(0);
			Water.Type = ETileType::Water;
			TestFalse(TEXT("Into water"), TileReachability::CanStep(MakeLand(0), Water, East));
			TestFalse(TEXT("Out of water"), TileReachability::CanStep(Water, MakeLand(0), East));
			TestFalse(TEXT("Not generated"), TileReachability::CanStep(MakeLand(0), FTileData(), East));
		});

		It("climbs a level where the surfaces meet", [this, East]()
		{
			const FIntPoint West(-1, 0);
			TestFalse(TEXT("Cliff up"), TileReachability::CanStep(MakeLand(0), MakeLand(1), East));
			TestTrue(TEXT("Slope up"), TileReachability::CanStep(MakeLand(0, TileData::MakeEdgeSlope(East)), MakeLand(1), East));
			TestTrue(TEXT("Down the same slope"), TileReachability::CanStep(MakeLand(1), MakeLand(0, TileData::MakeEdgeSlope(West)), East));
			TestFalse(TEXT("Slope facing away"), TileReachability::CanStep(MakeLand(0, TileData::MakeEdgeSlope(West)), MakeLand(1), East));
			TestFalse(TEXT("Two levels"), TileReachability::CanStep(MakeLand(0, TileData::MakeEdgeSlope(East)), MakeLand(2), East));
		});

		It("steps where only one end of the edge meets", [this, East]()
		{
			// A slope rising north, with higher ground to its east: they meet at the north-east corner.
			const uint8 RisesNorth = TileData::MakeEdgeSlope(FIntPoint(0, -1));
			TestTrue(TEXT("Beside the top of a slope"), TileReachability::CanStep(MakeLand(0, RisesNorth), MakeLand(1), East));

			// Off the side of the slope onto its own level: they meet at the south-east corner.
			TestTrue(TEXT("Beside the foot of a slope"), TileReachability::CanStep(MakeLand(0, RisesNorth), MakeLand(0), East));

			// An outer corner against the same higher ground: the raised corner is on the far side.
			TestFalse(TEXT("Corner raised away"), TileReachability::CanStep(MakeLand(0, Corners(ETileCorner::NorthWest)), MakeLand(1), East));
		});
	});

	Describe("ComputeWalkRegions", [this]()
	{
		It("splits land by water and by cliffs", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGWGG"), TEXT("GGWGG") });
			SetHeights(Grid, { TEXT("00000"), TEXT("01000") });

			TArray<int32> Regions;
			TestEq(*this, TEXT("Areas"), TileReachability::ComputeWalkRegions(Grid, Regions), 3);
			TestEq(*this, TEXT("Water"), RegionAt(Grid, Regions, 2, 0), static_cast<int32>(INDEX_NONE));
			TestEq(*this, TEXT("First area"), RegionAt(Grid, Regions, 0, 0), 0);
			// Numbered in grid order: the first tile of the land past the water comes before the raised tile.
			TestEq(*this, TEXT("Land past the water"), RegionAt(Grid, Regions, 3, 0), 1);
			TestEq(*this, TEXT("Raised tile is its own area"), RegionAt(Grid, Regions, 1, 1), 2);
			TestEq(*this, TEXT("Same area"), RegionAt(Grid, Regions, 0, 1), 0);
		});

		It("counts land joined by cliffs as one island", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGWGG"), TEXT("GGWGG") });
			SetHeights(Grid, { TEXT("00000"), TEXT("01000") });

			TArray<int32> Islands;
			TestEq(*this, TEXT("Islands"), TileReachability::ComputeLandIslands(Grid, Islands), 2);
			TestEq(*this, TEXT("Raised tile is on the first island"), RegionAt(Grid, Islands, 1, 1), 0);
			TestEq(*this, TEXT("Water"), RegionAt(Grid, Islands, 2, 1), static_cast<int32>(INDEX_NONE));
		});

		It("joins two levels through a slope", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGGG") });
			SetHeights(Grid, { TEXT("0011") });

			TArray<int32> Regions;
			TestEq(*this, TEXT("Without a slope"), TileReachability::ComputeWalkRegions(Grid, Regions), 2);

			Grid.ApplyMutation(FIntPoint(1, 0), FTileMutation().SetSlope(TileData::MakeEdgeSlope(FIntPoint(1, 0))));
			TestEq(*this, TEXT("With a slope"), TileReachability::ComputeWalkRegions(Grid, Regions), 1);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
