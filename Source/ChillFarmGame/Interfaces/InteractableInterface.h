#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"


USTRUCT(BlueprintType)
struct FInteractionPayloadBase
{
	GENERATED_BODY()
};


UINTERFACE()
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class CHILLFARMGAME_API IInteractableInterface
{
	GENERATED_BODY()
	
public:
	virtual void Interact(const FInteractionPayloadBase* InPayload) = 0;

public:
	virtual bool CanInteract(APlayerController* InPlayerController) const = 0;
	
};