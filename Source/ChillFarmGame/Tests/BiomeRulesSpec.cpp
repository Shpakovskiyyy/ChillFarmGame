// Specs for biome selection (UBiomeTypeDataSet) and tile type rules (UTilesGenerationRuleSet).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TileTestUtils.h"
#include "BiomeTypeData.h"
#include "BiomeTypeDataSet.h"
#include "TilesGenerationRuleSet.h"
#include "TileNoiseContext.h"

using namespace TileTestUtils;


BEGIN_DEFINE_SPEC(FBiomeRulesSpec, "ChillFarmGame.TileWorld.BiomesAndRules", TileTestUtils::SpecFlags)
END_DEFINE_SPEC(FBiomeRulesSpec)

void FBiomeRulesSpec::Define()
{
	Describe("UBiomeTypeDataSet::ResolveBiomeBlend", [this]()
	{
		It("picks the biome whose ranges contain the point, with no blending", [this]()
		{
			UBiomeTypeData* Low = MakeBiome(EBiomeType::Plains, FVector2D(0.0, 0.5), FVector2D(0.0, 1.0));
			UBiomeTypeData* High = MakeBiome(EBiomeType::Mountains, FVector2D(0.5, 1.0), FVector2D(0.0, 1.0));
			const UBiomeTypeDataSet* Set = MakeBiomeSet({ Low, High });

			const SResolvedBiomeBlend InLow = Set->ResolveBiomeBlend(0.2f, 0.5f);
			TestTrue(TEXT("Low point -> low biome"), InLow.Primary == Low);
			TestEqual(TEXT("No blending inside a range"), InLow.BlendAlpha, 0.f);

			// The middle of a range must win as clearly as its edge (it used to score worse).
			const SResolvedBiomeBlend InHigh = Set->ResolveBiomeBlend(0.75f, 0.5f);
			TestTrue(TEXT("Middle of the high range -> high biome"), InHigh.Primary == High);
			TestEqual(TEXT("No blending in the middle of a range"), InHigh.BlendAlpha, 0.f);
		});

		It("blends between two biomes when the point is in neither", [this]()
		{
			UBiomeTypeData* Low = MakeBiome(EBiomeType::Plains, FVector2D(0.0, 0.2), FVector2D(0.0, 1.0));
			UBiomeTypeData* High = MakeBiome(EBiomeType::Mountains, FVector2D(0.8, 1.0), FVector2D(0.0, 1.0));
			const UBiomeTypeDataSet* Set = MakeBiomeSet({ Low, High });

			const SResolvedBiomeBlend Halfway = Set->ResolveBiomeBlend(0.5f, 0.5f);
			TestTrue(TEXT("Both biomes are candidates"), Halfway.Secondary != nullptr);
			TestEqual(TEXT("Halfway is a coin flip"), Halfway.BlendAlpha, 0.5f, 1.e-4f);

			const SResolvedBiomeBlend NearLow = Set->ResolveBiomeBlend(0.3f, 0.5f);
			TestTrue(TEXT("Closer biome is primary"), NearLow.Primary == Low);
			TestTrue(TEXT("Some chance for the other one"), NearLow.BlendAlpha > 0.f && NearLow.BlendAlpha < 0.5f);
			TestEqual(TEXT("Chance grows with the distance to the primary"), NearLow.BlendAlpha, 0.1f / (0.1f + 0.5f), 1.e-4f);
		});

		It("does not blend when the point is inside two overlapping biomes", [this]()
		{
			UBiomeTypeData* First = MakeBiome(EBiomeType::Plains, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0));
			UBiomeTypeData* Second = MakeBiome(EBiomeType::Forest, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0));
			const SResolvedBiomeBlend Blend = MakeBiomeSet({ First, Second })->ResolveBiomeBlend(0.5f, 0.5f);
			TestTrue(TEXT("First listed wins"), Blend.Primary == First);
			TestEqual(TEXT("No blending"), Blend.BlendAlpha, 0.f);
		});

		It("handles a single biome, an empty set and empty slots", [this]()
		{
			UBiomeTypeData* Only = MakeBiome(EBiomeType::Plains, FVector2D(0.0, 0.1), FVector2D(0.0, 0.1));

			const SResolvedBiomeBlend Single = MakeBiomeSet({ Only })->ResolveBiomeBlend(0.9f, 0.9f);
			TestTrue(TEXT("Only biome is primary even far away"), Single.Primary == Only);
			TestNull(TEXT("No secondary"), Single.Secondary);
			TestEqual(TEXT("No blending"), Single.BlendAlpha, 0.f);

			const SResolvedBiomeBlend Empty = MakeBiomeSet({})->ResolveBiomeBlend(0.5f, 0.5f);
			TestNull(TEXT("Empty set has no biome"), Empty.Primary);

			const SResolvedBiomeBlend WithNull = MakeBiomeSet({ nullptr, Only })->ResolveBiomeBlend(0.05f, 0.05f);
			TestTrue(TEXT("Empty slots are skipped"), WithNull.Primary == Only);
		});

		It("treats reversed ranges like normal ones", [this]()
		{
			UBiomeTypeData* Reversed = MakeBiome(EBiomeType::Plains, FVector2D(0.6, 0.4), FVector2D(1.0, 0.0));
			UBiomeTypeData* Other = MakeBiome(EBiomeType::Desert, FVector2D(0.9, 1.0), FVector2D(0.0, 1.0));
			const SResolvedBiomeBlend Blend = MakeBiomeSet({ Other, Reversed })->ResolveBiomeBlend(0.5f, 0.5f);
			TestTrue(TEXT("Point inside the reversed range"), Blend.Primary == Reversed);
			TestEqual(TEXT("No blending"), Blend.BlendAlpha, 0.f);
		});
	});

	Describe("UBiomeTypeDataSet::FindBiome", [this]()
	{
		It("finds a biome by type, the first one if there are several", [this]()
		{
			UBiomeTypeData* Plains = MakeBiome(EBiomeType::Plains, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0));
			UBiomeTypeData* Water = MakeBiome(EBiomeType::Water, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0));
			UBiomeTypeData* SecondPlains = MakeBiome(EBiomeType::Plains, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0));
			const UBiomeTypeDataSet* Set = MakeBiomeSet({ nullptr, Plains, Water, SecondPlains });

			TestTrue(TEXT("Water"), Set->FindBiome(EBiomeType::Water) == Water);
			TestTrue(TEXT("First plains"), Set->FindBiome(EBiomeType::Plains) == Plains);
			TestNull(TEXT("Missing type"), Set->FindBiome(EBiomeType::Desert));
		});
	});

	Describe("UTilesGenerationRuleSet::ResolveTileType", [this]()
	{
		const STileNoiseContext Noise{ 0.6f, 0.3f, 0.9f };

		It("returns the first rule that matches", [this, Noise]()
		{
			const UTilesGenerationRuleSet* Rules = MakeRuleSet({
				MakeRule(ETileType::Rock, ETileNoiseSource::Height, 0.7f, 1.0f),
				MakeRule(ETileType::Dirt, ETileNoiseSource::Height, 0.5f, 1.0f),
				MakeRule(ETileType::Sand, ETileNoiseSource::Height, 0.0f, 1.0f),
			});
			TestEq(*this, TEXT("First match"), Rules->ResolveTileType(Noise), ETileType::Dirt);
		});

		It("includes the minimum and excludes the maximum", [this]()
		{
			const UTilesGenerationRuleSet* Rules = MakeRuleSet({ MakeRule(ETileType::Rock, ETileNoiseSource::Height, 0.5f, 0.6f) });
			TestEq(*this, TEXT("At the minimum"), Rules->ResolveTileType({ 0.5f, 0.f, 0.f }), ETileType::Rock);
			TestEq(*this, TEXT("At the maximum"), Rules->ResolveTileType({ 0.6f, 0.f, 0.f }), ETileType::Grass);
		});

		It("reads the noise each rule asks for", [this, Noise]()
		{
			TestEq(*this, TEXT("Height"), MakeRuleSet({ MakeRule(ETileType::Rock, ETileNoiseSource::Height, 0.55f, 0.65f) })->ResolveTileType(Noise), ETileType::Rock);
			TestEq(*this, TEXT("Moisture"), MakeRuleSet({ MakeRule(ETileType::Rock, ETileNoiseSource::Moisture, 0.25f, 0.35f) })->ResolveTileType(Noise), ETileType::Rock);
			TestEq(*this, TEXT("Secondary"), MakeRuleSet({ MakeRule(ETileType::Rock, ETileNoiseSource::Secondary, 0.85f, 0.95f) })->ResolveTileType(Noise), ETileType::Rock);

			// Height + (Secondary - 0.5) * 0.2 = 0.6 + 0.08 = 0.68
			TestEq(*this, TEXT("Height plus secondary"), MakeRuleSet({ MakeRule(ETileType::Rock, ETileNoiseSource::HeightPlusSecondary, 0.67f, 0.69f) })->ResolveTileType(Noise), ETileType::Rock);
			TestEq(*this, TEXT("Height plus secondary is clamped"), MakeRuleSet({ MakeRule(ETileType::Rock, ETileNoiseSource::HeightPlusSecondary, 0.99f, 1.01f) })->ResolveTileType({ 1.f, 0.f, 1.f }), ETileType::Rock);
		});

		It("falls back to grass", [this, Noise]()
		{
			TestEq(*this, TEXT("No rules"), MakeRuleSet({})->ResolveTileType(Noise), ETileType::Grass);
			TestEq(*this, TEXT("No match"), MakeRuleSet({ MakeRule(ETileType::Rock, ETileNoiseSource::Height, 0.f, 0.1f) })->ResolveTileType(Noise), ETileType::Grass);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
