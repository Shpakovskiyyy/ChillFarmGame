#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TileMutation.h"
#include "TileHistoryEditorSubsystem.generated.h"

class UTileWorldSubsystem;


// Records every change made to the tiles of an editor world, so the Tile Inspector can explain
// why a tile looks the way it does.
//
// Lives in the editor module: the runtime only broadcasts changes and never knows this exists, so
// a packaged game neither contains nor pays for history.
UCLASS()
class UTileHistoryEditorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	// Changes of one tile, oldest first. Empty when the tile was never changed.
	TConstArrayView<FTileChange> GetHistory(const FIntPoint& InTile) const;

	// Grows on every recorded change or clear. Lets widgets refresh only when something happened.
	uint32 GetVersion() const { return Version; }

private:
	void HandleTileChanged(const FTileChange& InChange);
	void HandleWorldCleared();

private:
	TWeakObjectPtr<UTileWorldSubsystem> TileWorld;
	TMap<FIntPoint, TArray<FTileChange>> History;
	uint32 Version = 0;
};
