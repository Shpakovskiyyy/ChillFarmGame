#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "TileDebugSnapshot.h"
#include "TileMutationHistory/TileMutationHistoryEntry.h"

#include "TileDebugSnapshotsRegistry.generated.h"

struct FTileDebugSnapshot;


UCLASS()
class CHILLFARMGAMEEDITOR_API UTileDebugSnapshotsRegistry : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UWorld* InWorld);

public:
	const FTileDebugSnapshot* const GetTileDebugSnapshot(const FIntPoint& InCoordinates) const;

protected:
	void OnTileMutated(TSharedPtr<const FTileMutationHistoryEntry> InHistoryEntry);

protected:
	TMap<FIntPoint, FTileDebugSnapshot> Snapshots;

};