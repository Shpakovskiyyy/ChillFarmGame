#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "ChillFarmGameCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS(Blueprintable)
class AChillFarmGameCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AChillFarmGameCharacter();

protected:
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	UCameraComponent* CameraComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	USpringArmComponent* SpringArm;
	
};