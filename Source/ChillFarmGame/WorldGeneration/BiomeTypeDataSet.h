#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "BiomeTypeData.h"

#include "BiomeTypeDataSet.generated.h"


struct SResolvedBiomeBlend
{
	UBiomeTypeData* Primary = nullptr;
	UBiomeTypeData* Secondary = nullptr;
	float BlendAlpha = 0.f; // 0 = primary, 1 = secondary
};


UCLASS()
class CHILLFARMGAME_API UBiomeTypeDataSet : public UDataAsset
{
	GENERATED_BODY()

public:
	SResolvedBiomeBlend ResolveBiomeBlend(float Height, float Moisture) const;
	
private:
	// TODO Move to some utility
	float DistanceToRange(float InValue, const FVector2D& Range) const
	{
		return FMath::Min(
			FMath::Abs(InValue - Range.X),
			FMath::Abs(Range.Y - InValue)
		);
	}

public:
	UPROPERTY(EditDefaultsOnly)
	TArray<UBiomeTypeData*> BiomeData;
	
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

	Result.BlendAlpha = FMath::Clamp(
		SecondScore / (BestScore + SecondScore),
		0.f,
		1.f
	);
	
	return Result;
}