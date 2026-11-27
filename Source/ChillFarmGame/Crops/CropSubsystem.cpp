#include "CropSubsystem.h"

#include "CropType.h"
#include "CropVisuals.h"
#include "TileWorldSubsystem.h"
#include "Engine/World.h"


void UCropSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UTileWorldSubsystem* Tiles = Collection.InitializeDependency<UTileWorldSubsystem>()) BindToTileWorld(*Tiles);
}

void UCropSubsystem::Deinitialize()
{
	UnbindFromTileWorld();
	Crops.Reset();
	Super::Deinitialize();
}

void UCropSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Crops only exist while playing, so their visuals are spawned here rather than placed in the level.
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Visuals = InWorld.SpawnActor<ACropVisuals>(Params);
	if (Visuals) Visuals->Bind(*this);
}

void UCropSubsystem::BindToTileWorld(UTileWorldSubsystem& InTileWorld)
{
	if (TileWorld.Get() == &InTileWorld) return;
	UnbindFromTileWorld();

	TileWorld = &InTileWorld;
	InTileWorld.OnTileChanged.AddUObject(this, &UCropSubsystem::HandleTileChanged);
	InTileWorld.OnWorldCleared.AddUObject(this, &UCropSubsystem::HandleWorldReset);
	InTileWorld.OnWorldRebuilt.AddUObject(this, &UCropSubsystem::HandleWorldReset);
}

void UCropSubsystem::UnbindFromTileWorld()
{
	if (UTileWorldSubsystem* Tiles = TileWorld.Get())
	{
		Tiles->OnTileChanged.RemoveAll(this);
		Tiles->OnWorldCleared.RemoveAll(this);
		Tiles->OnWorldRebuilt.RemoveAll(this);
	}
	TileWorld.Reset();
}

bool UCropSubsystem::CanPlant(const FIntPoint& InTile, const UCropType* InType) const
{
	const UTileWorldSubsystem* Tiles = TileWorld.Get();
	if (!Tiles || !CropGrowth::IsPlantable(InType) || Crops.Contains(InTile)) return false;

	const FTileData* Tile = Tiles->GetGrid().FindTile(InTile);
	return Tile && Tile->Type == ETileType::Farmland;
}

bool UCropSubsystem::Plant(const FIntPoint& InTile, const UCropType& InType)
{
	if (!CanPlant(InTile, &InType)) return false;

	FPlantedCrop& Crop = Crops.Add(InTile);
	Crop.Type = &InType;

	const FIntPoint Changed[] = { InTile };
	OnCropsChanged.Broadcast(TConstArrayView<FIntPoint>(Changed));
	return true;
}

bool UCropSubsystem::CanHarvest(const FIntPoint& InTile) const
{
	const FPlantedCrop* Crop = Crops.Find(InTile);
	return Crop && Crop->Type && CropGrowth::IsRipe(Crop->State, *Crop->Type);
}

int32 UCropSubsystem::Harvest(const FIntPoint& InTile)
{
	if (!CanHarvest(InTile)) return 0;

	FPlantedCrop& Crop = Crops[InTile];
	const UCropType& Type = *Crop.Type;
	const int32 Yield = FMath::Max(1, Type.HarvestYield);

	if (!CropGrowth::Regrow(Crop.State, Type)) Crops.Remove(InTile);

	OnCropHarvested.Broadcast(InTile, Type, Yield);
	const FIntPoint Changed[] = { InTile };
	OnCropsChanged.Broadcast(TConstArrayView<FIntPoint>(Changed));
	return Yield;
}

int32 UCropSubsystem::GrowOvernight()
{
	const UTileWorldSubsystem* Tiles = TileWorld.Get();
	if (!Tiles) return 0;

	TArray<FIntPoint> Changed;
	for (TPair<FIntPoint, FPlantedCrop>& Pair : Crops)
	{
		if (!Pair.Value.Type) continue;
		const FTileData* Tile = Tiles->GetGrid().FindTile(Pair.Key);
		const bool bWatered = Tile && Tile->HasFlag(ETileFlags::Watered);
		if (CropGrowth::GrowOvernight(Pair.Value.State, *Pair.Value.Type, bWatered)) Changed.Add(Pair.Key);
	}

	if (!Changed.IsEmpty()) OnCropsChanged.Broadcast(TConstArrayView<FIntPoint>(Changed));
	return Changed.Num();
}

void UCropSubsystem::HandleTileChanged(const FTileChange& InChange)
{
	// Only farmland holds crops: a tile turned back into something else (an editor tool, a building)
	// takes its crop with it.
	if (InChange.After.Type == ETileType::Farmland || !Crops.Contains(InChange.Coordinates)) return;

	Crops.Remove(InChange.Coordinates);
	const FIntPoint Changed[] = { InChange.Coordinates };
	OnCropsChanged.Broadcast(TConstArrayView<FIntPoint>(Changed));
}

void UCropSubsystem::HandleWorldReset()
{
	RemoveAll();
}

void UCropSubsystem::RemoveAll()
{
	if (Crops.IsEmpty()) return;

	TArray<FIntPoint> Changed;
	Crops.GetKeys(Changed);
	Crops.Reset();
	OnCropsChanged.Broadcast(TConstArrayView<FIntPoint>(Changed));
}
