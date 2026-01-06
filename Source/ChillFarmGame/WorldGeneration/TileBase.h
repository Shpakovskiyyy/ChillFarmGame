#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TileBase.generated.h"


UENUM(BlueprintType)
enum class ETileType : uint8
{
	Farmable,
	Grass,
	Forest,
	Water,
	ETileType_MAX UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(ETileType, ETileType::ETileType_MAX)

UCLASS(Transient)
class CHILLFARMGAME_API ATileBase : public AActor
{
	GENERATED_BODY()

public:
	ATileBase();

public:
	virtual void Initialize(ETileType InTileType);

public:
	virtual void OnClicked();
	virtual void Highlight();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETileType TileType;
	
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Mesh;
	
	UPROPERTY(VisibleAnywhere)
	int32 X;

	UPROPERTY(VisibleAnywhere)
	int32 Y;
	
};