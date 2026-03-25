#include "TileDebugSnapshotsRegistry.h"
#include "TileDebugSnapshot.h"
#include "TileMutationHistory/TileMutationHistorySubsystem.h"

#include "Engine/World.h"

void UTileDebugSnapshotsRegistry::Initialize(UWorld* InWorld)
{
	if (!InWorld) return;

	auto MutationHistorySubsystem = InWorld->GetSubsystem<UTileMutationHistorySubsystem>();
	check(MutationHistorySubsystem);

	MutationHistorySubsystem->OnTileMutated.AddUObject(this, &UTileDebugSnapshotsRegistry::OnTileMutated);
}

const FTileDebugSnapshot* const UTileDebugSnapshotsRegistry::GetTileDebugSnapshot(const FIntPoint& InCoordinates) const
{
	return Snapshots.Find(InCoordinates);
}

void UTileDebugSnapshotsRegistry::OnTileMutated(TSharedPtr<const FTileMutationHistoryEntry> InHistoryEntry)
{
	auto Snapshot = Snapshots.FindOrAdd(InHistoryEntry->Coordinates);
	
	Snapshot.MutationCount++;
	
	Snapshot.LastMutationSource = InHistoryEntry->Source;
	Snapshot.LastMutationReason = InHistoryEntry->Reason;
	Snapshot.LastMutationPhase = InHistoryEntry->Phase;

	Snapshot.AllMutationSources.Add(InHistoryEntry->Source);
	Snapshot.AllMutationReasons.Add(InHistoryEntry->Reason);

	// TODO Mutation: Make FNames static in some utility class or enums for type safety
	//if (InHistoryEntry.Reason == "Smoothing") 
	if (InHistoryEntry->Source == ETileMutationSource::PostProcess) Snapshot.bHasEverBeenPostProcessed = true; 
}