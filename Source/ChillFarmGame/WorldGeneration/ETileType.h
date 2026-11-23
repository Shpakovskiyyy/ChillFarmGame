#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumRange.h"
#include "ETileType.generated.h"

UENUM(BlueprintType)
enum class ETileType : uint8
{
	// Natural ground
	Grass,
	Dirt,
	Sand,

	// Solid / Blocking
	Rock,

	// Liquid
	Water,

	// Made by the player: tilled soil that crops can be planted in. Reverts to Dirt when un-tilled.
	// Appended last so the values of the types above (and every saved asset) stay the same.
	Farmland,

	ETileType_MAX UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(ETileType, ETileType::ETileType_MAX)
