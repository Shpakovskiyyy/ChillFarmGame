#include "ChillFarmGameGameMode.h"

#include "BlendSpaceAnalysis.h"
#include "ChillFarmGamePlayerController.h"
#include "ChillFarmGameCharacter.h"
#include "TileWorldGenerator.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"

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

	bStartPlayersAsSpectators = true;
}

void AChillFarmGameGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	auto Actor = UGameplayStatics::GetActorOfClass(this, ATileWorldGenerator::StaticClass());
	auto WorldGenerator = Cast<ATileWorldGenerator>(Actor);
		if (!ensureMsgf(WorldGenerator, TEXT("No TileWorldGenerator found on the map"))) return;
	WorldGenerator->GetOnTerrainGenerated().AddUObject(this, &AChillFarmGameGameMode::BeginPlayDeferred);
	WorldGenerator->GenerateWorld();
}

void AChillFarmGameGameMode::BeginPlayDeferred()
{
	RestartPlayer(GetWorld()->GetFirstPlayerController());
}