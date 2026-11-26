#include "BackpackAnimInstance.h"


void UBackpackAnimInstance::SetOpenAmount(float InOpenAmount)
{
	OpenAmount = FMath::Clamp(InOpenAmount, 0.f, 1.f);
}
