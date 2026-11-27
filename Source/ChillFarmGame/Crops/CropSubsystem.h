#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "CropGrowth.h"

#include "CropSubsystem.generated.h"

class ACropVisuals;
class UCropType;
class UTileWorldSubsystem;
struct FTileChange;


// Tiles whose crop appeared, changed stage or went away.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCropsChanged, TConstArrayView<FIntPoint>);

// A crop was harvested: where, what, and how many items it gave.
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnCropHarvested, const FIntPoint&, const UCropType&, int32);


// The crops of a world, keyed by tile coordinate.
//
// Crops are sparse objects next to the tile map, not part of FTileData: most tiles never hold one,
// and a crop knows things a tile does not (its type, its age). The subsystem listens to the tile
// world: a crop whose tile stops being farmland is removed, and a new or dropped map removes them all.
// It never changes tiles itself; the day cycle reads the Watered flag through it and dries the land.
UCLASS()
class CHILLFARMGAME_API UCropSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// Follows this tile world. Done by Initialize in a real world; tests call it on their own.
	void BindToTileWorld(UTileWorldSubsystem& InTileWorld);

public:
	const FPlantedCrop* FindCrop(const FIntPoint& InTile) const { return Crops.Find(InTile); }
	bool HasCrop(const FIntPoint& InTile) const { return Crops.Contains(InTile); }
	int32 NumCrops() const { return Crops.Num(); }
	const TMap<FIntPoint, FPlantedCrop>& GetCrops() const { return Crops; }
	const UTileWorldSubsystem* GetTileWorld() const { return TileWorld.Get(); }

	// Empty farmland and a crop type with a ripe stage.
	bool CanPlant(const FIntPoint& InTile, const UCropType* InType) const;
	bool Plant(const FIntPoint& InTile, const UCropType& InType);

	bool CanHarvest(const FIntPoint& InTile) const;

	// Takes the ripe crop. It either goes back to its regrow stage or is removed.
	// Returns the number of items it gave, 0 when there was nothing to harvest.
	int32 Harvest(const FIntPoint& InTile);

	// One night for every crop: the ones on watered tiles grow. Reads the tiles, does not change them.
	// Returns how many crops changed stage.
	int32 GrowOvernight();

public:
	FOnCropsChanged OnCropsChanged;
	FOnCropHarvested OnCropHarvested;

private:
	void HandleTileChanged(const FTileChange& InChange);
	void HandleWorldReset();
	void UnbindFromTileWorld();
	void RemoveAll();

private:
	UPROPERTY()
	TMap<FIntPoint, FPlantedCrop> Crops;

	TWeakObjectPtr<UTileWorldSubsystem> TileWorld;

	UPROPERTY(Transient)
	TObjectPtr<ACropVisuals> Visuals = nullptr;
};
