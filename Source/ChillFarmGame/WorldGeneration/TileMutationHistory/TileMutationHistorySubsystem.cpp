#include "TileMutationHistorySubsystem.h"

#include "BiomeTypeData.h"
#include "Editor.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogTileMutationHistorySubsystem, All, All)


void UTileMutationHistorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TileHistory.Reset();
	UE_LOG(LogTileMutationHistorySubsystem, Log, TEXT("TileMutationHistorySubsystem initialized"));
}

void UTileMutationHistorySubsystem::Deinitialize()
{
	Super::Deinitialize();
	TileHistory.Reset();
	UE_LOG(LogTileMutationHistorySubsystem, Log, TEXT("TileMutationHistorySubsystem deinitialized"));
}

bool UTileMutationHistorySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Outer && Outer->GetWorld() && Outer->GetWorld()->IsEditorWorld();
}

void UTileMutationHistorySubsystem::RecordMutation(const FTileMutationContext& InContext, const FTileData& InBefore, const FTileData& InAfter)
{
	auto Entry = MakeShared<FTileMutationHistoryEntry>();
	
	Entry->Source = InContext.Source;
	Entry->Instigator = InContext.Instigator;
	Entry->Reason = InContext.Reason;
	Entry->Phase = InContext.Phase;
	Entry->Flags = InContext.Flags;
	Entry->Before = InBefore;
	Entry->After = InAfter;
	
	TileHistory.FindOrAdd(InContext.Coordinates).Add(Entry);
	OnTileMutated.Broadcast(Entry);
}

const TArray<TSharedPtr<const FTileMutationHistoryEntry>>& UTileMutationHistorySubsystem::GetHistory(const FIntPoint& Coordinates) const
{
	return *TileHistory.Find(Coordinates);
}