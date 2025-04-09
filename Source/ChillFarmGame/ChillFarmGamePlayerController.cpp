#include "ChillFarmGamePlayerController.h"
#include "ChillFarmGameCharacter.h"

#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

#include "NiagaraFunctionLibrary.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"


DEFINE_LOG_CATEGORY(LogTemplateCharacter);


AChillFarmGamePlayerController::AChillFarmGamePlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	CachedDestination = FVector::ZeroVector;
	FollowTime = 0.f;
}

void AChillFarmGamePlayerController::BeginPlay()
{
	// Call the base class
	Super::BeginPlay();
}

void AChillFarmGamePlayerController::SetupInputComponent()
{
	// set up gameplay key bindings
	Super::SetupInputComponent();

	// Add Input Mapping Context
	if ( auto Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()) ) Subsystem->AddMappingContext( DefaultMappingContext, 0 );

	// Set up action bindings
	if ( auto EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent) )
	{
		// Setup mouse input events
		EnhancedInputComponent->BindAction( SetDestinationClickAction, ETriggerEvent::Started, this, &AChillFarmGamePlayerController::OnInputStarted );
		EnhancedInputComponent->BindAction( SetDestinationClickAction, ETriggerEvent::Triggered, this, &AChillFarmGamePlayerController::OnSetDestinationTriggered );
		EnhancedInputComponent->BindAction( SetDestinationClickAction, ETriggerEvent::Completed, this, &AChillFarmGamePlayerController::OnSetDestinationReleased );
		EnhancedInputComponent->BindAction( SetDestinationClickAction, ETriggerEvent::Canceled, this, &AChillFarmGamePlayerController::OnSetDestinationReleased );
	}
	else UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find EnhancedInputComponent"), *GetNameSafe(this));
}

void AChillFarmGamePlayerController::OnInputStarted()
{
	StopMovement();
}

// Triggered every frame when the input is held down
void AChillFarmGamePlayerController::OnSetDestinationTriggered()
{
	FollowTime += GetWorld()->GetDeltaSeconds();
	
	FHitResult Hit;
	auto bHitSuccessful = GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit);

	// If we hit a surface, cache the location
	if (bHitSuccessful) CachedDestination = Hit.Location;
	
	// Move towards mouse pointer or touch
	auto ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	FVector WorldDirection = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal();
	ControlledPawn->AddMovementInput(WorldDirection, 1.0, false);
}

void AChillFarmGamePlayerController::OnSetDestinationReleased()
{
	// If it was a short press
	if ( FollowTime <= ShortPressThreshold )
	{
		// We move there and spawn some particles
		UAIBlueprintHelperLibrary::SimpleMoveToLocation( this, CachedDestination );
		UNiagaraFunctionLibrary::SpawnSystemAtLocation( this, CursorPointEffect, CachedDestination, FRotator::ZeroRotator, FVector::OneVector, true, true, ENCPoolMethod::None, true );
	}

	FollowTime = 0.f;
}