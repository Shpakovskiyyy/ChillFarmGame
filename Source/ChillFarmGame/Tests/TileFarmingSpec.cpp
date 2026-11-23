// Specs for the farming rules (TileFarming) and for a gameplay edit going through the subsystem.
// No world needed: the rules only read tile data, the subsystem is created on its own.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TileTestUtils.h"
#include "TileFarming.h"
#include "TileWorldSubsystem.h"

using namespace TileTestUtils;

namespace TileFarmingSpecPrivate
{
	FTileData MakeTile(ETileType InType, ETileFlags InFlags = ETileFlags::None)
	{
		FTileData Tile;
		Tile.Type = InType;
		Tile.Biome = EBiomeType::Plains;
		Tile.Height = 1;
		Tile.Fertility = 42;
		Tile.Moisture = 37;
		Tile.Flags = static_cast<uint8>(InFlags);
		return Tile;
	}
}

using namespace TileFarmingSpecPrivate;


BEGIN_DEFINE_SPEC(FTileFarmingSpec, "ChillFarmGame.TileWorld.Farming", TileTestUtils::SpecFlags)
END_DEFINE_SPEC(FTileFarmingSpec)

void FTileFarmingSpec::Define()
{
	Describe("Tilling", [this]()
	{
		It("tills grass and dirt only", [this]()
		{
			TestTrue(TEXT("Grass"), TileFarming::CanTill(MakeTile(ETileType::Grass)));
			TestTrue(TEXT("Dirt"), TileFarming::CanTill(MakeTile(ETileType::Dirt)));
			TestFalse(TEXT("Sand"), TileFarming::CanTill(MakeTile(ETileType::Sand)));
			TestFalse(TEXT("Rock"), TileFarming::CanTill(MakeTile(ETileType::Rock)));
			TestFalse(TEXT("Water"), TileFarming::CanTill(MakeTile(ETileType::Water)));
			TestFalse(TEXT("Already farmland"), TileFarming::CanTill(MakeTile(ETileType::Farmland)));
			TestFalse(TEXT("Not generated"), TileFarming::CanTill(FTileData()));
		});

		It("does not till under something that stands on the tile, or on a slope", [this]()
		{
			TestFalse(TEXT("Occupied grass"), TileFarming::CanTill(MakeTile(ETileType::Grass, ETileFlags::Occupied)));

			FTileData Slope = MakeTile(ETileType::Grass);
			Slope.Slope = TileData::MakeEdgeSlope(FIntPoint(1, 0));
			TestFalse(TEXT("Grass slope"), TileFarming::CanTill(Slope));

			Slope.Slope = static_cast<uint8>(ETileCorner::SouthWest);
			TestFalse(TEXT("Grass corner"), TileFarming::CanTill(Slope));
		});

		It("turns the tile into farmland and keeps the rest of the land", [this]()
		{
			const FTileData Before = MakeTile(ETileType::Grass);
			FTileData After = Before;
			const ETileField Changed = TileFarming::MakeTill().ApplyTo(After);

			TestEq(*this, TEXT("Changed fields"), Changed, ETileField::Type);
			TestEq(*this, TEXT("Type"), After.Type, ETileType::Farmland);
			TestEq(*this, TEXT("Fertility"), After.Fertility, Before.Fertility);
			TestEq(*this, TEXT("Moisture"), After.Moisture, Before.Moisture);
			TestFalse(TEXT("Not watered by tilling"), After.HasFlag(ETileFlags::Watered));
		});
	});

	Describe("Watering", [this]()
	{
		It("waters dry farmland only", [this]()
		{
			TestTrue(TEXT("Dry farmland"), TileFarming::CanWater(MakeTile(ETileType::Farmland)));
			TestFalse(TEXT("Watered farmland"), TileFarming::CanWater(MakeTile(ETileType::Farmland, ETileFlags::Watered)));
			TestFalse(TEXT("Grass"), TileFarming::CanWater(MakeTile(ETileType::Grass)));
			TestFalse(TEXT("Dirt"), TileFarming::CanWater(MakeTile(ETileType::Dirt)));
		});

		It("sets the watered flag and leaves the natural moisture alone", [this]()
		{
			const FTileData Before = MakeTile(ETileType::Farmland, ETileFlags::Occupied);
			FTileData After = Before;
			const ETileField Changed = TileFarming::MakeWater().ApplyTo(After);

			TestEq(*this, TEXT("Changed fields"), Changed, ETileField::Flags);
			TestTrue(TEXT("Watered"), After.HasFlag(ETileFlags::Watered));
			TestTrue(TEXT("Other flags kept"), After.HasFlag(ETileFlags::Occupied));
			TestEq(*this, TEXT("Moisture"), After.Moisture, Before.Moisture);
		});
	});

	Describe("The use action", [this]()
	{
		It("tills, then waters, then does nothing", [this]()
		{
			FTileData Tile = MakeTile(ETileType::Dirt);
			FTileMutation Mutation;
			FName Step;

			TestTrue(TEXT("First use"), TileFarming::MakeToolMutation(Tile, Mutation, Step));
			TestEq(*this, TEXT("First step"), Step, TileFarming::TillStep);
			Mutation.ApplyTo(Tile);

			TestTrue(TEXT("Second use"), TileFarming::MakeToolMutation(Tile, Mutation, Step));
			TestEq(*this, TEXT("Second step"), Step, TileFarming::WaterStep);
			Mutation.ApplyTo(Tile);

			TestFalse(TEXT("Third use"), TileFarming::MakeToolMutation(Tile, Mutation, Step));
			TestEq(*this, TEXT("Type"), Tile.Type, ETileType::Farmland);
			TestTrue(TEXT("Watered"), Tile.HasFlag(ETileFlags::Watered));
		});

		It("does nothing on sand, rock or water", [this]()
		{
			FTileMutation Mutation;
			FName Step;
			for (const ETileType Type : { ETileType::Sand, ETileType::Rock, ETileType::Water })
			{
				TestFalse(*UEnum::GetValueAsString(Type), TileFarming::MakeToolMutation(MakeTile(Type), Mutation, Step));
			}
		});

		It("reaches listeners as a gameplay change with its step", [this]()
		{
			UTileWorldSubsystem* World = NewObject<UTileWorldSubsystem>();
			World->SetGrid(MakeGrid({ TEXT("GG"), TEXT("GW") }));

			TArray<FTileChange> Changes;
			TArray<FIntPoint> ChangedTiles;
			World->OnTileChanged.AddLambda([&Changes](const FTileChange& Change) { Changes.Add(Change); });
			World->OnTilesChanged.AddLambda([&ChangedTiles](TConstArrayView<FIntPoint> Tiles) { ChangedTiles.Append(Tiles.GetData(), Tiles.Num()); });

			const FIntPoint Target(1, 0);
			FTileMutation Mutation;
			FTileChangeContext Context;
			Context.Source = ETileMutationSource::Gameplay;
			TestTrue(TEXT("Grass can be used"), TileFarming::MakeToolMutation(*World->GetGrid().FindTile(Target), Mutation, Context.Step));
			World->MutateTile(Target, Mutation, Context);

			if (!TestEq(*this, TEXT("Change events"), Changes.Num(), 1)) return;
			TestEq(*this, TEXT("Source"), Changes[0].Context.Source, ETileMutationSource::Gameplay);
			TestEq(*this, TEXT("Step"), Changes[0].Context.Step, TileFarming::TillStep);
			TestEq(*this, TEXT("After"), Changes[0].After.Type, ETileType::Farmland);
			if (TestEq(*this, TEXT("Changed tile count"), ChangedTiles.Num(), 1)) TestEq(*this, TEXT("Changed tile"), ChangedTiles[0], Target);
			TestEq(*this, TEXT("Map"), DescribeTypes(World->GetGrid()), FString(TEXT("GF/GW")));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
