// Specs for the farming rules (TileFarming) and for a gameplay edit going through the subsystem.
// No world needed: the rules only read tile data, the subsystem is created on its own.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TileTestUtils.h"
#include "TileFarming.h"
#include "TileWorldSubsystem.h"

using namespace TileTestUtils;

namespace TileFarming
{
	// Found by TestEq through argument-dependent lookup: EFarmAction is not a reflected enum.
	inline FString ToTestString(EFarmAction InAction)
	{
		switch (InAction)
		{
			case EFarmAction::Harvest: return TEXT("Harvest");
			case EFarmAction::Till:    return TEXT("Till");
			case EFarmAction::Plant:   return TEXT("Plant");
			case EFarmAction::Water:   return TEXT("Water");
			default:                   return TEXT("None");
		}
	}
}

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
		It("goes through the loop: till, plant, water, then waits, then harvests", [this]()
		{
			using namespace TileFarming;
			FTileData Tile = MakeTile(ETileType::Dirt);
			FFarmActionInput Input;
			Input.bHasSeed = true;

			TestEq(*this, TEXT("Dirt"), ChooseAction(Tile, Input), EFarmAction::Till);
			MakeTill().ApplyTo(Tile);

			TestEq(*this, TEXT("Empty farmland"), ChooseAction(Tile, Input), EFarmAction::Plant);
			Input.bHasCrop = true;

			TestEq(*this, TEXT("Dry crop"), ChooseAction(Tile, Input), EFarmAction::Water);
			MakeWater().ApplyTo(Tile);

			TestEq(*this, TEXT("Watered crop"), ChooseAction(Tile, Input), EFarmAction::None);

			Input.bCropRipe = true;
			TestEq(*this, TEXT("Ripe crop on watered land"), ChooseAction(Tile, Input), EFarmAction::Harvest);
			MakeDry().ApplyTo(Tile);
			TestEq(*this, TEXT("Ripe crop on dry land"), ChooseAction(Tile, Input), EFarmAction::Harvest);
		});

		It("waters empty farmland when there is no seed", [this]()
		{
			using namespace TileFarming;
			TestEq(*this, TEXT("No seed"), ChooseAction(MakeTile(ETileType::Farmland), FFarmActionInput()), EFarmAction::Water);
			TestEq(*this, TEXT("No seed, watered"), ChooseAction(MakeTile(ETileType::Farmland, ETileFlags::Watered), FFarmActionInput()), EFarmAction::None);

			FFarmActionInput Seed;
			Seed.bHasSeed = true;
			TestEq(*this, TEXT("Seed on watered farmland"), ChooseAction(MakeTile(ETileType::Farmland, ETileFlags::Watered), Seed), EFarmAction::Plant);
		});

		It("does nothing on sand, rock or water", [this]()
		{
			using namespace TileFarming;
			FFarmActionInput Input;
			Input.bHasSeed = true;
			for (const ETileType Type : { ETileType::Sand, ETileType::Rock, ETileType::Water })
			{
				TestEq(*this, *UEnum::GetValueAsString(Type), ChooseAction(MakeTile(Type), Input), EFarmAction::None);
			}
		});

		It("changes tiles only for tilling and watering", [this]()
		{
			using namespace TileFarming;
			FTileMutation Mutation;
			FName Step;
			TestTrue(TEXT("Till"), MakeActionMutation(EFarmAction::Till, Mutation, Step));
			TestEq(*this, TEXT("Till step"), Step, TillStep);
			TestTrue(TEXT("Water"), MakeActionMutation(EFarmAction::Water, Mutation, Step));
			TestEq(*this, TEXT("Water step"), Step, WaterStep);
			TestFalse(TEXT("Plant"), MakeActionMutation(EFarmAction::Plant, Mutation, Step));
			TestFalse(TEXT("Harvest"), MakeActionMutation(EFarmAction::Harvest, Mutation, Step));
			TestFalse(TEXT("None"), MakeActionMutation(EFarmAction::None, Mutation, Step));
		});

		It("dries watered land and keeps other flags", [this]()
		{
			FTileData Tile = MakeTile(ETileType::Farmland, ETileFlags::Watered | ETileFlags::Occupied);
			TestEq(*this, TEXT("Changed"), TileFarming::MakeDry().ApplyTo(Tile), ETileField::Flags);
			TestFalse(TEXT("Dry"), Tile.HasFlag(ETileFlags::Watered));
			TestTrue(TEXT("Still occupied"), Tile.HasFlag(ETileFlags::Occupied));
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
			const TileFarming::EFarmAction Action = TileFarming::ChooseAction(*World->GetGrid().FindTile(Target), TileFarming::FFarmActionInput());
			TestTrue(TEXT("Grass can be used"), TileFarming::MakeActionMutation(Action, Mutation, Context.Step));
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
