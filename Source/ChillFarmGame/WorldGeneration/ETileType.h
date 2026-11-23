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

	ETileType_MAX UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(ETileType, ETileType::ETileType_MAX)
