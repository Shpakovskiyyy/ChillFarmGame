#include "ChillFarmGameCharacter.h"

#include "BackpackAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Materials/Material.h"


static const auto s_CharacterCapsuleSize = FVector2D(42.f, 96.0f);

static const auto s_CharacterMovementRotationRate = FRotator(0.f, 640.f, 0.f);

static constexpr auto s_TargetSpringArmLength = 500.f;
static const auto s_SpringArmRelativeRotation = FRotator(-25.f, 0.f, 0.f);

static const FName s_BackpackSocketName(TEXT("BackpackSocket"));


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
	SpringArm->SetUsingAbsoluteRotation(false); // Should SpringArm rotate when the character does
	SpringArm->bEnableCameraLag = true;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraLagSpeed = 4.f;
	SpringArm->TargetArmLength = s_TargetSpringArmLength;
	SpringArm->SetRelativeRotation( s_SpringArmRelativeRotation );
	SpringArm->bDoCollisionTest = false; // Don't want to pull camera in when it collides with level

	
	CameraComponent = CreateDefaultSubobject<UCameraComponent>( TEXT("CameraComponent") );
	CameraComponent->SetupAttachment( SpringArm, USpringArmComponent::SocketName );
	CameraComponent->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// The socket's offset puts the mesh's origin where the pack sits on the back, so no relative transform here.
	BackpackMesh = CreateDefaultSubobject<USkeletalMeshComponent>( TEXT("BackpackMesh") );
	BackpackMesh->SetupAttachment( GetMesh(), s_BackpackSocketName );
	BackpackMesh->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	BackpackMesh->SetGenerateOverlapEvents( false );

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AChillFarmGameCharacter::SetBackpackOpenAmount(float OpenAmount)
{
	auto* BackpackAnim = Cast<UBackpackAnimInstance>( BackpackMesh->GetAnimInstance() );
		if ( !ensureMsgf(BackpackAnim, TEXT("BackpackMesh needs an anim class derived from UBackpackAnimInstance")) ) return;
	BackpackAnim->SetOpenAmount( OpenAmount );
}

void AChillFarmGameCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
}