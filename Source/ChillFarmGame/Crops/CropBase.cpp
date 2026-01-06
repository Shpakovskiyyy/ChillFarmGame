#include "CropBase.h"


ACropBase::ACropBase()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACropBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ACropBase::Interact(const FInteractionPayloadBase* InPayload)
{
	UE_LOG(LogTemp, Warning, TEXT("Interacted with Crop %s"), *GetName())
}

bool ACropBase::CanInteract(APlayerController* InPlayerController) const
{
	return true;
}