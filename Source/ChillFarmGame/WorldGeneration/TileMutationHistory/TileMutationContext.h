#pragma once

#include "CoreMinimal.h"
#include "ETileMutationSource.h"
#include "TileMutationContext.generated.h"


USTRUCT()
struct CHILLFARMGAME_API FTileMutationContext
{
	GENERATED_BODY()
	
	
	static FORCEINLINE FTileMutationContext Make(const FIntPoint& InCoordinates, ETileMutationSource InSource, UBiomeTypeData* InBiomeTypeData, UObject* InInstigator, FName InReason, FName InPhase, uint32 InFlags = INDEX_NONE)
	{
		FTileMutationContext Context;
		Context.Coordinates = InCoordinates;
		Context.BiomeType = InBiomeTypeData;
		Context.Source = InSource;
		Context.Instigator = InInstigator;
		Context.Reason = InReason;
		Context.Phase = InPhase;
		Context.Flags = InFlags;
		return Context;
	}
	
	UPROPERTY()
	FIntPoint Coordinates = FIntPoint::ZeroValue;

	UPROPERTY()
	UBiomeTypeData* BiomeType = nullptr;
	
	UPROPERTY()
	ETileMutationSource Source = ETileMutationSource::Unknown;

	UPROPERTY()
	UObject* Instigator = nullptr;
	
	UPROPERTY()
	FName Reason = NAME_None;
	
	UPROPERTY()
	FName Phase = NAME_None;
	
	UPROPERTY()
	uint32 Flags = 0;
	
};