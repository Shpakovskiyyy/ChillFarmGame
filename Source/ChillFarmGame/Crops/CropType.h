#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CropType.generated.h"

class UMaterialInterface;
class UStaticMesh;


// One look of a growing crop and how long it keeps it.
USTRUCT(BlueprintType)
struct FCropStage
{
	GENERATED_BODY()

	// Watered nights the crop spends in this stage before it moves to the next one. Ignored on the
	// last stage: that one is the ripe crop and waits for the harvest.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = "1"))
	int32 Days = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	// Optional. The mesh's own material when empty.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	TObjectPtr<UMaterialInterface> Material = nullptr;

	// Scale of the mesh. The mesh stands on the tile's surface, centred on the tile, whatever its pivot.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	FVector Scale = FVector::OneVector;
};


// A kind of crop: its growth stages and what a harvest gives.
//
// Read-only data. The planted crops themselves live in UCropSubsystem, keyed by tile; they only point
// at their type. The class default object is a working placeholder crop made of engine shapes, so the
// farming loop can be played before any crop asset exists.
UCLASS(BlueprintType)
class CHILLFARMGAME_API UCropType : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UCropType();

	int32 NumStages() const { return Stages.Num(); }
	int32 GetLastStage() const { return Stages.Num() - 1; }

	// Watered nights from planting to ripe.
	int32 GetDaysToRipe() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	FText DisplayName;

	// From the seed to the ripe crop. The last stage is the one that can be harvested.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	TArray<FCropStage> Stages;

	// How many items one harvest gives.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = "1"))
	int32 HarvestYield = 1;

	// The stage a harvested crop goes back to and grows again from (bushes, vines). None: the crop is
	// gone after the harvest.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = "-1"))
	int32 RegrowStage = INDEX_NONE;
};
