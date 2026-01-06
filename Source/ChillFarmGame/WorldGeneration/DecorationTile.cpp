#include "DecorationTile.h"


ADecorationTile::ADecorationTile()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ADecorationTile::Initialize(ETileType InTileType)
{
	Super::Initialize(InTileType);

	// auto DecorationMesh = LoadObject<UStaticMesh>(this, TEXT("/Engine/BasicShapes/Sphere"));
	// if ( DecorationMesh ) Mesh->SetStaticMesh(DecorationMesh);
	//
	// Mesh->SetRelativeScale3D(FVector(2.f, 2.f, 1.f));
}
