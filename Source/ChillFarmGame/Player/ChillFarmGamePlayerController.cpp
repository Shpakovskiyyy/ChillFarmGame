#include "ChillFarmGamePlayerController.h"

#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

#include "NiagaraFunctionLibrary.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"

#include "TileMutation.h"
#include "TileWorldSubsystem.h"


DEFINE_LOG_CATEGORY(LogCFGPlayerController);


AChillFarmGamePlayerController::AChillFarmGamePlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	CachedDestination = FVector::ZeroVector;
	FollowTime = 0.f;
}

void AChillFarmGamePlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void AChillFarmGamePlayerController::SetupInputComponent()
{
	// Set up gameplay key bindings
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

		if (UseToolAction) EnhancedInputComponent->BindAction( UseToolAction, ETriggerEvent::Started, this, &AChillFarmGamePlayerController::OnUseTool );
		else UE_LOG(LogCFGPlayerController, Warning, TEXT("'%s' Has no UseToolAction, tiles cannot be tilled or watered"), *GetNameSafe(this));
	}
	else UE_LOG(LogCFGPlayerController, Error, TEXT("'%s' Failed to find EnhancedInputComponent"), *GetNameSafe(this));
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

	auto WorldDirection = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal();
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
	else UE_LOG(LogTemp, Warning, TEXT("No MoveTo activated because FollowTime > ShortPressThreshold: FollowTime - %f, ShortPressThreshold - %f"), FollowTime, ShortPressThreshold)

	FollowTime = 0.f;
}
void AChillFarmGamePlayerController::OnUseTool()
{
	UTileWorldSubsystem* TileWorld = GetWorld() ? GetWorld()->GetSubsystem<UTileWorldSubsystem>() : nullptr;
	const APawn* ControlledPawn = GetPawn();
	if (!TileWorld || !TileWorld->HasTiles() || !ControlledPawn) return;

	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit)) return;

	// The hit is on the tile's top surface; nudge it down so a hit exactly on a level edge still
	// lands inside the tile below the cursor.
	FIntPoint Target;
	const FTileData* Tile = TileWorld->FindTileAt(Hit.Location - FVector(0.0, 0.0, 1.0), &Target);
	if (!Tile) return;

	FIntPoint Standing;
	if (!TileWorld->FindTileAt(ControlledPawn->GetActorLocation(), &Standing)) return;

	const FIntPoint Delta = Target - Standing;
	if (FMath::Max(FMath::Abs(Delta.X), FMath::Abs(Delta.Y)) > UseToolRange)
	{
		UE_LOG(LogCFGPlayerController, Verbose, TEXT("Tile (%d, %d) is out of reach"), Target.X, Target.Y);
		return;
	}

	FTileMutation Mutation;
	FTileChangeContext Context;
	Context.Source = ETileMutationSource::Gameplay;
	Context.Instigator = this;
	if (!MakeToolMutation(*Tile, Mutation, Context.Step)) return;

	TileWorld->MutateTile(Target, Mutation, Context);
}

bool AChillFarmGamePlayerController::MakeToolMutation(const FTileData& InTile, FTileMutation& OutMutation, FName& OutStep) const
{
	// TODO(Yevhen): call the TileFarming rules here (card #18, step 1):
	//   if (TileFarming::CanTill(InTile))  { OutMutation = TileFarming::MakeTill(InTile);  OutStep = TEXT("Till");  return true; }
	//   if (TileFarming::CanWater(InTile)) { OutMutation = TileFarming::MakeWater(InTile); OutStep = TEXT("Water"); return true; }
	UE_LOG(LogCFGPlayerController, Display, TEXT("Use tool on a %s tile: no farming rules yet"), *UEnum::GetDisplayValueAsText(InTile.Type).ToString());
	return false;
}
