#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "ChillFarmGamePlayerController.generated.h"

class UNiagaraSystem;
class UInputMappingContext;
class UInputAction;
class UCropType;

DECLARE_LOG_CATEGORY_EXTERN(LogCFGPlayerController, Log, All);

UCLASS()
class AChillFarmGamePlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	AChillFarmGamePlayerController();

	/** Time Threshold to know if it was a short press */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	float ShortPressThreshold = 0.3f;

	/** FX Class that we will spawn when clicking */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UNiagaraSystem* CursorPointEffect;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;
	
	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* SetDestinationClickAction;

	/** Uses the tool on the tile under the cursor: harvests a ripe crop, tills grass or dirt, plants on empty farmland, waters dry farmland. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* UseToolAction;

	/** How far from the character's tile the tool reaches, in tiles (diagonals count as one). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Farming", meta = (ClampMin = "0"))
	int32 UseToolRange = 2;

	/** What the tool plants on empty farmland. Empty: the placeholder crop (UCropType's defaults). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Farming")
	TObjectPtr<UCropType> SeedCrop = nullptr;

protected:
	/** True if the controlled character should navigate to the mouse cursor. */
	uint32 bMoveToMouseCursor : 1;

	virtual void BeginPlay();
	virtual void SetupInputComponent() override;

	/** Input handlers for SetDestination action. */
	void OnInputStarted();
	void OnSetDestinationTriggered();
	void OnSetDestinationReleased();

	/** Input handler for UseTool: finds the tile under the cursor and applies the tool to it. */
	void OnUseTool();

	const UCropType* GetSeedCrop() const;

private:
	FVector CachedDestination;
	float FollowTime; // For how long it has been pressed
	
};