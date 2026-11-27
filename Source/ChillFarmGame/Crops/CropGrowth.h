#pragma once

#include "CoreMinimal.h"
#include "CropGrowth.generated.h"

class UCropType;


// How far one planted crop has grown.
USTRUCT(BlueprintType)
struct FCropState
{
	GENERATED_BODY()

	// Index into UCropType::Stages.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crop")
	uint8 Stage = 0;

	// Watered nights spent in the current stage.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crop")
	uint8 DaysInStage = 0;

	bool operator==(const FCropState& Other) const { return Stage == Other.Stage && DaysInStage == Other.DaysInStage; }
};


// A crop standing on a tile: what it is and how far it has grown.
USTRUCT(BlueprintType)
struct FPlantedCrop
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crop")
	TObjectPtr<const UCropType> Type = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crop")
	FCropState State;
};


// The growth rules. Pure: they read a crop type and change one state, nothing else.
namespace CropGrowth
{
	// A type that can be planted: at least one stage to grow in and a ripe stage.
	CHILLFARMGAME_API bool IsPlantable(const UCropType* InType);

	// In its last stage, waiting for the harvest.
	CHILLFARMGAME_API bool IsRipe(const FCropState& InState, const UCropType& InType);

	// One night passes. A watered crop counts the day and moves to the next stage once it has spent
	// the stage's days in it; a dry crop waits. A ripe crop does not change.
	// Returns true when the stage changed (the crop looks different).
	CHILLFARMGAME_API bool GrowOvernight(FCropState& InOutState, const UCropType& InType, bool bInWatered);

	// What is left after a harvest. Returns false when the crop is gone; otherwise the state is
	// reset to the type's regrow stage.
	CHILLFARMGAME_API bool Regrow(FCropState& InOutState, const UCropType& InType);
}
