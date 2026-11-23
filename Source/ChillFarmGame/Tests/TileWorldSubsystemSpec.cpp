// Specs for UTileWorldSubsystem: owning the live grid, runtime edits and the events it broadcasts.
// The subsystem is created on its own: none of this needs a world.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TileTestUtils.h"
#include "TileWorldSubsystem.h"

using namespace TileTestUtils;

namespace TileWorldSubsystemSpecPrivate
{
	// Counts every event the subsystem broadcasts.
	struct FEventLog
	{
		TArray<FTileChange> Changes;
		TArray<FIntPoint> ChangedTiles;
		int32 Cleared = 0;
		int32 Rebuilt = 0;

		void Listen(UTileWorldSubsystem& InWorld)
		{
			InWorld.OnTileChanged.AddLambda([this](const FTileChange& Change) { Changes.Add(Change); });
			InWorld.OnTilesChanged.AddLambda([this](TConstArrayView<FIntPoint> Tiles) { ChangedTiles.Append(Tiles.GetData(), Tiles.Num()); });
			InWorld.OnWorldCleared.AddLambda([this]() { ++Cleared; });
			InWorld.OnWorldRebuilt.AddLambda([this]() { ++Rebuilt; });
		}
	};

	UTileWorldSubsystem* MakeWorldWithGrid()
	{
		UTileWorldSubsystem* World = NewObject<UTileWorldSubsystem>();
		World->SetGrid(MakeGrid({
			TEXT("GGG"),
			TEXT("GWG"),
		}));
		return World;
	}
}

using namespace TileWorldSubsystemSpecPrivate;


BEGIN_DEFINE_SPEC(FTileWorldSubsystemSpec, "ChillFarmGame.TileWorld.Subsystem", TileTestUtils::SpecFlags)
END_DEFINE_SPEC(FTileWorldSubsystemSpec)

void FTileWorldSubsystemSpec::Define()
{
	Describe("Owning the grid", [this]()
	{
		It("starts without tiles", [this]()
		{
			const UTileWorldSubsystem* World = NewObject<UTileWorldSubsystem>();
			TestFalse(TEXT("No tiles"), World->HasTiles());
			TestNull(TEXT("Nothing anywhere"), World->FindTileAt(FVector::ZeroVector));
		});

		It("takes a generated grid and announces it", [this]()
		{
			UTileWorldSubsystem* World = NewObject<UTileWorldSubsystem>();
			FEventLog Log;
			Log.Listen(*World);

			World->SetGrid(MakeGrid({ TEXT("GW") }));

			TestTrue(TEXT("Has tiles"), World->HasTiles());
			TestEq(*this, TEXT("Size"), World->GetGrid().GetSize(), FIntPoint(2, 1));
			TestEq(*this, TEXT("Rebuilt events"), Log.Rebuilt, 1);
			TestEq(*this, TEXT("No per tile events for a whole new map"), Log.Changes.Num(), 0);
		});

		It("drops the grid and announces it", [this]()
		{
			UTileWorldSubsystem* World = MakeWorldWithGrid();
			FEventLog Log;
			Log.Listen(*World);

			World->ClearWorld();

			TestFalse(TEXT("No tiles"), World->HasTiles());
			TestEq(*this, TEXT("Cleared events"), Log.Cleared, 1);
		});

		It("finds the tile under a world position through the layout", [this]()
		{
			UTileWorldSubsystem* World = NewObject<UTileWorldSubsystem>();
			FTileGrid Grid(MakeLayout(4, 4, FVector(1000.0, 2000.0, 0.0)));
			Grid.ApplyMutation(FIntPoint(2, 3), FTileMutation().SetType(ETileType::Rock));
			World->SetGrid(MoveTemp(Grid));

			FIntPoint Coordinates;
			const FTileData* Tile = World->FindTileAt(FVector(1250.0, 2350.0, 75.0), &Coordinates);
			if (!TestNotNull(TEXT("Found"), Tile)) return;
			TestEq(*this, TEXT("Coordinates"), Coordinates, FIntPoint(2, 3));
			TestEq(*this, TEXT("Tile"), Tile->Type, ETileType::Rock);

			FIntPoint Outside;
			TestNull(TEXT("Outside the map"), World->FindTileAt(FVector(0.0, 0.0, 0.0), &Outside));
			TestEq(*this, TEXT("Coordinates are still reported outside"), Outside, FIntPoint(-10, -20));
		});
	});

	Describe("MutateTile", [this]()
	{
		It("changes the tile and announces the change with its context", [this]()
		{
			UTileWorldSubsystem* World = MakeWorldWithGrid();
			FEventLog Log;
			Log.Listen(*World);

			UObject* Player = NewObject<UTileWorldSubsystem>();
			FTileChangeContext Context;
			Context.Source = ETileMutationSource::Gameplay;
			Context.Step = TEXT("Till");
			Context.Instigator = Player;

			const ETileField Changed = World->MutateTile(FIntPoint(0, 0), FTileMutation().SetType(ETileType::Dirt).SetFlag(ETileFlags::Occupied, true), Context);

			TestEq(*this, TEXT("Returned fields"), Changed, ETileField::Type | ETileField::Flags);
			TestEq(*this, TEXT("Tile changed"), World->GetGrid().GetTile(FIntPoint(0, 0)).Type, ETileType::Dirt);

			if (!TestEq(*this, TEXT("One change event"), Log.Changes.Num(), 1)) return;
			const FTileChange& Change = Log.Changes[0];
			TestEq(*this, TEXT("Coordinates"), Change.Coordinates, FIntPoint(0, 0));
			TestEq(*this, TEXT("Before"), Change.Before.Type, ETileType::Grass);
			TestEq(*this, TEXT("After"), Change.After.Type, ETileType::Dirt);
			TestTrue(TEXT("After has the flag"), Change.After.HasFlag(ETileFlags::Occupied));
			TestEq(*this, TEXT("Fields"), Change.ChangedFields, ETileField::Type | ETileField::Flags);
			TestEq(*this, TEXT("Source"), Change.Context.Source, ETileMutationSource::Gameplay);
			TestTrue(TEXT("Step"), Change.Context.Step == FName(TEXT("Till")));
			TestTrue(TEXT("Instigator"), Change.Context.Instigator.Get() == Player);

			TestTrue(TEXT("Visuals are told which tile changed"), Log.ChangedTiles == TArray<FIntPoint>({ FIntPoint(0, 0) }));
		});

		It("announces nothing when nothing changes", [this]()
		{
			UTileWorldSubsystem* World = MakeWorldWithGrid();
			FEventLog Log;
			Log.Listen(*World);

			TestEq(*this, TEXT("Same value"), World->MutateTile(FIntPoint(0, 0), FTileMutation().SetType(ETileType::Grass), FTileChangeContext()), ETileField::None);
			TestEq(*this, TEXT("Outside the map"), World->MutateTile(FIntPoint(9, 9), FTileMutation().SetType(ETileType::Dirt), FTileChangeContext()), ETileField::None);
			TestEq(*this, TEXT("Empty mutation"), World->MutateTile(FIntPoint(1, 0), FTileMutation(), FTileChangeContext()), ETileField::None);

			TestEq(*this, TEXT("Change events"), Log.Changes.Num(), 0);
			TestEq(*this, TEXT("Visual events"), Log.ChangedTiles.Num(), 0);
		});

		It("does nothing before a grid exists", [this]()
		{
			UTileWorldSubsystem* World = NewObject<UTileWorldSubsystem>();
			TestEq(*this, TEXT("No grid"), World->MutateTile(FIntPoint(0, 0), FTileMutation().SetHeight(1), FTileChangeContext()), ETileField::None);
		});
	});

	Describe("Change notifications", [this]()
	{
		It("knows whether anybody listens", [this]()
		{
			UTileWorldSubsystem* World = NewObject<UTileWorldSubsystem>();
			TestFalse(TEXT("Nobody yet"), World->WantsChangeNotifications());

			const FDelegateHandle Handle = World->OnTileChanged.AddLambda([](const FTileChange&) {});
			TestTrue(TEXT("A listener"), World->WantsChangeNotifications());

			World->OnTileChanged.Remove(Handle);
			TestFalse(TEXT("Listener removed"), World->WantsChangeNotifications());
		});

		It("forwards changes made on a grid it does not own yet without touching its own", [this]()
		{
			UTileWorldSubsystem* World = MakeWorldWithGrid();
			FEventLog Log;
			Log.Listen(*World);

			FTileChange External;
			External.Coordinates = FIntPoint(1, 0);
			External.After.Type = ETileType::Rock;
			External.ChangedFields = ETileField::Type;
			World->NotifyExternalChange(External);

			if (!TestEq(*this, TEXT("Forwarded"), Log.Changes.Num(), 1)) return;
			TestEq(*this, TEXT("Same change"), Log.Changes[0].Coordinates, FIntPoint(1, 0));
			TestEq(*this, TEXT("Own grid untouched"), World->GetGrid().GetTile(FIntPoint(1, 0)).Type, ETileType::Grass);
			TestEq(*this, TEXT("No visual update"), Log.ChangedTiles.Num(), 0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
