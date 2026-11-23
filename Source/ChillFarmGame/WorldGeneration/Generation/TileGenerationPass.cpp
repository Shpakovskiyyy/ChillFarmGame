#include "Generation/TileGenerationPass.h"
#include "Generation/TileGenerationContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogTileGeneration, Log, All);


bool TileGeneration::RunPasses(TConstArrayView<const UTileGenerationPass*> InPasses, FTileGenerationContext& InOutContext)
{
	bool bSetupValid = true;
	for (const UTileGenerationPass* Pass : InPasses)
	{
		if (!Pass || !Pass->bEnabled) continue;

		const FString SetupError = Pass->GetSetupError(InOutContext);
		if (!SetupError.IsEmpty())
		{
			UE_LOG(LogTileGeneration, Error, TEXT("Pass '%s' cannot run: %s"), *Pass->GetStepName().ToString(), *SetupError);
			bSetupValid = false;
		}
	}
	if (!bSetupValid) return false;

	for (const UTileGenerationPass* Pass : InPasses)
	{
		if (!Pass || !Pass->bEnabled) continue;

		InOutContext.CurrentStep = Pass->GetStepName();
		Pass->Execute(InOutContext);
	}

	InOutContext.CurrentStep = NAME_None;
	return true;
}
