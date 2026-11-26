#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"

#include "BackpackAnimInstance.generated.h"


// Base class for the backpack's Animation Blueprint (ABP_Backpack).
// The AnimGraph reads OpenAmount: it scrubs the flap-opening animation and fades out the flap physics.
UCLASS()
class CHILLFARMGAME_API UBackpackAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// 0 = closed, 1 = fully open. Clamped.
	void SetOpenAmount(float InOpenAmount);
	float GetOpenAmount() const { return OpenAmount; }

protected:
	UPROPERTY(BlueprintReadOnly, Category = Backpack, meta = (ClampMin = "0", ClampMax = "1"))
	float OpenAmount = 0.f;
};
