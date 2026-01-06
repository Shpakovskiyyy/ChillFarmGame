// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TileBase.h"
#include "GameFramework/Actor.h"
#include "TileGrid.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

UCLASS()
class CHILLFARMGAME_API ATileGrid : public AActor
{
	GENERATED_BODY()

public:
	ATileGrid();

public:
	void SetupMeshComponents();

public:
	void AddInstances(ETileType InTileType, const TArray<FTransform>& InTransform);
	void ClearAllInstances();

public:
	void FinalizeNavigation();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Grid Components")
	TMap<ETileType, UHierarchicalInstancedStaticMeshComponent*> Tiles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Components")
	TMap<ETileType, UMaterialInstance*> TileTypeMaterials;

protected:
	FString MeshPath = "/Game/ChillFarmGame/GenerationTest/Meshes/SM_TestPlane.SM_TestPlane";
	
};