#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "TileGenerationRule.h"
#include "TileNoiseContext.h"

#include "TilesGenerationRuleSet.generated.h"


// A set of rules that defines when the tile can be generated.
UCLASS(BlueprintType)
class CHILLFARMGAME_API UTilesGenerationRuleSet : public UDataAsset
{
	GENERATED_BODY()

public:
	// First rule whose noise range contains the value wins; Grass when none matches.
	ETileType ResolveTileType(const STileNoiseContext& Context) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	static float GetNoiseBySource(ETileNoiseSource NoiseSource, const STileNoiseContext& Context);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FTileGenerationRule> Rules;

};
