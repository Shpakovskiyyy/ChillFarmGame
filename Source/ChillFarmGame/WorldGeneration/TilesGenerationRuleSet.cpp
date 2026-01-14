#include "TilesGenerationRuleSet.h"

#include "Misc/EnumRange.h"

DEFINE_LOG_CATEGORY_STATIC(LogTileGenerationRuleSet, All, All)

UTilesGenerationRuleSet::UTilesGenerationRuleSet()
{
	for (auto TileType : TEnumRange<ETileType>())
	{
		auto bFound = false;
		for (const auto& Rule : Rules)
		{
			if (Rule.TileType == TileType) bFound = true;
		}
		if (!bFound) UE_LOG(LogTileGenerationRuleSet, Warning, TEXT("%s - missing TileType %s in TileGenRuleSet. Fix this to enable this TileType generation"), *GetName(), *UEnum::GetValueAsString(TileType))
	}
}

ETileType UTilesGenerationRuleSet::ResolveTileType(const STileNoiseContext& Context)
{
	// for (const auto& Rule : Rules)
	// {
	// 	if (NoiseValue < Rule.MinNoise || NoiseValue >= Rule.MaxNoise) continue;
	// 	return Rule.TileType;
	// }
	//
	// return ETileType::Grass;
	
	for (const auto& Rule : Rules)
	{
		auto Noise = GetNoiseBySource(Rule.NoiseSource, Context);

		if (Noise >= Rule.MinNoise && Noise < Rule.MaxNoise)
			return Rule.TileType;
	}
	return ETileType::Grass;
}

float UTilesGenerationRuleSet::GetNoiseBySource(ETileNoiseSource NoiseSource, const STileNoiseContext& Context)
{
	switch (NoiseSource)
	{
		case ETileNoiseSource::Height:
			return Context.Height;

		case ETileNoiseSource::Moisture:
			return Context.Moisture;

		case ETileNoiseSource::Secondary:
			return Context.Secondary;

		case ETileNoiseSource::HeightPlusSecondary:
			return FMath::Clamp(Context.Height + (Context.Secondary - 0.5f) * 0.2f, 0.f, 1.f);
	}

	return 0.f;
}