#include "BiomeTypeDataSet.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "Misc/EnumRange.h"
#endif

#define LOCTEXT_NAMESPACE "BiomeTypeDataSet"


#if WITH_EDITOR
EDataValidationResult UBiomeTypeDataSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	TSet<EBiomeType> SeenTypes;
	for (int32 Index = 0; Index < BiomeData.Num(); ++Index)
	{
		const UBiomeTypeData* Biome = BiomeData[Index];
		if (!Biome)
		{
			Context.AddError(FText::Format(LOCTEXT("EmptySlot", "Biome slot {0} is empty."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		bool bAlreadyInSet = false;
		SeenTypes.Add(Biome->BiomeType, &bAlreadyInSet);
		if (bAlreadyInSet)
		{
			Context.AddError(FText::Format(
				LOCTEXT("DuplicateType", "Biome type {0} is used by more than one asset ({1}); only the first one is ever used."),
				UEnum::GetDisplayValueAsText(Biome->BiomeType), FText::FromString(GetNameSafe(Biome))));
			Result = EDataValidationResult::Invalid;
		}
	}

	for (const ETileType TileType : TEnumRange<ETileType>())
	{
		if (TileType != ETileType::Water && !SoilFertility.Contains(TileType))
		{
			Context.AddWarning(FText::Format(
				LOCTEXT("MissingSoil", "Tile type {0} has no soil fertility entry, so nothing grows on it."),
				UEnum::GetDisplayValueAsText(TileType)));
		}
	}

	if (!SeenTypes.Contains(EBiomeType::Water))
	{
		Context.AddWarning(LOCTEXT("NoWater", "There is no Water biome: water tiles will have no shore or rules."));
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
