#pragma once

#include "CoreMinimal.h"

#include "TileData.h"
#include "TileTypeVisualVariant.h"

#include "Engine/DataAsset.h"
#include "TileTypeVisualData.generated.h"

class UStaticMesh;
class UMaterialInterface;

// Defines visual data for ETileType.
UCLASS(BlueprintType)
class CHILLFARMGAME_API UTileTypeVisualData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	const FTileTypeVisualVariant* FindVisualVariant(const FTileVisualKey& Key) const;

	// Same as FindVisualVariant, but never leaves a tile without a mesh while the asset has at least one:
	// exact position first, then Center, then Isolated, then the first variant that has a mesh.
	const FTileTypeVisualVariant* FindVisualVariantOrFallback(const FTileVisualKey& Key) const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETileType TileType = ETileType::ETileType_MAX;

	UPROPERTY(EditAnywhere)
	TArray<FTileTypeVisualVariant> VisualsVariants;

	// TODO Should be moved to logical data
	UPROPERTY(EditAnywhere)
	bool bCanPlant = false;

	UPROPERTY(EditAnywhere)
	float MovementCost = 1.0f;

	UPROPERTY(EditAnywhere)
	bool bAffectsNavigation = true;
};

inline const FTileTypeVisualVariant* UTileTypeVisualData::FindVisualVariant(const FTileVisualKey& Key) const
{
	for (const auto& Variant : VisualsVariants)
	{
		if (Variant.VisualType == Key.VisualPositionType /*&& Variant.Rotation == Key.Rotation */)
		{
			return &Variant;
		}
	}
	return nullptr;
}

inline const FTileTypeVisualVariant* UTileTypeVisualData::FindVisualVariantOrFallback(const FTileVisualKey& Key) const
{
	const auto FindWithMesh = [this](ETileVisualPositionType Position) -> const FTileTypeVisualVariant*
	{
		for (const auto& Variant : VisualsVariants)
		{
			if (Variant.VisualType == Position && Variant.Mesh) return &Variant;
		}
		return nullptr;
	};

	if (const auto* Exact = FindWithMesh(Key.VisualPositionType)) return Exact;
	if (const auto* Center = FindWithMesh(ETileVisualPositionType::Center)) return Center;
	if (const auto* Isolated = FindWithMesh(ETileVisualPositionType::Isolated)) return Isolated;

	for (const auto& Variant : VisualsVariants)
	{
		if (Variant.Mesh) return &Variant;
	}
	return nullptr;
}