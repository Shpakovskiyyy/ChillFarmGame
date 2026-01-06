#pragma once

#include "CoreMinimal.h"
#include "TileBase.h"
#include "GameFramework/Actor.h"
#include "WaterTile.generated.h"


UCLASS()
class CHILLFARMGAME_API AWaterTile : public ATileBase
{
	GENERATED_BODY()

public:
	AWaterTile();

public:
	virtual void Initialize(ETileType InTileType) override;
	
};
