#include "ChillFarmGameGameMode.h"
#include "ChillFarmGamePlayerController.h"
#include "ChillFarmGameCharacter.h"

#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCFGGameMode, Log, Log)

AChillFarmGameGameMode::AChillFarmGameGameMode()
{
	// use our custom PlayerController class
	PlayerControllerClass = AChillFarmGamePlayerController::StaticClass();

	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ChillFarmGame/Blueprints/BP_CFG_Character"));
	if ( PlayerPawnBPClass.Class ) DefaultPawnClass = PlayerPawnBPClass.Class;
	else UE_LOG(LogCFGGameMode, Error, TEXT("'%s' failed to find BP_CFG_Character blueprint class"), *GetNameSafe(this))

	// set default controller to our Blueprinted controller
	static ConstructorHelpers::FClassFinder<APlayerController> PlayerControllerBPClass(TEXT("/Game/ChillFarmGame/Blueprints/BP_CFG_PlayerController"));
	if( PlayerControllerBPClass.Class ) PlayerControllerClass = PlayerControllerBPClass.Class;
	else UE_LOG(LogCFGGameMode, Error, TEXT("'%s' failed to find BP_CFG_PlayerController blueprint class"), *GetNameSafe(this))
}