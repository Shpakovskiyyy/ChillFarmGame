#include "DayCycleSubsystem.h"

#include "CropSubsystem.h"
#include "TileFarming.h"
#include "TileWorldSubsystem.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCFGDayCycle, Log, All);


namespace DayCyclePrivate
{
	TAutoConsoleVariable<float> CVarDayLengthSeconds(
		TEXT("CFG.Farming.DayLengthSeconds"),
		600.f,
		TEXT("Real seconds one game day lasts. 0 stops the clock: days then only change with CFG.Farming.NextDay."));

	FAutoConsoleCommandWithWorld NextDayCommand(
		TEXT("CFG.Farming.NextDay"),
		TEXT("Skips to the next morning: watered crops grow and the land dries."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* InWorld)
		{
			UDayCycleSubsystem* DayCycle = InWorld ? InWorld->GetSubsystem<UDayCycleSubsystem>() : nullptr;
			if (DayCycle) DayCycle->StartNextDay();
			else UE_LOG(LogCFGDayCycle, Warning, TEXT("No day cycle in this world: start Play first"));
		}));
}


bool UDayCycleSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Time only passes while playing.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDayCycleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UTileWorldSubsystem* Tiles = Collection.InitializeDependency<UTileWorldSubsystem>();
	UCropSubsystem* CropWorld = Collection.InitializeDependency<UCropSubsystem>();
	if (Tiles && CropWorld) Bind(*Tiles, *CropWorld);
}

void UDayCycleSubsystem::Bind(UTileWorldSubsystem& InTileWorld, UCropSubsystem& InCrops)
{
	TileWorld = &InTileWorld;
	Crops = &InCrops;
}

TStatId UDayCycleSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDayCycleSubsystem, STATGROUP_Tickables);
}

float UDayCycleSubsystem::GetTimeOfDay01() const
{
	const float Length = DayCyclePrivate::CVarDayLengthSeconds.GetValueOnGameThread();
	return Length > 0.f ? FMath::Clamp(SecondsIntoDay / Length, 0.f, 1.f) : 0.f;
}

void UDayCycleSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const UWorld* World = GetWorld();
	const float Length = DayCyclePrivate::CVarDayLengthSeconds.GetValueOnGameThread();
	if (!World || !World->HasBegunPlay() || World->IsPaused() || Length <= 0.f) return;

	SecondsIntoDay += DeltaTime;
	if (SecondsIntoDay >= Length) StartNextDay();
}

void UDayCycleSubsystem::StartNextDay()
{
	// Crops first: they grow from the water they got today, before the land dries.
	const int32 Grown = Crops.IsValid() ? Crops->GrowOvernight() : 0;

	int32 Dried = 0;
	if (UTileWorldSubsystem* Tiles = TileWorld.Get())
	{
		const FTileGrid& Grid = Tiles->GetGrid();
		TArray<FIntPoint> Watered;
		const TConstArrayView<FTileData> AllTiles = Grid.GetTiles();
		for (int32 Index = 0; Index < AllTiles.Num(); ++Index)
		{
			if (AllTiles[Index].HasFlag(ETileFlags::Watered)) Watered.Add(Grid.GetLayout().ToTile(Index));
		}

		FTileChangeContext Context;
		Context.Source = ETileMutationSource::Simulation;
		Context.Step = TileFarming::OvernightStep;
		Context.Instigator = this;
		Dried = Tiles->MutateTiles(Watered, TileFarming::MakeDry(), Context);
	}

	++Day;
	SecondsIntoDay = 0.f;
	UE_LOG(LogCFGDayCycle, Log, TEXT("Day %d: %d crops grew, %d tiles dried"), Day, Grown, Dried);
	OnDayStarted.Broadcast(Day);
}
