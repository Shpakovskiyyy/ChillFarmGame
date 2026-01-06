#pragma once

#include "CoreMinimal.h"
#include "InteractableInterface.h"
#include "GameFramework/Actor.h"
#include "CropBase.generated.h"


UCLASS(Abstract)
class CHILLFARMGAME_API ACropBase : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	ACropBase();

	/* Begin IInteractableInterface */
public:
	virtual void Interact(const FInteractionPayloadBase* InPayload) override;
	virtual bool CanInteract(APlayerController* InPlayerController) const override;
	/* End IInteractableInterface */
	
protected:
	virtual void Tick(float DeltaTime) override;

protected:
	

protected:
	UPROPERTY(EditAnywhere, Category = "Crop Visuals")
	UStaticMeshComponent* CropMesh = nullptr;
	
};