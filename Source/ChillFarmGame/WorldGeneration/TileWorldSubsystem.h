#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "TileGrid.h"
#include "TileMutation.h"

#include "TileWorldSubsystem.generated.h"


// Every applied change, one tile at a time. Includes the changes generation passes make.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTileChanged, const FTileChange&);

// Tiles of the live grid that changed, after the change was applied. Used to update visuals.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTilesChanged, TConstArrayView<FIntPoint>);


// Owns the tile map of a world and is the only way to change it.
//
// Generation builds a grid on its own and hands it over with SetGrid; gameplay and tools change
// single tiles with MutateTile. Anything that wants to react (visuals, history, a future save
// system, crops) listens to the events below instead of being called by whoever made the change.
UCLASS()
class CHILLFARMGAME_API UTileWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

public:
	const FTileGrid& GetGrid() const { return Grid; }
	const FTileGridLayout& GetLayout() const { return Grid.GetLayout(); }
	bool HasTiles() const { return !Grid.IsEmpty(); }

	// nullptr when the point is outside the map.
	const FTileData* FindTileAt(const FVector& InWorldLocation, FIntPoint* OutCoordinates = nullptr) const;

public:
	// Drops the current map. Called before generation starts so listeners can forget old state.
	void ClearWorld();

	// Takes over a fully generated map.
	void SetGrid(FTileGrid&& InGrid);

	// Changes one tile of the live map. Returns the fields that changed.
	ETileField MutateTile(const FIntPoint& InTile, const FTileMutation& InMutation, const FTileChangeContext& InContext);

	// Applies the same mutation to many tiles. Every tile still gets its own OnTileChanged, but
	// OnTilesChanged fires once with all the tiles that changed, so visuals redraw each chunk once.
	// Returns how many tiles changed.
	int32 MutateTiles(TConstArrayView<FIntPoint> InTiles, const FTileMutation& InMutation, const FTileChangeContext& InContext);

	// Reports a change that was applied to a grid this subsystem does not own yet (a grid that is being
	// generated). Lets tools record generation steps without generation knowing about them.
	void NotifyExternalChange(const FTileChange& InChange) const;

	// True when somebody listens to individual changes. Generation skips building FTileChange otherwise.
	bool WantsChangeNotifications() const { return OnTileChanged.IsBound(); }

public:
	FOnTileChanged OnTileChanged;
	FOnTilesChanged OnTilesChanged;

	// The map was dropped (generation is about to start or the world was cleaned).
	FSimpleMulticastDelegate OnWorldCleared;

	// A complete new map was set.
	FSimpleMulticastDelegate OnWorldRebuilt;

private:
	FTileGrid Grid;
};
