#pragma once

#include "CoreMinimal.h"
#include "TileNoiseContext.generated.h"

// Which noise value a generation rule looks at.
UENUM()
enum class ETileNoiseSource : uint8
{
	Height,
	Moisture,
	Secondary,
	HeightPlusSecondary,
};

// The noise values of one tile, all in 0..1. Secondary is the high frequency detail noise.
struct STileNoiseContext
{
	float Height;
	float Moisture;
	float Secondary;
};
