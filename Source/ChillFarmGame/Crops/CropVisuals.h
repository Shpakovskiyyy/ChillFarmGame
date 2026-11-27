#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CropVisuals.generated.h"

class UCropSubsystem;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;


// Draws the crops of a UCropSubsystem: one instanced component per stage look (mesh and material).
//
// Holds no crop data. It rebuilds every instance whenever a crop changes, which is cheap for the
// hundreds of crops a farm has; per-instance updates can come later if fields get much bigger.
UCLASS(NotPlaceable, Transient)
class CHILLFARMGAME_API ACropVisuals : public AActor
{
	GENERATED_BODY()

public:
	ACropVisuals();

	void Bind(UCropSubsystem& InCrops);

	// Rebuilds every instance from the bound subsystem.
	void RebuildAll();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleCropsChanged(TConstArrayView<FIntPoint> InTiles);
	UHierarchicalInstancedStaticMeshComponent* GetComponent(UStaticMesh* InMesh, UMaterialInterface* InMaterial);

private:
	TWeakObjectPtr<UCropSubsystem> Crops;

	// One component per mesh and material pair, kept between rebuilds and emptied instead.
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Components;
};
