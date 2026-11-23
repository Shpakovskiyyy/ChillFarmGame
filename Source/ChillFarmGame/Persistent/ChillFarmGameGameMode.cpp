#include "ChillFarmGameGameMode.h"

#include "ChillFarmGamePlayerController.h"
#include "ChillFarmGameCharacter.h"
#include "TileWorldGenerator.h"
#include "TileWorldSubsystem.h"
#include "Engine/World.h"
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
	else UE_LOG(LogCFGGameMode, Error, TEXT("'%s' Failed to find BP_CFG_Character blueprint class"), *GetNameSafe(this))

	// set default controller to our Blueprinted controller
	static ConstructorHelpers::FClassFinder<APlayerController> PlayerControllerBPClass(TEXT("/Game/ChillFarmGame/Blueprints/BP_CFG_PlayerController"));
	if( PlayerControllerBPClass.Class ) PlayerControllerClass = PlayerControllerBPClass.Class;
	else UE_LOG(LogCFGGameMode, Error, TEXT("'%s' Failed to find BP_CFG_PlayerController blueprint class"), *GetNameSafe(this))

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
	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	if (!Controller) return;

	// The PlayerStart is placed by hand, but the ground under it depends on the generated elevation.
	// Stand the pawn in the middle of the tile under the start, on its top: a raised tile under the
	// start, or a higher neighbour next to it, would otherwise block the spawn.
	const AActor* Start = FindPlayerStart(Controller);
	const UTileWorldSubsystem* TileWorld = GetWorld()->GetSubsystem<UTileWorldSubsystem>();
	const UClass* PawnClass = GetDefaultPawnClassForController(Controller);
	FIntPoint TileCoordinates;
	const FTileData* Tile = Start && TileWorld ? TileWorld->FindTileAt(Start->GetActorLocation(), &TileCoordinates) : nullptr;
	if (!Tile || !PawnClass)
	{
		RestartPlayer(Controller);
		return;
	}

	constexpr double SpawnClearance = 2.0;
	// The top of the tile, so a slope's surface is never above the spawn point.
	FVector Location = TileWorld->GetLayout().TileCenter(TileCoordinates, TileData::GetTopLevel(*Tile));
	Location.Z += PawnClass->GetDefaultObject<APawn>()->GetDefaultHalfHeight() + SpawnClearance;
	const FTransform SpawnTransform(FRotator(0.0, Start->GetActorRotation().Yaw, 0.0), Location);

	RestartPlayerAtTransform(Controller, SpawnTransform);
}