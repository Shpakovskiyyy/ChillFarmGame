#include "WaterTile.h"


AWaterTile::AWaterTile()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWaterTile::Initialize(ETileType InTileType)
{
	Super::Initialize(InTileType);

	// auto DecorationMesh = LoadObject<UStaticMesh>(this, TEXT("/Engine/BasicShapes/Plane"));
	// if ( DecorationMesh ) Mesh->SetStaticMesh(DecorationMesh);
	//
	// Mesh->SetRelativeScale3D(FVector(2.f, 2.f, 1.f));
}