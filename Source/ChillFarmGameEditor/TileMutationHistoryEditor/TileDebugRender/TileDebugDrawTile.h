#pragma once

#include "CoreMinimal.h"

// One wire box drawn by the tile highlight.
struct FTileDebugDrawTile
{
	FVector Center = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;
	FColor Color = FColor::Yellow;
};
