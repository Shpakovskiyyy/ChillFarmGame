#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "TileData.h"
#include "TileMutationHistory/TileMutationHistoryEntry.h"
#include "TileMutationHistory/TileMutationContext.h"

#include "TileMutationHistorySubsystem.generated.h"


DECLARE_MULTICAST_DELEGATE_OneParam(FOnTileMutated, TSharedPtr<const FTileMutationHistoryEntry>)

UCLASS()
class CHILLFARMGAME_API UTileMutationHistorySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

public:
	void RecordMutation(const FTileMutationContext& InContext, const FTileData& InBefore, const FTileData& InAfter);

public:
	const TArray<TSharedPtr<const FTileMutationHistoryEntry>>& GetHistory(const FIntPoint& Coordinates) const;

public:
	FOnTileMutated OnTileMutated;

private:
	TMap<FIntPoint, TArray<TSharedPtr<const FTileMutationHistoryEntry>>> TileHistory;
	
};