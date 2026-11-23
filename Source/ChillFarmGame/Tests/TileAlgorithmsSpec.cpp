// Specs for the grid algorithms: terrace clamping and the autotile neighbour resolver.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TileTestUtils.h"
#include "TileAlgorithms.h"
#include "TileNeighborResolver.h"
#include "TileVisualKey.h"
#include "TileTypeVisualVariant.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"

using namespace TileTestUtils;

namespace TileAlgorithmsSpecPrivate
{
	// Reference answer for ClampLevelSteps: for every tile, the lowest value of
	// "level of any tile + MaxStep * Manhattan distance to it". Slow but obviously right.
	TArray<uint8> ClampLevelStepsBruteForce(const TArray<uint8>& InLevels, const FIntPoint& InSize, int32 InMaxStep)
	{
		TArray<uint8> Result = InLevels;
		for (int32 Y = 0; Y < InSize.Y; ++Y)
		{
			for (int32 X = 0; X < InSize.X; ++X)
			{
				int32 Best = InLevels[X + Y * InSize.X];
				for (int32 SY = 0; SY < InSize.Y; ++SY)
				{
					for (int32 SX = 0; SX < InSize.X; ++SX)
					{
						const int32 Distance = FMath::Abs(SX - X) + FMath::Abs(SY - Y);
						Best = FMath::Min(Best, InLevels[SX + SY * InSize.X] + InMaxStep * Distance);
					}
				}
				Result[X + Y * InSize.X] = static_cast<uint8>(Best);
			}
		}
		return Result;
	}

	FTileVisualKey PieceAt(const FTileGrid& InGrid, int32 InX, int32 InY)
	{
		const FIntPoint Tile(InX, InY);
		return FTileNeighborResolver::Resolve(InGrid.GetTile(Tile).Type, FTileNeighborResolver::ComputeSameTypeMask(InGrid, Tile));
	}

	constexpr uint8 Bit(ETileNeighborSide InSide) { return static_cast<uint8>(InSide); }

	constexpr uint8 AllSides = 0xFF;

	// Mask operations stay in uint8 (plain & and ~ promote to int).
	constexpr uint8 Without(uint8 InMask, uint8 InBits) { return static_cast<uint8>(InMask & ~InBits); }
	constexpr uint8 With(uint8 InA, uint8 InB) { return static_cast<uint8>(InA | InB); }
}

using namespace TileAlgorithmsSpecPrivate;


BEGIN_DEFINE_SPEC(FTileAlgorithmsSpec, "ChillFarmGame.TileWorld.Algorithms", TileTestUtils::SpecFlags)
END_DEFINE_SPEC(FTileAlgorithmsSpec)

void FTileAlgorithmsSpec::Define()
{
	Describe("ClampLevelSteps", [this]()
	{
		It("leaves a valid field untouched", [this]()
		{
			TArray<uint8> Levels = { 0, 1, 2, 1, 2, 3, 2, 3, 3 };
			const TArray<uint8> Original = Levels;
			TestEq(*this, TEXT("Changed tiles"), TileAlgorithms::ClampLevelSteps(Levels, FIntPoint(3, 3), 1), 0);
			TestTrue(TEXT("Same levels"), Levels == Original);
		});

		It("cuts a spike down to one level above its neighbours", [this]()
		{
			TArray<uint8> Levels;
			Levels.Init(0, 25);
			Levels[12] = 9;
			TestEq(*this, TEXT("Changed tiles"), TileAlgorithms::ClampLevelSteps(Levels, FIntPoint(5, 5), 1), 1);
			TestEq(*this, TEXT("Spike"), Levels[12], static_cast<uint8>(1));
		});

		It("turns a cliff into a staircase", [this]()
		{
			TArray<uint8> Levels = { 0, 5, 5, 5, 5 };
			TileAlgorithms::ClampLevelSteps(Levels, FIntPoint(5, 1), 1);
			TestTrue(TEXT("Steps of one"), Levels == TArray<uint8>({ 0, 1, 2, 3, 4 }));

			TArray<uint8> Wide = { 0, 5, 5, 5, 5 };
			TileAlgorithms::ClampLevelSteps(Wide, FIntPoint(5, 1), 2);
			TestTrue(TEXT("Steps of two"), Wide == TArray<uint8>({ 0, 2, 4, 5, 5 }));
		});

		It("matches the brute force answer on random fields", [this]()
		{
			FRandomStream Stream(20261121);
			for (int32 Run = 0; Run < 200; ++Run)
			{
				const FIntPoint Size(Stream.RandRange(1, 8), Stream.RandRange(1, 8));
				const int32 MaxStep = Stream.RandRange(1, 3);

				TArray<uint8> Levels;
				for (int32 Index = 0; Index < Size.X * Size.Y; ++Index) Levels.Add(static_cast<uint8>(Stream.RandRange(0, 12)));

				const TArray<uint8> Expected = ClampLevelStepsBruteForce(Levels, Size, MaxStep);
				int32 ExpectedChanges = 0;
				for (int32 Index = 0; Index < Levels.Num(); ++Index) if (Expected[Index] != Levels[Index]) ++ExpectedChanges;

				const int32 Changes = TileAlgorithms::ClampLevelSteps(Levels, Size, static_cast<uint8>(MaxStep));
				if (!TestTrue(FString::Printf(TEXT("Run %d: levels match"), Run), Levels == Expected)) return;
				if (!TestEq(*this, FString::Printf(TEXT("Run %d: change count"), Run), Changes, ExpectedChanges)) return;
			}
		});

		It("never raises a tile and leaves no step above MaxStep", [this]()
		{
			FRandomStream Stream(7);
			const FIntPoint Size(16, 16);
			for (int32 MaxStep = 1; MaxStep <= 3; ++MaxStep)
			{
				TArray<uint8> Original;
				for (int32 Index = 0; Index < Size.X * Size.Y; ++Index) Original.Add(static_cast<uint8>(Stream.RandRange(0, 15)));

				TArray<uint8> Levels = Original;
				TileAlgorithms::ClampLevelSteps(Levels, Size, static_cast<uint8>(MaxStep));

				bool bNeverRaised = true;
				bool bStepsOk = true;
				for (int32 Y = 0; Y < Size.Y; ++Y)
				{
					for (int32 X = 0; X < Size.X; ++X)
					{
						const int32 Index = X + Y * Size.X;
						bNeverRaised &= Levels[Index] <= Original[Index];
						if (X + 1 < Size.X) bStepsOk &= FMath::Abs(Levels[Index] - Levels[Index + 1]) <= MaxStep;
						if (Y + 1 < Size.Y) bStepsOk &= FMath::Abs(Levels[Index] - Levels[Index + Size.X]) <= MaxStep;
					}
				}
				TestTrue(FString::Printf(TEXT("MaxStep %d: never raised"), MaxStep), bNeverRaised);
				TestTrue(FString::Printf(TEXT("MaxStep %d: steps"), MaxStep), bStepsOk);

				TestEq(*this, FString::Printf(TEXT("MaxStep %d: second run changes nothing"), MaxStep),
					TileAlgorithms::ClampLevelSteps(Levels, Size, static_cast<uint8>(MaxStep)), 0);
			}
		});

		It("refuses data that does not match the size", [this]()
		{
			TArray<uint8> Levels = { 9, 0, 0 };
			TestEq(*this, TEXT("Wrong size"), TileAlgorithms::ClampLevelSteps(Levels, FIntPoint(2, 2), 1), 0);
			TestEq(*this, TEXT("Untouched"), Levels[0], static_cast<uint8>(9));

			TArray<uint8> Empty;
			TestEq(*this, TEXT("Empty"), TileAlgorithms::ClampLevelSteps(Empty, FIntPoint(0, 0), 1), 0);
		});
	});

	Describe("FTileNeighborResolver::Resolve", [this]()
	{
		using EPos = ETileVisualPositionType;
		using ESide = ETileNeighborSide;

		It("can produce every piece", [this]()
		{
			TSet<EPos> Seen;
			for (int32 Mask = 0; Mask < 256; ++Mask)
			{
				Seen.Add(FTileNeighborResolver::Resolve(ETileType::Grass, static_cast<uint8>(Mask)).VisualPositionType);
			}
			TestEq(*this, TEXT("Pieces"), Seen.Num(), 5);
		});

		It("keeps the tile type in the key", [this]()
		{
			TestEq(*this, TEXT("Type"), FTileNeighborResolver::Resolve(ETileType::Rock, AllSides).TileType, ETileType::Rock);
		});

		It("uses Center when surrounded and Isolated when alone", [this]()
		{
			const FTileVisualKey Surrounded = FTileNeighborResolver::Resolve(ETileType::Grass, AllSides);
			TestEq(*this, TEXT("Surrounded"), Surrounded.VisualPositionType, EPos::Center);
			TestEq(*this, TEXT("Surrounded rotation"), Surrounded.Rotation, 0);
			TestEq(*this, TEXT("Alone"), FTileNeighborResolver::Resolve(ETileType::Grass, 0).VisualPositionType, EPos::Isolated);
		});

		It("turns an edge towards its open side", [this]()
		{
			const TPair<ESide, int32> Cases[] = { { ESide::N, 0 }, { ESide::E, 90 }, { ESide::S, 180 }, { ESide::W, 270 } };
			for (const auto& [OpenSide, Rotation] : Cases)
			{
				const FTileVisualKey Key = FTileNeighborResolver::Resolve(ETileType::Grass, Without(AllSides, Bit(OpenSide)));
				TestEq(*this, FString::Printf(TEXT("Piece for rotation %d"), Rotation), Key.VisualPositionType, EPos::Edge);
				TestEq(*this, TEXT("Rotation"), Key.Rotation, Rotation);
			}
		});

		It("turns an outer corner towards its two open sides", [this]()
		{
			struct FCase { ESide A; ESide B; int32 Rotation; };
			const FCase Cases[] = { { ESide::N, ESide::E, 0 }, { ESide::E, ESide::S, 90 }, { ESide::S, ESide::W, 180 }, { ESide::W, ESide::N, 270 } };
			for (const FCase& Case : Cases)
			{
				const FTileVisualKey Key = FTileNeighborResolver::Resolve(ETileType::Grass, Without(AllSides, With(Bit(Case.A), Bit(Case.B))));
				TestEq(*this, FString::Printf(TEXT("Piece for rotation %d"), Case.Rotation), Key.VisualPositionType, EPos::Corner);
				TestEq(*this, TEXT("Rotation"), Key.Rotation, Case.Rotation);
			}
		});

		It("turns an inner corner towards its open diagonal", [this]()
		{
			const TPair<ESide, int32> Cases[] = { { ESide::NE, 0 }, { ESide::SE, 90 }, { ESide::SW, 180 }, { ESide::NW, 270 } };
			for (const auto& [OpenDiagonal, Rotation] : Cases)
			{
				const FTileVisualKey Key = FTileNeighborResolver::Resolve(ETileType::Grass, Without(AllSides, Bit(OpenDiagonal)));
				TestEq(*this, FString::Printf(TEXT("Piece for rotation %d"), Rotation), Key.VisualPositionType, EPos::InnerCorner);
				TestEq(*this, TEXT("Rotation"), Key.Rotation, Rotation);
			}
		});

		It("falls back for shapes without a piece", [this]()
		{
			const uint8 TwoOpenDiagonals = Without(AllSides, With(Bit(ESide::NE), Bit(ESide::SW)));
			TestEq(*this, TEXT("Two open diagonals"), FTileNeighborResolver::Resolve(ETileType::Grass, TwoOpenDiagonals).VisualPositionType, EPos::Center);

			const uint8 VerticalStrip = With(Bit(ESide::N), Bit(ESide::S));
			TestEq(*this, TEXT("One tile wide strip"), FTileNeighborResolver::Resolve(ETileType::Grass, VerticalStrip).VisualPositionType, EPos::Isolated);

			const uint8 StripEnd = Bit(ESide::N);
			TestEq(*this, TEXT("End of a strip"), FTileNeighborResolver::Resolve(ETileType::Grass, StripEnd).VisualPositionType, EPos::Isolated);
		});
	});

	Describe("FTileNeighborResolver::ComputeSameTypeMask", [this]()
	{
		using EPos = ETileVisualPositionType;

		It("gives a grass island in water a centre, edges and corners", [this]()
		{
			const FTileGrid Grid = MakeGrid({
				TEXT("WWWWW"),
				TEXT("WGGGW"),
				TEXT("WGGGW"),
				TEXT("WGGGW"),
				TEXT("WWWWW"),
			});

			TestEq(*this, TEXT("Middle"), PieceAt(Grid, 2, 2).VisualPositionType, EPos::Center);

			TestEq(*this, TEXT("North side"), PieceAt(Grid, 2, 1).VisualPositionType, EPos::Edge);
			TestEq(*this, TEXT("North side rotation"), PieceAt(Grid, 2, 1).Rotation, 0);
			TestEq(*this, TEXT("East side rotation"), PieceAt(Grid, 3, 2).Rotation, 90);
			TestEq(*this, TEXT("South side rotation"), PieceAt(Grid, 2, 3).Rotation, 180);
			TestEq(*this, TEXT("West side rotation"), PieceAt(Grid, 1, 2).Rotation, 270);

			TestEq(*this, TEXT("North-east corner"), PieceAt(Grid, 3, 1).VisualPositionType, EPos::Corner);
			TestEq(*this, TEXT("North-east corner rotation"), PieceAt(Grid, 3, 1).Rotation, 0);
			TestEq(*this, TEXT("South-east corner rotation"), PieceAt(Grid, 3, 3).Rotation, 90);
			TestEq(*this, TEXT("South-west corner rotation"), PieceAt(Grid, 1, 3).Rotation, 180);
			TestEq(*this, TEXT("North-west corner rotation"), PieceAt(Grid, 1, 1).Rotation, 270);
		});

		It("finds inner corners", [this]()
		{
			const FTileGrid Grid = MakeGrid({
				TEXT("GGW"),
				TEXT("GGG"),
				TEXT("GGG"),
			});
			TestEq(*this, TEXT("Piece"), PieceAt(Grid, 1, 1).VisualPositionType, EPos::InnerCorner);
			TestEq(*this, TEXT("Rotation"), PieceAt(Grid, 1, 1).Rotation, 0);
		});

		It("treats the map edge as a different neighbour", [this]()
		{
			const FTileGrid Grid = MakeGrid({ TEXT("GG"), TEXT("GG") });
			const uint8 Expected = With(With(Bit(ETileNeighborSide::E), Bit(ETileNeighborSide::SE)), Bit(ETileNeighborSide::S));
			TestEq(*this, TEXT("Top-left mask"), FTileNeighborResolver::ComputeSameTypeMask(Grid, FIntPoint(0, 0)), Expected);
			TestEq(*this, TEXT("Top-left piece"), PieceAt(Grid, 0, 0).VisualPositionType, EPos::Corner);
		});

		It("treats the same type on another level as a different neighbour", [this]()
		{
			FTileGrid Grid = MakeGrid({ TEXT("GG"), TEXT("GG") });
			SetHeights(Grid, { TEXT("01"), TEXT("00") });
			const uint8 Expected = Bit(ETileNeighborSide::S);
			TestEq(*this, TEXT("Top-left mask"), FTileNeighborResolver::ComputeSameTypeMask(Grid, FIntPoint(0, 0)), static_cast<uint8>(Expected | Bit(ETileNeighborSide::SE)));
			TestEq(*this, TEXT("Raised tile mask"), FTileNeighborResolver::ComputeSameTypeMask(Grid, FIntPoint(1, 0)), static_cast<uint8>(0));
		});

		It("returns an empty mask outside the grid", [this]()
		{
			const FTileGrid Grid = MakeGrid({ TEXT("GG"), TEXT("GG") });
			TestEq(*this, TEXT("Outside"), FTileNeighborResolver::ComputeSameTypeMask(Grid, FIntPoint(5, 5)), static_cast<uint8>(0));
		});
	});

	Describe("Watered visuals", [this]()
	{
		It("draws watered tiles with the watered material, or the dry one when there is none", [this]()
		{
			UMaterialInterface* Dry = UMaterial::GetDefaultMaterial(MD_Surface);
			UMaterialInterface* Wet = NewObject<UMaterialInstanceConstant>();

			FTileTypeVisualVariant Variant;
			Variant.Material = Dry;
			TestTrue(TEXT("No watered material, watered tile"), Variant.GetMaterial(true) == Dry);

			Variant.WateredMaterial = Wet;
			TestTrue(TEXT("Dry tile"), Variant.GetMaterial(false) == Dry);
			TestTrue(TEXT("Watered tile"), Variant.GetMaterial(true) == Wet);
		});

		It("keeps watered and dry tiles of the same piece in separate groups", [this]()
		{
			FTileVisualKey Dry = FTileNeighborResolver::Resolve(ETileType::Farmland, 0);
			FTileVisualKey Wet = Dry;
			Wet.bWatered = true;
			TestFalse(TEXT("Equal"), Dry == Wet);

			TSet<FTileVisualKey> Keys = { Dry, Wet };
			TestEq(*this, TEXT("Groups"), Keys.Num(), 2);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
