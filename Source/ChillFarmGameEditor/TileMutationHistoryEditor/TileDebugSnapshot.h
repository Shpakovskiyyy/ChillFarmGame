#pragma once

#include "CoreMinimal.h"
#include "TileMutationHistory/ETileMutationSource.h"
#include "TileDebugSnapshot.generated.h"

struct FTileData;


USTRUCT()
struct FTileDebugSnapshot
{
	GENERATED_BODY()

	uint32 MutationCount = INDEX_NONE;

	ETileMutationSource LastMutationSource = ETileMutationSource::Unknown;
	FName LastMutationReason = NAME_None;
	FName LastMutationPhase = NAME_None;

	TSet<ETileMutationSource> AllMutationSources;
	TSet<FName> AllMutationReasons;

	bool bHasEverBeenSmoothed = false;
	bool bHasEverBeenPostProcessed = false;
	
};