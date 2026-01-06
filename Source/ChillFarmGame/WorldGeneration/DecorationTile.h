#pragma once

#include "CoreMinimal.h"
#include "TileBase.h"
#include "GameFramework/Actor.h"
#include "DecorationTile.generated.h"


UCLASS()
class CHILLFARMGAME_API ADecorationTile : public ATileBase
{
	GENERATED_BODY()

public:
	ADecorationTile();

public:
	virtual void Initialize(ETileType InTileType) override;
	
};