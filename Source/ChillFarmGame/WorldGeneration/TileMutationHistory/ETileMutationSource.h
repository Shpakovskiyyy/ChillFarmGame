#pragma once

#include "ETileMutationSource.generated.h"

UENUM()
enum class ETileMutationSource : uint8
{
	Unknown,

	WorldGeneration,
	PostProcess,
	Editor,
	Gameplay,
	Simulation
};