#include "TilesGenerationRuleSet.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "TilesGenerationRuleSet"


// Replaces the constructor check, which ran before the asset's rules were loaded and therefore
// reported every tile type as missing for every asset. A rule set also does not have to mention
// every tile type (a desert has no grass), so this validates the rules themselves instead.
#if WITH_EDITOR
EDataValidationResult UTilesGenerationRuleSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (Rules.IsEmpty())
	{
		Context.AddError(LOCTEXT("NoRules", "The rule set has no rules, every tile will fall back to Grass."));
		Result = EDataValidationResult::Invalid;
	}

	for (int32 Index = 0; Index < Rules.Num(); ++Index)
	{
		const FTileGenerationRule& Rule = Rules[Index];

		if (Rule.TileType == ETileType::ETileType_MAX)
		{
			Context.AddError(FText::Format(LOCTEXT("RuleWithoutType", "Rule {0} has no tile type."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}

		if (Rule.MinNoise >= Rule.MaxNoise)
		{
			Context.AddError(FText::Format(
				LOCTEXT("EmptyNoiseRange", "Rule {0} can never match: MinNoise ({1}) is not below MaxNoise ({2})."),
				FText::AsNumber(Index), FText::AsNumber(Rule.MinNoise), FText::AsNumber(Rule.MaxNoise)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE

ETileType UTilesGenerationRuleSet::ResolveTileType(const STileNoiseContext& Context) const
{
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

		// Secondary is the 0..1 detail noise, so this nudges height by at most +/-0.1.
		case ETileNoiseSource::HeightPlusSecondary:
			return FMath::Clamp(Context.Height + (Context.Secondary - 0.5f) * 0.2f, 0.f, 1.f);
	}

	return 0.f;
}