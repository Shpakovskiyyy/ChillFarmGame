#include "CropType.h"

#include "Engine/StaticMesh.h"
#include "Misc/DataValidation.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "CropType"


UCropType::UCropType()
{
	// Placeholder crop from engine shapes: a seed, a sprout, a young plant and the ripe crop.
	// Real crop assets replace all of this; it exists so the loop can be played without art.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));

	DisplayName = LOCTEXT("PlaceholderName", "Placeholder crop");

	auto AddStage = [this](int32 InDays, UStaticMesh* InMesh, const FVector& InScale)
	{
		FCropStage& Stage = Stages.AddDefaulted_GetRef();
		Stage.Days = InDays;
		Stage.Mesh = InMesh;
		Stage.Scale = InScale;
	};
	AddStage(1, Sphere.Object, FVector(0.12, 0.12, 0.06));
	AddStage(1, Cone.Object, FVector(0.15, 0.15, 0.2));
	AddStage(2, Cone.Object, FVector(0.3, 0.3, 0.45));
	AddStage(1, Sphere.Object, FVector(0.45, 0.45, 0.45));
}

int32 UCropType::GetDaysToRipe() const
{
	int32 Days = 0;
	for (int32 Index = 0; Index < GetLastStage(); ++Index) Days += FMath::Max(1, Stages[Index].Days);
	return Days;
}

#if WITH_EDITOR
EDataValidationResult UCropType::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (Stages.Num() < 2)
	{
		Context.AddError(LOCTEXT("TooFewStages", "A crop needs at least two stages: one to grow in and the ripe one."));
		Result = EDataValidationResult::Invalid;
	}
	if (Stages.Num() > MAX_uint8)
	{
		Context.AddError(LOCTEXT("TooManyStages", "A crop can have at most 255 stages."));
		Result = EDataValidationResult::Invalid;
	}
	if (RegrowStage != INDEX_NONE && !Stages.IsValidIndex(RegrowStage))
	{
		Context.AddError(FText::Format(LOCTEXT("BadRegrowStage", "Regrow stage {0} does not exist."), FText::AsNumber(RegrowStage)));
		Result = EDataValidationResult::Invalid;
	}
	if (RegrowStage == GetLastStage() && RegrowStage != INDEX_NONE)
	{
		Context.AddError(LOCTEXT("RegrowRipe", "Regrow stage is the ripe stage: the crop could be harvested every day without growing."));
		Result = EDataValidationResult::Invalid;
	}
	for (int32 Index = 0; Index < Stages.Num(); ++Index)
	{
		if (!Stages[Index].Mesh)
		{
			Context.AddWarning(FText::Format(LOCTEXT("NoMesh", "Stage {0} has no mesh: the crop is invisible in it."), FText::AsNumber(Index)));
		}
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
