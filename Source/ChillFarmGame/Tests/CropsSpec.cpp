// Specs for the farming loop: crop growth rules, the crop subsystem and the day cycle.
// No world needed: subsystems are created on their own and bound to each other by hand.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TileTestUtils.h"
#include "CropGrowth.h"
#include "CropSubsystem.h"
#include "CropType.h"
#include "DayCycleSubsystem.h"
#include "TileFarming.h"
#include "TileWorldSubsystem.h"

using namespace TileTestUtils;

namespace CropsSpecPrivate
{
	// A crop with the given days per stage; the last entry is the ripe stage.
	UCropType* MakeCropType(std::initializer_list<int32> InStageDays, int32 InRegrowStage = INDEX_NONE, int32 InYield = 1)
	{
		UCropType* Type = NewObject<UCropType>();
		Type->Stages.Reset();
		for (const int32 Days : InStageDays)
		{
			FCropStage& Stage = Type->Stages.AddDefaulted_GetRef();
			Stage.Days = Days;
		}
		Type->RegrowStage = InRegrowStage;
		Type->HarvestYield = InYield;
		return Type;
	}

	// A tile world, its crops and its day cycle, wired the way a real world wires them.
	struct FFarm
	{
		UTileWorldSubsystem* Tiles = nullptr;
		UCropSubsystem* Crops = nullptr;
		UDayCycleSubsystem* Days = nullptr;

		explicit FFarm(std::initializer_list<const TCHAR*> InRows)
		{
			Tiles = NewObject<UTileWorldSubsystem>();
			Tiles->SetGrid(MakeGrid(InRows));
			Crops = NewObject<UCropSubsystem>();
			Crops->BindToTileWorld(*Tiles);
			Days = NewObject<UDayCycleSubsystem>();
			Days->Bind(*Tiles, *Crops);
		}

		void Water(const FIntPoint& InTile) const
		{
			Tiles->MutateTile(InTile, TileFarming::MakeWater(), FTileChangeContext());
		}

		bool IsWatered(const FIntPoint& InTile) const
		{
			return Tiles->GetGrid().GetTile(InTile).HasFlag(ETileFlags::Watered);
		}

		int32 StageAt(const FIntPoint& InTile) const
		{
			const FPlantedCrop* Crop = Crops->FindCrop(InTile);
			return Crop ? Crop->State.Stage : INDEX_NONE;
		}
	};
}

using namespace CropsSpecPrivate;


BEGIN_DEFINE_SPEC(FCropsSpec, "ChillFarmGame.Farming.Crops", TileTestUtils::SpecFlags)
END_DEFINE_SPEC(FCropsSpec)

void FCropsSpec::Define()
{
	Describe("Growth rules", [this]()
	{
		It("grows one watered night at a time through the stage days", [this]()
		{
			const UCropType* Type = MakeCropType({ 1, 2, 1 });
			FCropState State;
			TestEq(*this, TEXT("Days to ripe"), Type->GetDaysToRipe(), 3);

			TestTrue(TEXT("Night 1 changes the stage"), CropGrowth::GrowOvernight(State, *Type, true));
			TestEq(*this, TEXT("Stage after night 1"), State.Stage, static_cast<uint8>(1));

			TestFalse(TEXT("Night 2 only counts"), CropGrowth::GrowOvernight(State, *Type, true));
			TestEq(*this, TEXT("Days in stage"), State.DaysInStage, static_cast<uint8>(1));

			TestTrue(TEXT("Night 3 changes the stage"), CropGrowth::GrowOvernight(State, *Type, true));
			TestTrue(TEXT("Ripe"), CropGrowth::IsRipe(State, *Type));

			TestFalse(TEXT("A ripe crop stays as it is"), CropGrowth::GrowOvernight(State, *Type, true));
			TestEq(*this, TEXT("Stage stays ripe"), State.Stage, static_cast<uint8>(2));
		});

		It("waits on dry nights without losing progress", [this]()
		{
			const UCropType* Type = MakeCropType({ 2, 1 });
			FCropState State;
			CropGrowth::GrowOvernight(State, *Type, true);
			const FCropState Before = State;

			TestFalse(TEXT("Dry night"), CropGrowth::GrowOvernight(State, *Type, false));
			TestTrue(TEXT("Unchanged"), State == Before);
		});

		It("regrows from the regrow stage or is gone", [this]()
		{
			FCropState State;
			State.Stage = 2;
			State.DaysInStage = 3;
			TestFalse(TEXT("No regrow stage"), CropGrowth::Regrow(State, *MakeCropType({ 1, 1, 1 })));

			TestTrue(TEXT("Regrow stage 1"), CropGrowth::Regrow(State, *MakeCropType({ 1, 1, 1 }, 1)));
			TestEq(*this, TEXT("Back to stage"), State.Stage, static_cast<uint8>(1));
			TestEq(*this, TEXT("Days reset"), State.DaysInStage, static_cast<uint8>(0));
		});

		It("only plants types with a stage to grow in", [this]()
		{
			TestFalse(TEXT("None"), CropGrowth::IsPlantable(nullptr));
			TestFalse(TEXT("Only a ripe stage"), CropGrowth::IsPlantable(MakeCropType({ 1 })));
			TestTrue(TEXT("Two stages"), CropGrowth::IsPlantable(MakeCropType({ 1, 1 })));
			TestTrue(TEXT("The placeholder crop"), CropGrowth::IsPlantable(GetDefault<UCropType>()));
		});
	});

	Describe("Planting and harvesting", [this]()
	{
		It("plants once on farmland only", [this]()
		{
			const FFarm Farm({ TEXT("FGW") });
			const UCropType* Type = MakeCropType({ 1, 1 });

			TestTrue(TEXT("Farmland"), Farm.Crops->Plant(FIntPoint(0, 0), *Type));
			TestFalse(TEXT("Already planted"), Farm.Crops->Plant(FIntPoint(0, 0), *Type));
			TestFalse(TEXT("Grass"), Farm.Crops->Plant(FIntPoint(1, 0), *Type));
			TestFalse(TEXT("Water"), Farm.Crops->Plant(FIntPoint(2, 0), *Type));
			TestFalse(TEXT("Outside the map"), Farm.Crops->Plant(FIntPoint(5, 0), *Type));
			TestEq(*this, TEXT("Crops"), Farm.Crops->NumCrops(), 1);
			TestEq(*this, TEXT("Starts as a seed"), Farm.StageAt(FIntPoint(0, 0)), 0);
		});

		It("harvests a ripe crop only, and the crop is gone", [this]()
		{
			const FFarm Farm({ TEXT("F") });
			const FIntPoint Tile(0, 0);
			Farm.Crops->Plant(Tile, *MakeCropType({ 1, 1 }, INDEX_NONE, 3));

			int32 Harvested = 0;
			Farm.Crops->OnCropHarvested.AddLambda([&Harvested](const FIntPoint&, const UCropType&, int32 Yield) { Harvested += Yield; });

			TestEq(*this, TEXT("Not ripe yet"), Farm.Crops->Harvest(Tile), 0);

			Farm.Water(Tile);
			Farm.Days->StartNextDay();
			TestTrue(TEXT("Ripe"), Farm.Crops->CanHarvest(Tile));
			TestEq(*this, TEXT("Yield"), Farm.Crops->Harvest(Tile), 3);
			TestEq(*this, TEXT("Event"), Harvested, 3);
			TestFalse(TEXT("Gone"), Farm.Crops->HasCrop(Tile));
			TestEq(*this, TEXT("Tile is still farmland"), Farm.Tiles->GetGrid().GetTile(Tile).Type, ETileType::Farmland);
		});

		It("keeps a regrowing crop after the harvest", [this]()
		{
			const FFarm Farm({ TEXT("F") });
			const FIntPoint Tile(0, 0);
			Farm.Crops->Plant(Tile, *MakeCropType({ 1, 1, 1 }, 1));
			for (int32 Night = 0; Night < 2; ++Night)
			{
				Farm.Water(Tile);
				Farm.Days->StartNextDay();
			}

			TestEq(*this, TEXT("Harvest"), Farm.Crops->Harvest(Tile), 1);
			TestEq(*this, TEXT("Back to the regrow stage"), Farm.StageAt(Tile), 1);
		});

		It("drops a crop whose tile stops being farmland, and all crops with the map", [this]()
		{
			const FFarm Farm({ TEXT("FF") });
			const UCropType* Type = MakeCropType({ 1, 1 });
			Farm.Crops->Plant(FIntPoint(0, 0), *Type);
			Farm.Crops->Plant(FIntPoint(1, 0), *Type);

			TArray<FIntPoint> Changed;
			Farm.Crops->OnCropsChanged.AddLambda([&Changed](TConstArrayView<FIntPoint> Tiles) { Changed.Append(Tiles.GetData(), Tiles.Num()); });

			Farm.Water(FIntPoint(0, 0));
			TestEq(*this, TEXT("Watering keeps the crop"), Farm.Crops->NumCrops(), 2);

			Farm.Tiles->MutateTile(FIntPoint(0, 0), FTileMutation().SetType(ETileType::Grass), FTileChangeContext());
			TestFalse(TEXT("Removed with the farmland"), Farm.Crops->HasCrop(FIntPoint(0, 0)));
			TestTrue(TEXT("Announced"), Changed == TArray<FIntPoint>({ FIntPoint(0, 0) }));

			Farm.Tiles->SetGrid(MakeGrid({ TEXT("FF") }));
			TestEq(*this, TEXT("A new map has no crops"), Farm.Crops->NumCrops(), 0);
		});
	});

	Describe("The day cycle", [this]()
	{
		It("grows watered crops, then dries the land, then starts the next day", [this]()
		{
			const FFarm Farm({ TEXT("FFF") });
			const UCropType* Type = MakeCropType({ 1, 1, 1 });
			Farm.Crops->Plant(FIntPoint(0, 0), *Type);
			Farm.Crops->Plant(FIntPoint(1, 0), *Type);
			Farm.Water(FIntPoint(0, 0));
			Farm.Water(FIntPoint(2, 0));

			TArray<FTileChange> TileChanges;
			int32 VisualEvents = 0;
			int32 StartedDay = 0;
			Farm.Tiles->OnTileChanged.AddLambda([&TileChanges](const FTileChange& Change) { TileChanges.Add(Change); });
			Farm.Tiles->OnTilesChanged.AddLambda([&VisualEvents](TConstArrayView<FIntPoint>) { ++VisualEvents; });
			Farm.Days->OnDayStarted.AddLambda([&StartedDay](int32 Day) { StartedDay = Day; });

			TestEq(*this, TEXT("First day"), Farm.Days->GetDay(), 1);
			Farm.Days->StartNextDay();

			TestEq(*this, TEXT("Watered crop grew"), Farm.StageAt(FIntPoint(0, 0)), 1);
			TestEq(*this, TEXT("Dry crop waited"), Farm.StageAt(FIntPoint(1, 0)), 0);
			TestFalse(TEXT("Crop tile dried"), Farm.IsWatered(FIntPoint(0, 0)));
			TestFalse(TEXT("Empty tile dried"), Farm.IsWatered(FIntPoint(2, 0)));

			TestEq(*this, TEXT("Day"), Farm.Days->GetDay(), 2);
			TestEq(*this, TEXT("Day event"), StartedDay, 2);

			if (TestEq(*this, TEXT("One change per dried tile"), TileChanges.Num(), 2))
			{
				TestEq(*this, TEXT("Source"), TileChanges[0].Context.Source, ETileMutationSource::Simulation);
				TestEq(*this, TEXT("Step"), TileChanges[0].Context.Step, TileFarming::OvernightStep);
			}
			TestEq(*this, TEXT("Visuals redraw once"), VisualEvents, 1);
		});

		It("ripens a crop in its days to ripe when watered every day", [this]()
		{
			const FFarm Farm({ TEXT("F") });
			const FIntPoint Tile(0, 0);
			const UCropType* Type = GetDefault<UCropType>();
			Farm.Crops->Plant(Tile, *Type);

			for (int32 Night = 0; Night < Type->GetDaysToRipe(); ++Night)
			{
				TestFalse(*FString::Printf(TEXT("Not ripe before night %d"), Night + 1), Farm.Crops->CanHarvest(Tile));
				Farm.Water(Tile);
				Farm.Days->StartNextDay();
			}
			TestTrue(TEXT("Ripe"), Farm.Crops->CanHarvest(Tile));
		});

		It("runs a night without crops or tiles", [this]()
		{
			UDayCycleSubsystem* Days = NewObject<UDayCycleSubsystem>();
			Days->StartNextDay();
			TestEq(*this, TEXT("Day"), Days->GetDay(), 2);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
