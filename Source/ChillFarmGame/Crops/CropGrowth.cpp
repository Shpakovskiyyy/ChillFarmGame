#include "CropGrowth.h"

#include "CropType.h"


namespace CropGrowth
{
	bool IsPlantable(const UCropType* InType)
	{
		return InType && InType->NumStages() >= 2 && InType->NumStages() <= MAX_uint8;
	}

	bool IsRipe(const FCropState& InState, const UCropType& InType)
	{
		return InState.Stage >= InType.GetLastStage();
	}

	bool GrowOvernight(FCropState& InOutState, const UCropType& InType, bool bInWatered)
	{
		if (!bInWatered || IsRipe(InOutState, InType)) return false;

		++InOutState.DaysInStage;
		if (InOutState.DaysInStage < FMath::Max(1, InType.Stages[InOutState.Stage].Days)) return false;

		++InOutState.Stage;
		InOutState.DaysInStage = 0;
		return true;
	}

	bool Regrow(FCropState& InOutState, const UCropType& InType)
	{
		if (InType.RegrowStage == INDEX_NONE || InType.RegrowStage >= InType.GetLastStage()) return false;

		InOutState.Stage = static_cast<uint8>(InType.RegrowStage);
		InOutState.DaysInStage = 0;
		return true;
	}
}
