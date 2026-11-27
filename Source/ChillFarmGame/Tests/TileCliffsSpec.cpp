// Specs for the cliff sides between elevation levels: which edges get a side, and where the wall goes.
// Pure grid and transform math, no world needed.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TileTestUtils.h"
#include "TileCliffs.h"

using namespace TileTestUtils;

namespace TileCliffsSpecPrivate
{
	TArray<FTileCliffFace> CollectAll(const FTileGrid& InGrid)
	{
		TArray<FTileCliffFace> Faces;
		TileCliffs::CollectFaces(InGrid, FIntPoint::ZeroValue, InGrid.GetSize(), Faces);
		return Faces;
	}

	TArray<FTileCliffFace> FacesOf(const TArray<FTileCliffFace>& InFaces, const FIntPoint& InTile)
	{
		return InFaces.FilterByPredicate([&InTile](const FTileCliffFace& Face) { return Face.Tile == InTile; });
	}

	bool IsNear(const FVector& A, const FVector& B) { return A.Equals(B, 0.01); }
}

using namespace TileCliffsSpecPrivate;


BEGIN_DEFINE_SPEC(FTileCliffsSpec, "ChillFarmGame.TileWorld.Cliffs", TileTestUtils::SpecFlags)
END_DEFINE_SPEC(FTileCliffsSpec)

void FTileCliffsSpec::Define()
{
	Describe("CollectFaces", [this]()
	{
		It("finds nothing on a flat map at level 0", [this]()
		{
			const FTileGrid Grid = MakeGrid({ TEXT("GGG"), TEXT("GWG") });
			TestEq(*this, TEXT("Faces"), CollectAll(Grid).Num(), 0);
		});

		It("gives a raised tile a side towards every lower neighbour", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGG"), TEXT("GGG"), TEXT("GGG") });
			SetHeights(Grid, { TEXT("000"), TEXT("010"), TEXT("000") });

			const TArray<FTileCliffFace> Faces = CollectAll(Grid);
			TestEq(*this, TEXT("Faces"), Faces.Num(), 4);
			for (const FIntPoint& Offset : FTileGrid::GetCardinalOffsets())
			{
				const FTileCliffFace Expected{ FIntPoint(1, 1), Offset, 1, 0 };
				TestTrue(*FString::Printf(TEXT("Side towards %s"), *ToTestString(Offset)), Faces.Contains(Expected));
			}
		});

		It("puts the side on the high tile only, spanning the whole drop", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGG"), TEXT("GGG"), TEXT("GGG") });
			SetHeights(Grid, { TEXT("333"), TEXT("313"), TEXT("333") });

			const TArray<FTileCliffFace> Faces = CollectAll(Grid);
			TestEq(*this, TEXT("Low tile has no side"), FacesOf(Faces, FIntPoint(1, 1)).Num(), 0);

			const TArray<FTileCliffFace> North = FacesOf(Faces, FIntPoint(1, 0));
			const FTileCliffFace* Inner = North.FindByPredicate([](const FTileCliffFace& Face) { return Face.Direction == FIntPoint(0, 1); });
			if (!TestNotNull(TEXT("Side facing the low tile"), Inner)) return;
			TestEq(*this, TEXT("Top"), Inner->TopLevel, static_cast<uint8>(3));
			TestEq(*this, TEXT("Bottom"), Inner->BottomLevel, static_cast<uint8>(1));
		});

		It("closes a raised map edge down to level 0", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GG"), TEXT("GG") });
			SetHeights(Grid, { TEXT("22"), TEXT("22") });

			const TArray<FTileCliffFace> Faces = CollectAll(Grid);
			TestEq(*this, TEXT("Two outer sides per corner tile"), Faces.Num(), 8);
			for (const FTileCliffFace& Face : Faces)
			{
				TestFalse(TEXT("Side points out of the map"), Grid.GetLayout().Contains(Face.Tile + Face.Direction));
				TestEq(*this, TEXT("Down to level 0"), Face.BottomLevel, static_cast<uint8>(0));
			}
		});

		It("only looks at tiles in the requested range", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGGG") });
			SetHeights(Grid, { TEXT("0101") });

			TArray<FTileCliffFace> Faces;
			TileCliffs::CollectFaces(Grid, FIntPoint(0, 0), FIntPoint(2, 1), Faces);
			for (const FTileCliffFace& Face : Faces)
			{
				TestEq(*this, TEXT("Tile"), Face.Tile, FIntPoint(1, 0));
			}
			TestEq(*this, TEXT("Faces of tile (1, 0)"), Faces.Num(), 4);
		});

		It("leaves the edge open where a slope rises to the tile", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GGG"), TEXT("GGG"), TEXT("GGG") });
			SetHeights(Grid, { TEXT("000"), TEXT("010"), TEXT("000") });
			Grid.ApplyMutation(FIntPoint(1, 2), FTileMutation().SetSlope(TileData::MakeEdgeSlope(FIntPoint(0, -1))));

			const TArray<FTileCliffFace> Faces = CollectAll(Grid);
			TestEq(*this, TEXT("Faces"), Faces.Num(), 3);
			TestFalse(TEXT("No side towards the slope"), Faces.Contains(FTileCliffFace{ FIntPoint(1, 1), FIntPoint(0, 1), 1, 0 }));
		});

		It("reaches down to a slope's low corner where the slope runs out into a cliff", [this]()
		{
			// The slope at (0, 1) rises north; the higher tile (1, 1) is beside it, so only the
			// north-east corner meets.
			FTileGrid Grid = MakeGrid({ TEXT("GG"), TEXT("GG") });
			SetHeights(Grid, { TEXT("11"), TEXT("01") });
			Grid.ApplyMutation(FIntPoint(0, 1), FTileMutation().SetSlope(TileData::MakeEdgeSlope(FIntPoint(0, -1))));

			const TArray<FTileCliffFace> Faces = CollectAll(Grid);
			TestTrue(TEXT("Higher ground beside the slope"), Faces.Contains(FTileCliffFace{ FIntPoint(1, 1), FIntPoint(-1, 0), 1, 0 }));
			TestFalse(TEXT("No side above the slope"), Faces.Contains(FTileCliffFace{ FIntPoint(0, 0), FIntPoint(0, 1), 1, 0 }));

			// The slope itself: its low end is on its own level, its sides close themselves.
			for (const FTileCliffFace& Face : Faces)
			{
				TestFalse(TEXT("No side from the slope"), Face.Tile == FIntPoint(0, 1));
			}
		});

		It("skips tiles that are not generated", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("G.") });
			SetHeights(Grid, { TEXT("01") });
			TestEq(*this, TEXT("Faces"), CollectAll(Grid).Num(), 0);
		});
	});

	Describe("MakeWallTransform", [this]()
	{
		// Test layout: 100 units per tile, 50 per level, origin at zero.
		const FTileGridLayout Layout = MakeLayout(3, 3);

		It("stretches a centred cube into the side, inside the high tile", [this, Layout]()
		{
			const FBox Cube(FVector(-50.0), FVector(50.0));
			const FTileCliffFace East{ FIntPoint(1, 1), FIntPoint(1, 0), 2, 0 };

			const FBox Wall = Cube.TransformBy(TileCliffs::MakeWallTransform(Layout, East, Cube, 10.0));
			TestTrue(TEXT("Min"), IsNear(Wall.Min, FVector(190.0, 100.0, 0.0)));
			TestTrue(TEXT("Max"), IsNear(Wall.Max, FVector(200.0, 200.0, 99.5)));
		});

		It("does not depend on the mesh pivot", [this, Layout]()
		{
			const FBox CornerPivot(FVector(0.0), FVector(200.0, 40.0, 10.0));
			const FTileCliffFace North{ FIntPoint(1, 1), FIntPoint(0, -1), 1, 0 };

			const FBox Wall = CornerPivot.TransformBy(TileCliffs::MakeWallTransform(Layout, North, CornerPivot, 10.0));
			TestTrue(TEXT("Min"), IsNear(Wall.Min, FVector(100.0, 100.0, 0.0)));
			TestTrue(TEXT("Max"), IsNear(Wall.Max, FVector(200.0, 110.0, 49.5)));
		});

		It("tilts a flat tile mesh into a ramp from its level to the next", [this, Layout]()
		{
			// A flat plate whose top face is at Z = 0, centred on its pivot.
			const FBox Plate(FVector(-50.0, -50.0, -1.0), FVector(50.0, 50.0, 0.0));
			const FTransform Transform = TileCliffs::MakeTiltedTileTransform(Layout, FIntPoint(1, 1), 2, FIntPoint(1, 0), Plate);

			// Middle of the top face's low and high edges (test layout: 100 per tile, 50 per level).
			TestTrue(TEXT("Low edge"), IsNear(Transform.TransformPosition(FVector(-50.0, 0.0, 0.0)), FVector(100.0, 150.0, 100.0)));
			TestTrue(TEXT("High edge"), IsNear(Transform.TransformPosition(FVector(50.0, 0.0, 0.0)), FVector(200.0, 150.0, 150.0)));
			TestTrue(TEXT("Side"), IsNear(Transform.TransformPosition(FVector(0.0, 50.0, 0.0)), FVector(150.0, 200.0, 125.0)));
		});

		It("turns the ramp towards its direction", [this, Layout]()
		{
			const FBox Plate(FVector(-50.0, -50.0, -1.0), FVector(50.0, 50.0, 0.0));
			const FTransform North = TileCliffs::MakeTiltedTileTransform(Layout, FIntPoint(1, 1), 0, FIntPoint(0, -1), Plate);

			// North is -Y: the high edge is on the tile's north side.
			TestTrue(TEXT("High edge"), IsNear(North.TransformPosition(FVector(50.0, 0.0, 0.0)), FVector(150.0, 100.0, 50.0)));
			TestTrue(TEXT("Low edge"), IsNear(North.TransformPosition(FVector(-50.0, 0.0, 0.0)), FVector(150.0, 200.0, 0.0)));
		});

		It("fits a wedge mesh into the slope tile, turned to the slope's direction", [this, Layout]()
		{
			// Like LevelPrototyping's SM_Ramp: a 100 cube box, high at Y = 0, low at Y = 100 (rises north).
			const FBox Wedge(FVector(0.0), FVector(100.0));
			const FTransform East = TileCliffs::MakeSlopePieceTransform(Layout, FIntPoint(1, 1), 2, FIntPoint(1, 0), Wedge, FIntPoint(0, -1));

			TestTrue(TEXT("High edge on the east side, one level up"), IsNear(East.TransformPosition(FVector(50.0, 0.0, 100.0)), FVector(200.0, 150.0, 150.0)));
			TestTrue(TEXT("Low edge on the west side, on the tile's level"), IsNear(East.TransformPosition(FVector(50.0, 100.0, 0.0)), FVector(100.0, 150.0, 100.0)));
			TestTrue(TEXT("Fills the tile"), Wedge.TransformBy(East).Equals(FBox(FVector(100.0, 100.0, 100.0), FVector(200.0, 200.0, 150.0)), 0.01));
		});

		It("turns a corner piece from its own raised corner to the slope's", [this, Layout]()
		{
			// A corner piece in a 100 cube box whose raised corner is north-east (max X, min Y).
			const FBox Piece(FVector(0.0), FVector(100.0));
			const FTransform SouthWest = TileCliffs::MakeSlopePieceTransform(Layout, FIntPoint(1, 1), 0, FIntPoint(-1, 1), Piece, FIntPoint(1, -1));

			TestTrue(TEXT("Raised corner on the south-west, one level up"), IsNear(SouthWest.TransformPosition(FVector(100.0, 0.0, 100.0)), FVector(100.0, 200.0, 50.0)));
			TestTrue(TEXT("Fills the tile"), Piece.TransformBy(SouthWest).Equals(FBox(FVector(100.0, 100.0, 0.0), FVector(200.0, 200.0, 50.0)), 0.01));
		});

		It("stretches a mesh into a column over one tile between two levels", [this, Layout]()
		{
			// A cube with a corner pivot; test layout: 100 per tile, 50 per level.
			const FBox Cube(FVector(0.0), FVector(100.0));
			const FBox Column = Cube.TransformBy(TileCliffs::MakeColumnTransform(Layout, FIntPoint(1, 2), 1, 4, Cube));
			TestTrue(TEXT("Fills the tile from level 1 to 4"), Column.Equals(FBox(FVector(100.0, 200.0, 50.0), FVector(200.0, 300.0, 200.0)), 0.01));
		});

		It("starts at the lower level, not at the ground", [this, Layout]()
		{
			const FBox Cube(FVector(-50.0), FVector(50.0));
			const FTileCliffFace West{ FIntPoint(1, 1), FIntPoint(-1, 0), 3, 1 };

			const FBox Wall = Cube.TransformBy(TileCliffs::MakeWallTransform(Layout, West, Cube, 10.0));
			TestTrue(TEXT("Min"), IsNear(Wall.Min, FVector(100.0, 100.0, 50.0)));
			TestTrue(TEXT("Max"), IsNear(Wall.Max, FVector(110.0, 200.0, 149.5)));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
