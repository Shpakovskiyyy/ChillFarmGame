#include "ChillFarmGameCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Materials/Material.h"


static const auto s_CharacterCapsuleSize = FVector2D(42.f, 96.0f);

static const auto s_CharacterMovementRotationRate = FRotator(0.f, 640.f, 0.f);

static constexpr auto s_TargetSpringArmLength = 800.f; 
static const auto s_SpringArmRelativeRotation = FRotator(-60.f, 0.f, 0.f);


AChillFarmGameCharacter::AChillFarmGameCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize( s_CharacterCapsuleSize.X, s_CharacterCapsuleSize.Y );

	// Don't rotate character to camera direction
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	auto CharacterMovementComp = GetCharacterMovement();
	CharacterMovementComp->bOrientRotationToMovement = true; // Rotate character to moving direction
	CharacterMovementComp->RotationRate = s_CharacterMovementRotationRate;
	CharacterMovementComp->bConstrainToPlane = true;
	CharacterMovementComp->bSnapToPlaneAtStart = true;
	
	SpringArm = CreateDefaultSubobject<USpringArmComponent>( TEXT("SpringArm") );
	SpringArm->SetupAttachment( RootComponent );
	SpringArm->SetUsingAbsoluteRotation(true); // Don't want arm to rotate when character does
	SpringArm->TargetArmLength = s_TargetSpringArmLength;
	SpringArm->SetRelativeRotation( s_SpringArmRelativeRotation );
	SpringArm->bDoCollisionTest = false; // Don't want to pull camera in when it collides with level

	
	CameraComponent = CreateDefaultSubobject<UCameraComponent>( TEXT("CameraComponent") );
	CameraComponent->SetupAttachment( SpringArm, USpringArmComponent::SocketName );
	CameraComponent->bUsePawnControlRotation = false; // Camera does not rotate relative to arm
	
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AChillFarmGameCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
}