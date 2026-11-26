#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "ChillFarmGameCharacter.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
class USpringArmComponent;

UCLASS(Blueprintable)
class AChillFarmGameCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AChillFarmGameCharacter();

	// Opens the backpack flap: 0 = closed, 1 = fully open (e.g. ease it over time when the inventory opens).
	UFUNCTION(BlueprintCallable, Category = Backpack)
	void SetBackpackOpenAmount(float OpenAmount);

protected:
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	UCameraComponent* CameraComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	USpringArmComponent* SpringArm;

	// Worn on the body mesh's BackpackSocket; its own anim (e.g. the flap opening) runs independently.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Appearance)
	USkeletalMeshComponent* BackpackMesh;

};