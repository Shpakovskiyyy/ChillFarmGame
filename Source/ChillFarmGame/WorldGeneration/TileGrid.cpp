#include "TileGrid.h"

#include "NavigationSystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"


ATileGrid::ATileGrid()
{
	PrimaryActorTick.bCanEverTick = false;
	
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	for (auto TileType : TEnumRange<ETileType>())
	{
		auto ComponentName = UEnum::GetValueAsString(TileType) + "InstancedStaticMeshComponent";
		Tiles.Add(TileType);
		
		auto HISMC = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(FName(ComponentName));
		HISMC->SetupAttachment(RootComponent);
		
		HISMC->SetMobility(EComponentMobility::Static);
		HISMC->SetCollisionEnabled(ECollisionEnabled::Type::QueryAndPhysics);
		HISMC->SetCollisionResponseToAllChannels(ECR_Block);
		HISMC->bUseAsOccluder = false;
		HISMC->bCastDynamicShadow = false;
		HISMC->bCastStaticShadow = false;
		HISMC->SetMobility(EComponentMobility::Static);
		HISMC->SetCollisionProfileName(TEXT("BlockAll"));
		HISMC->SetCanEverAffectNavigation(true);
		HISMC->bNavigationRelevant = true;

		Tiles[TileType] = HISMC;
	}
}

void ATileGrid::SetupMeshComponents()
{
	for (auto TileType : TEnumRange<ETileType>())
	{
		auto Mesh = LoadObject<UStaticMesh>(this, *MeshPath);
		if (!ensureMsgf(Mesh, TEXT("No Mesh found by reference %s"), *MeshPath)) continue;
		if (!ensureMsgf(TileTypeMaterials.Contains(TileType), TEXT("No TileType binding in TileTypeMaterials map for %s"), *UEnum::GetValueAsString(TileType))) continue;
		auto TileHISMC = Tiles[TileType];
		TileHISMC->SetStaticMesh(Mesh);
		TileHISMC->SetMaterial(0, TileTypeMaterials[TileType]);
	}
}

void ATileGrid::AddInstances(ETileType InTileType, const TArray<FTransform>& InTransform)
{
	if (!ensureMsgf(Tiles.Contains(InTileType), TEXT("No TileType binding in Tiles map for %s"), *UEnum::GetValueAsString(InTileType))) return;
	auto HISMC = Tiles[InTileType];
	HISMC->AddInstances(InTransform, false, true, true);
	HISMC->UpdateBounds();
	HISMC->UpdateNavigationBounds();
	HISMC->MarkRenderTransformDirty();
	
	auto NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSys) NavSys->Build();
}

void ATileGrid::ClearAllInstances()
{
	for (const auto& [TileType, MeshComponent] : Tiles)
	{
		if (!MeshComponent) continue;
		MeshComponent->ClearInstances();
	}
}

void ATileGrid::FinalizeNavigation()
{
	if (auto NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		NavSys->AddDirtyArea(this->GetComponentsBoundingBox(), ENavigationDirtyFlag::All);
	}	
}