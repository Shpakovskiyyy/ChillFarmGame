#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumRange.h"
#include "EBiomeType.generated.h"

UENUM(BlueprintType)
enum class EBiomeType : uint8
{
	Plains,
	Forest,
	Desert,
	Mountains,
	Water,

	EBiomeType_MAX UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EBiomeType, EBiomeType::EBiomeType_MAX)
