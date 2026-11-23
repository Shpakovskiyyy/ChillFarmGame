// Specs for the plain tile data types: FTileData, FTileMutation, FTileGridLayout, FTileGrid.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TileTestUtils.h"
#include "TileData.h"
#include "TileGrid.h"
#include "TileGridLayout.h"
#include "TileMutation.h"

using namespace TileTestUtils;


BEGIN_DEFINE_SPEC(FTileDataSpec, "ChillFarmGame.TileWorld.Data", TileTestUtils::SpecFlags)
END_DEFINE_SPEC(FTileDataSpec)

void FTileDataSpec::Define()
{
	Describe("FTileData", [this]()
	{
		It("starts as an empty, not generated tile", [this]()
		{
			const FTileData Tile;
			TestFalse(TEXT("Not generated"), Tile.IsGenerated());
			TestFalse(TEXT("Not water"), Tile.IsWater());
			TestEq(*this, TEXT("No biome"), Tile.Biome, EBiomeType::EBiomeType_MAX);
			TestEq(*this, TEXT("Height"), Tile.Height, static_cast<uint8>(0));
			TestEq(*this, TEXT("Fertility"), Tile.Fertility, static_cast<uint8>(0));
			TestEq(*this, TEXT("Moisture"), Tile.Moisture, static_cast<uint8>(0));
			TestFalse(TEXT("No flags"), Tile.HasFlag(ETileFlags::Occupied));
		});

		It("reports exactly the fields that differ", [this]()
		{
			const FTileData Base;
			const TPair<ETileField, FTileMutation> Cases[] = {
				{ ETileField::Type,      FTileMutation().SetType(ETileType::Rock) },
				{ ETileField::Biome,     FTileMutation().SetBiome(EBiomeType::Desert) },
				{ ETileField::Height,    FTileMutation().SetHeight(2) },
				{ ETileField::Fertility, FTileMutation().SetFertility(10) },
				{ ETileField::Moisture,  FTileMutation().SetMoisture(20) },
				{ ETileField::Flags,     FTileMutation().SetFlag(ETileFlags::Occupied, true) },
			};

			for (const auto& [Field, Mutation] : Cases)
			{
				FTileData Changed = Base;
				Mutation.ApplyTo(Changed);
				TestEq(*this, FString::Printf(TEXT("Diff of %s"), TileData::GetFieldName(Field)), TileData::Diff(Base, Changed), Field);
				TestTrue(TEXT("Tiles differ"), Base != Changed);
			}

			TestEq(*this, TEXT("Identical tiles"), TileData::Diff(Base, Base), ETileField::None);
		});

		It("names every single field", [this]()
		{
			TestEq(*this, TEXT("Type"), FString(TileData::GetFieldName(ETileField::Type)), FString(TEXT("Type")));
			TestEq(*this, TEXT("Biome"), FString(TileData::GetFieldName(ETileField::Biome)), FString(TEXT("Biome")));
			TestEq(*this, TEXT("Height"), FString(TileData::GetFieldName(ETileField::Height)), FString(TEXT("Height")));
			TestEq(*this, TEXT("Fertility"), FString(TileData::GetFieldName(ETileField::Fertility)), FString(TEXT("Fertility")));
			TestEq(*this, TEXT("Moisture"), FString(TileData::GetFieldName(ETileField::Moisture)), FString(TEXT("Moisture")));
			TestEq(*this, TEXT("Flags"), FString(TileData::GetFieldName(ETileField::Flags)), FString(TEXT("Flags")));
			TestEq(*this, TEXT("A combination is not a single field"), FString(TileData::GetFieldName(ETileField::Type | ETileField::Height)), FString(TEXT("None")));
		});

		It("converts 0..1 values to clamped percentages", [this]()
		{
			TestEq(*this, TEXT("Rounds"), TileData::ToPercent(0.456f), static_cast<uint8>(46));
			TestEq(*this, TEXT("Half"), TileData::ToPercent(0.5f), static_cast<uint8>(50));
			TestEq(*this, TEXT("Clamped below"), TileData::ToPercent(-1.f), static_cast<uint8>(0));
			TestEq(*this, TEXT("Clamped above"), TileData::ToPercent(2.f), static_cast<uint8>(100));
		});
	});

	Describe("FTileMutation", [this]()
	{
		It("is empty until something is set", [this]()
		{
			TestTrue(TEXT("Fresh mutation"), FTileMutation().IsEmpty());
			TestFalse(TEXT("With a value"), FTileMutation().SetHeight(1).IsEmpty());
			TestFalse(TEXT("With a flag"), FTileMutation().SetFlag(ETileFlags::Occupied, false).IsEmpty());

			FTileData Tile;
			TestEq(*this, TEXT("Applying nothing changes nothing"), FTileMutation().ApplyTo(Tile), ETileField::None);
		});

		It("writes only the fields that were set", [this]()
		{
			FTileData Tile;
			FTileMutation().SetType(ETileType::Grass).SetBiome(EBiomeType::Plains).SetHeight(1).SetFertility(30).SetMoisture(40).ApplyTo(Tile);

			const ETileField Changed = FTileMutation().SetType(ETileType::Sand).SetMoisture(90).ApplyTo(Tile);
			TestEq(*this, TEXT("Changed fields"), Changed, ETileField::Type | ETileField::Moisture);
			TestEq(*this, TEXT("Type"), Tile.Type, ETileType::Sand);
			TestEq(*this, TEXT("Moisture"), Tile.Moisture, static_cast<uint8>(90));
			TestEq(*this, TEXT("Biome kept"), Tile.Biome, EBiomeType::Plains);
			TestEq(*this, TEXT("Height kept"), Tile.Height, static_cast<uint8>(1));
			TestEq(*this, TEXT("Fertility kept"), Tile.Fertility, static_cast<uint8>(30));
		});

		It("does not count a value that is already there as a change", [this]()
		{
			FTileData Tile;
			const FTileMutation Mutation = FTileMutation().SetType(ETileType::Rock).SetHeight(3);
			TestEq(*this, TEXT("First apply"), Mutation.ApplyTo(Tile), ETileField::Type | ETileField::Height);
			TestEq(*this, TEXT("Second apply"), Mutation.ApplyTo(Tile), ETileField::None);
		});

		It("sets and clears flags, the last call for a flag wins", [this]()
		{
			FTileData Tile;
			TestEq(*this, TEXT("Set"), FTileMutation().SetFlag(ETileFlags::Occupied, true).ApplyTo(Tile), ETileField::Flags);
			TestTrue(TEXT("Is set"), Tile.HasFlag(ETileFlags::Occupied));

			TestEq(*this, TEXT("Set again"), FTileMutation().SetFlag(ETileFlags::Occupied, true).ApplyTo(Tile), ETileField::None);

			FTileMutation().SetFlag(ETileFlags::Occupied, true).SetFlag(ETileFlags::Occupied, false).ApplyTo(Tile);
			TestFalse(TEXT("Clear after set in one mutation"), Tile.HasFlag(ETileFlags::Occupied));

			FTileMutation().SetFlag(ETileFlags::Occupied, false).SetFlag(ETileFlags::Occupied, true).ApplyTo(Tile);
			TestTrue(TEXT("Set after clear in one mutation"), Tile.HasFlag(ETileFlags::Occupied));
		});
	});

	Describe("FTileGridLayout", [this]()
	{
		It("knows its size", [this]()
		{
			TestTrue(TEXT("Default is empty"), FTileGridLayout().IsEmpty());
			TestEq(*this, TEXT("Default has no tiles"), FTileGridLayout().NumTiles(), 0);
			TestEq(*this, TEXT("Tile count"), MakeLayout(7, 3).NumTiles(), 21);
			TestFalse(TEXT("Not empty"), MakeLayout(7, 3).IsEmpty());
		});

		It("round trips between coordinates and indices", [this]()
		{
			const FTileGridLayout Layout = MakeLayout(7, 3);
			for (int32 Index = 0; Index < Layout.NumTiles(); ++Index)
			{
				TestEq(*this, TEXT("Index round trip"), Layout.ToIndex(Layout.ToTile(Index)), Index);
			}
			TestEq(*this, TEXT("Row major"), Layout.ToIndex(FIntPoint(2, 1)), 9);
		});

		It("maps every tile centre back to its tile, on every level", [this]()
		{
			const FTileGridLayout Layout = MakeLayout(9, 5, FVector(1000.0, -500.0, 20.0));
			for (int32 Y = 0; Y < Layout.Size.Y; ++Y)
			{
				for (int32 X = 0; X < Layout.Size.X; ++X)
				{
					for (uint8 Level = 0; Level < 4; ++Level)
					{
						TestEq(*this, TEXT("Centre round trip"), Layout.WorldToTile(Layout.TileCenter(FIntPoint(X, Y), Level)), FIntPoint(X, Y));
					}
				}
			}
		});

		It("includes the lower edge of a tile and excludes the upper edge", [this]()
		{
			const FTileGridLayout Layout = MakeLayout(4, 4, FVector(1000.0, -500.0, 0.0));
			const FVector Origin = Layout.Origin;
			TestEq(*this, TEXT("Exactly on the origin"), Layout.WorldToTile(Origin), FIntPoint(0, 0));
			TestEq(*this, TEXT("Just below the next tile"), Layout.WorldToTile(Origin + FVector(99.99, 99.99, 0.0)), FIntPoint(0, 0));
			TestEq(*this, TEXT("Exactly on the next tile"), Layout.WorldToTile(Origin + FVector(100.0, 100.0, 0.0)), FIntPoint(1, 1));
		});

		It("rounds down for points before the origin", [this]()
		{
			const FTileGridLayout Layout = MakeLayout(4, 4);
			TestEq(*this, TEXT("Just before the origin"), Layout.WorldToTile(FVector(-0.01, -0.01, 0.0)), FIntPoint(-1, -1));
			TestEq(*this, TEXT("Further away"), Layout.WorldToTile(FVector(-150.0, 250.0, 0.0)), FIntPoint(-2, 2));
			TestFalse(TEXT("Negative tile is outside"), Layout.Contains(FIntPoint(-1, 0)));
			TestFalse(TEXT("Size is outside"), Layout.Contains(FIntPoint(4, 0)));
			TestTrue(TEXT("Last tile is inside"), Layout.Contains(FIntPoint(3, 3)));
		});

		It("places levels and bounds from the origin", [this]()
		{
			const FTileGridLayout Layout = MakeLayout(4, 2, FVector(10.0, 20.0, 30.0));
			TestEqual(TEXT("Level 0"), Layout.LevelToZ(0), 30.0);
			TestEqual(TEXT("Level 3"), Layout.LevelToZ(3), 30.0 + 3 * 50.0);
			TestEqual(TEXT("Centre of tile (1, 0)"), Layout.TileCenter(FIntPoint(1, 0), 2), FVector(160.0, 70.0, 130.0));

			const FBox Bounds = Layout.GetWorldBounds(2);
			TestEqual(TEXT("Bounds min"), Bounds.Min, FVector(10.0, 20.0, 30.0));
			TestEqual(TEXT("Bounds max"), Bounds.Max, FVector(410.0, 220.0, 130.0));
		});
	});

	Describe("FTileGrid", [this]()
	{
		It("starts with one not generated tile per layout tile", [this]()
		{
			const FTileGrid Grid(MakeLayout(5, 4));
			TestEq(*this, TEXT("Tile count"), Grid.GetTiles().Num(), 20);
			TestEq(*this, TEXT("Size"), Grid.GetSize(), FIntPoint(5, 4));
			for (const FTileData& Tile : Grid.GetTiles()) TestFalse(TEXT("Not generated"), Tile.IsGenerated());

			TestTrue(TEXT("Default grid is empty"), FTileGrid().IsEmpty());
		});

		It("finds tiles only inside the grid", [this]()
		{
			const FTileGrid Grid(MakeLayout(3, 3));
			TestNotNull(TEXT("Inside"), Grid.FindTile(FIntPoint(2, 2)));
			TestNull(TEXT("Negative"), Grid.FindTile(FIntPoint(-1, 0)));
			TestNull(TEXT("Past the end"), Grid.FindTile(FIntPoint(0, 3)));
		});

		It("applies mutations and reports the tile as it was", [this]()
		{
			FTileGrid Grid(MakeLayout(3, 3));
			Grid.ApplyMutation(FIntPoint(1, 1), FTileMutation().SetType(ETileType::Grass).SetHeight(2));

			FTileData Before;
			const ETileField Changed = Grid.ApplyMutation(FIntPoint(1, 1), FTileMutation().SetType(ETileType::Dirt), &Before);
			TestEq(*this, TEXT("Changed"), Changed, ETileField::Type);
			TestEq(*this, TEXT("Before type"), Before.Type, ETileType::Grass);
			TestEq(*this, TEXT("Before height"), Before.Height, static_cast<uint8>(2));
			TestEq(*this, TEXT("After type"), Grid.GetTile(FIntPoint(1, 1)).Type, ETileType::Dirt);
			TestFalse(TEXT("Neighbour untouched"), Grid.GetTile(FIntPoint(0, 1)).IsGenerated());
		});

		It("ignores mutations outside the grid and empty mutations", [this]()
		{
			FTileGrid Grid(MakeLayout(3, 3));
			TestEq(*this, TEXT("Outside"), Grid.ApplyMutation(FIntPoint(5, 5), FTileMutation().SetHeight(1)), ETileField::None);
			TestEq(*this, TEXT("Empty"), Grid.ApplyMutation(FIntPoint(1, 1), FTileMutation()), ETileField::None);
		});

		It("looks for neighbour types on the four sides only", [this]()
		{
			const FTileGrid Grid = MakeGrid({
				TEXT("WGG"),
				TEXT("GGG"),
				TEXT("GGR"),
			});
			TestTrue(TEXT("Water north of (0,1)"), Grid.HasNeighborOfType(FIntPoint(0, 1), ETileType::Water));
			TestTrue(TEXT("Water west of (1,0)"), Grid.HasNeighborOfType(FIntPoint(1, 0), ETileType::Water));
			TestFalse(TEXT("Diagonal water does not count"), Grid.HasNeighborOfType(FIntPoint(1, 1), ETileType::Water));
			TestTrue(TEXT("Rock south of (2,1)"), Grid.HasNeighborOfType(FIntPoint(2, 1), ETileType::Rock));
			TestFalse(TEXT("Corner tile, nothing past the edge"), Grid.HasNeighborOfType(FIntPoint(2, 2), ETileType::Rock));
		});

		It("lists the four direct neighbours as north, east, south, west", [this]()
		{
			const FIntPoint (&Offsets)[4] = FTileGrid::GetCardinalOffsets();
			TestEq(*this, TEXT("North"), Offsets[0], FIntPoint(0, -1));
			TestEq(*this, TEXT("East"), Offsets[1], FIntPoint(1, 0));
			TestEq(*this, TEXT("South"), Offsets[2], FIntPoint(0, 1));
			TestEq(*this, TEXT("West"), Offsets[3], FIntPoint(-1, 0));
		});

		It("is built from rows by the test helper as expected", [this]()
		{
			const FTileGrid Grid = MakeGrid({ TEXT("GW."), TEXT("SDR") });
			TestEq(*this, TEXT("Round trip"), DescribeTypes(Grid), FString(TEXT("GW./SDR")));
			TestEq(*this, TEXT("Water gets the water biome"), Grid.GetTile(FIntPoint(1, 0)).Biome, EBiomeType::Water);
			TestEq(*this, TEXT("Land gets the land biome"), Grid.GetTile(FIntPoint(0, 0)).Biome, EBiomeType::Plains);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
