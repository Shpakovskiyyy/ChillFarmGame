#pragma once

#include "CoreMinimal.h"
#include "TileData.h"
#include "ETileMutationSource.h"
#include "TileMutationHistoryEntry.generated.h"

class UBiomeTypeData;


USTRUCT()
struct CHILLFARMGAME_API FTileMutationHistoryEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FIntPoint Coordinates = FIntPoint::ZeroValue;
	
	UPROPERTY()
	ETileMutationSource Source = ETileMutationSource::Unknown;
	
	UPROPERTY()
	TWeakObjectPtr<UObject> Instigator = nullptr;
	
	UPROPERTY()
	FName Reason = NAME_None;
	
	UPROPERTY()
	FName Phase = NAME_None;
	
	UPROPERTY()
	uint32 Flags = 0;

	UPROPERTY()
	FTileData Before = {};
	
	UPROPERTY()
	FTileData After = {};
	
};