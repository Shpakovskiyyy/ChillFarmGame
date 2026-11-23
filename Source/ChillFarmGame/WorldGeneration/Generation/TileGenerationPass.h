#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TileGenerationPass.generated.h"

struct FTileGenerationContext;


// One step of world generation.
//
// The generator runs its passes in order. Each pass reads what earlier passes produced and adds
// its own layer: terrain, biomes, tile types, terraces, shores, and later roads, resources or farm
// site scoring. Passes are instanced objects, so their settings are edited inline on the generator
// and a new step is a new subclass rather than another branch in one big function.
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class CHILLFARMGAME_API UTileGenerationPass : public UObject
{
	GENERATED_BODY()

public:
	// Runs the pass. Must only depend on the context (and the pass settings) to stay deterministic.
	virtual void Execute(FTileGenerationContext& Context) const PURE_VIRTUAL(UTileGenerationPass::Execute, );

	// Name shown in tile history.
	virtual FName GetStepName() const { return GetClass()->GetFName(); }

	// Explains why the pass cannot run with this context; empty when it can.
	virtual FString GetSetupError(const FTileGenerationContext& Context) const { return FString(); }

public:
	UPROPERTY(EditAnywhere, Category = "Pass")
	bool bEnabled = true;
};


namespace TileGeneration
{
	// Runs the passes in order on the context's grid. Null and disabled passes are skipped.
	//
	// Every pass is checked before any of them runs, so a broken setup never leaves a half generated
	// grid: on a setup error nothing is changed and false is returned (the errors are logged).
	// Needs no world, which is what the generator, tests and a future background generation use.
	CHILLFARMGAME_API bool RunPasses(TConstArrayView<const UTileGenerationPass*> InPasses, FTileGenerationContext& InOutContext);
}
