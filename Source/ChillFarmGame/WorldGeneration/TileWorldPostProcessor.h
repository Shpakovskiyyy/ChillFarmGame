#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TileWorldPostProcessor.generated.h"

class UTileDataGrid;


UCLASS()
class CHILLFARMGAME_API UTileWorldPostProcessor : public UObject
{
	GENERATED_BODY()

public:
	virtual void ProcessWorld(UTileDataGrid* TileGrid);

protected:
	void ApplyBiomeSmoothing(UTileDataGrid* TileGrid);
	void ApplyShoreSmoothing(UTileDataGrid* TileGrid);
	void ApplyTileSmoothing(UTileDataGrid* TileGrid);
	
};