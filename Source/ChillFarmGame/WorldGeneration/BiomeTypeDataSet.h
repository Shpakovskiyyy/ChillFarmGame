#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "BiomeTypeData.h"

#include "BiomeTypeDataSet.generated.h"


struct SResolvedBiomeBlend
{
	UBiomeTypeData* Primary = nullptr;
	UBiomeTypeData* Secondary = nullptr;
	float BlendAlpha = 0.f; // Chance to use Secondary: 0 inside the primary's ranges, up to 0.5 halfway between two biomes
};


UCLASS()
class CHILLFARMGAME_API UBiomeTypeDataSet : public UDataAsset
{
	GENERATED_BODY()

public:
	SResolvedBiomeBlend ResolveBiomeBlend(float Height, float Moisture) const;

	// The asset for a biome type, nullptr when the set has none. Tiles store the type, not the asset.
	const UBiomeTypeData* FindBiome(EBiomeType InType) const
	{
		for (const UBiomeTypeData* Biome : BiomeData)
		{
			if (Biome && Biome->BiomeType == InType) return Biome;
		}
		return nullptr;
	}

	// Multiplier for a surface type in the fertility formula. Types missing from the table count as 0.
	float GetSoilFertility(ETileType InType) const
	{
		const float* Factor = SoilFertility.Find(InType);
		return Factor ? FMath::Max(0.f, *Factor) : 0.f;
	}

#if WITH_EDITOR
	// Empty slots and two assets with the same biome type (FindBiome would only ever see the first).
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	
private:
	// TODO Move to some utility
	// How far the value lies outside the range: 0 anywhere inside it, otherwise the gap to the nearest bound.
	// The previous version returned the distance to the nearest bound even for values inside the range,
	// so the middle of a biome's range scored worse than its borders.
	float DistanceToRange(float InValue, const FVector2D& Range) const
	{
		const float RangeMin = static_cast<float>(FMath::Min(Range.X, Range.Y));
		const float RangeMax = static_cast<float>(FMath::Max(Range.X, Range.Y));

		if (InValue < RangeMin) return RangeMin - InValue;
		if (InValue > RangeMax) return InValue - RangeMax;
		return 0.f;
	}

public:
	UPROPERTY(EditDefaultsOnly)
	TArray<UBiomeTypeData*> BiomeData;

	// How well each surface grows crops, as a multiplier of the biome's base fertility (0 = nothing grows).
	// A type missing from the table counts as 0, so a new tile type is not plantable until it is added here.
	UPROPERTY(EditDefaultsOnly, Category = "Fertility", meta = (ClampMin = "0"))
	TMap<ETileType, float> SoilFertility = {
		{ ETileType::Grass, 1.0f },
		{ ETileType::Dirt,  0.8f },
		{ ETileType::Sand,  0.3f },
		{ ETileType::Rock,  0.0f },
		{ ETileType::Water, 0.0f },
		{ ETileType::Farmland, 1.0f },
	};

};

FORCEINLINE SResolvedBiomeBlend UBiomeTypeDataSet::ResolveBiomeBlend(float Height, float Moisture) const
{
	SResolvedBiomeBlend Result;

	auto BestScore = MAX_flt;
	auto SecondScore = MAX_flt;

	for (const auto Biome : BiomeData)
	{
		if (!Biome) continue;

		const auto HeightDistance = DistanceToRange(Height, Biome->HeightRange);
		const auto MoistureDistance = DistanceToRange(Moisture, Biome->MoistureRange);

		const auto Score = HeightDistance + MoistureDistance;

		if (Score < BestScore)
		{
			SecondScore = BestScore;
			Result.Secondary = Result.Primary;

			BestScore = Score;
			Result.Primary = Biome;
		}
		else if (Score < SecondScore)
		{
			SecondScore = Score;
			Result.Secondary = Biome;
		}
	}

	// Chance to pick the secondary biome: 0 while the point is inside the primary range (BestScore == 0),
	// rising to 0.5 where both biomes are equally far away. It used to be SecondScore / Sum, which is never
	// below 0.5 and is 0/0 when the point is inside both ranges.
	const auto ScoreSum = BestScore + SecondScore;
	Result.BlendAlpha = (Result.Secondary && ScoreSum > KINDA_SMALL_NUMBER && ScoreSum < MAX_flt)
		? FMath::Clamp(BestScore / ScoreSum, 0.f, 0.5f)
		: 0.f;

	return Result;
}