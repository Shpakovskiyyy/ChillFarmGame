#include "TilesVisualDataSet.h"

#include "Misc/EnumRange.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "TilesVisualDataSet"


// The check used to live in the constructor. A constructor runs before the asset's properties are
// loaded (and for the class default object), so the map was always empty there and every tile type
// was reported as missing no matter what the asset contained.
#if WITH_EDITOR
EDataValidationResult UTilesVisualDataSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	for (const ETileType TileType : TEnumRange<ETileType>())
	{
		const auto* VisualData = TileTypeVisuals.Find(TileType);
		if (!VisualData || !*VisualData)
		{
			Context.AddError(FText::Format(
				LOCTEXT("MissingTileTypeVisual", "Tile type {0} has no visual data, tiles of this type will not be drawn."),
				UEnum::GetDisplayValueAsText(TileType)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
