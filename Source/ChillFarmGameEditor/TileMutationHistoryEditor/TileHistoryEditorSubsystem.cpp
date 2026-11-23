#include "TileHistoryEditorSubsystem.h"

#include "TileWorldSubsystem.h"
#include "Engine/World.h"


bool UTileHistoryEditorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Outer ? Outer->GetWorld() : nullptr;
	return World && World->IsEditorWorld() && !World->IsGameWorld();
}

void UTileHistoryEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Make sure the tile world exists before subscribing, whatever order subsystems are created in.
	if (UTileWorldSubsystem* World = Collection.InitializeDependency<UTileWorldSubsystem>())
	{
		TileWorld = World;
		World->OnTileChanged.AddUObject(this, &UTileHistoryEditorSubsystem::HandleTileChanged);
		World->OnWorldCleared.AddUObject(this, &UTileHistoryEditorSubsystem::HandleWorldCleared);
	}
}

void UTileHistoryEditorSubsystem::Deinitialize()
{
	if (UTileWorldSubsystem* World = TileWorld.Get())
	{
		World->OnTileChanged.RemoveAll(this);
		World->OnWorldCleared.RemoveAll(this);
	}
	History.Reset();
	Super::Deinitialize();
}

TConstArrayView<FTileChange> UTileHistoryEditorSubsystem::GetHistory(const FIntPoint& InTile) const
{
	const TArray<FTileChange>* Found = History.Find(InTile);
	return Found ? TConstArrayView<FTileChange>(*Found) : TConstArrayView<FTileChange>();
}

void UTileHistoryEditorSubsystem::HandleTileChanged(const FTileChange& InChange)
{
	History.FindOrAdd(InChange.Coordinates).Add(InChange);
	++Version;
}

void UTileHistoryEditorSubsystem::HandleWorldCleared()
{
	History.Reset();
	++Version;
}
