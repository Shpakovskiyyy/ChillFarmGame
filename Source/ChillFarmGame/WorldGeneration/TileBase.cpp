#include "TileBase.h"


ATileBase::ATileBase()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	SetRootComponent(Mesh);
}

void ATileBase::Initialize(ETileType InTileType)
{
	TileType = InTileType;
}

void ATileBase::OnClicked()
{
}

void ATileBase::Highlight()
{
}