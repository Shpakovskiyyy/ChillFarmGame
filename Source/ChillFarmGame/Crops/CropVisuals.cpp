#include "CropVisuals.h"

#include "CropSubsystem.h"
#include "CropType.h"
#include "TileWorldSubsystem.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"


ACropVisuals::ACropVisuals()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void ACropVisuals::Bind(UCropSubsystem& InCrops)
{
	if (UCropSubsystem* Old = Crops.Get()) Old->OnCropsChanged.RemoveAll(this);

	Crops = &InCrops;
	InCrops.OnCropsChanged.AddUObject(this, &ACropVisuals::HandleCropsChanged);
	RebuildAll();
}

void ACropVisuals::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCropSubsystem* Old = Crops.Get()) Old->OnCropsChanged.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}

void ACropVisuals::HandleCropsChanged(TConstArrayView<FIntPoint> InTiles)
{
	RebuildAll();
}

UHierarchicalInstancedStaticMeshComponent* ACropVisuals::GetComponent(UStaticMesh* InMesh, UMaterialInterface* InMaterial)
{
	const FString Key = InMesh->GetPathName() + TEXT("|") + GetPathNameSafe(InMaterial);
	if (TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* Found = Components.Find(Key)) return *Found;

	UHierarchicalInstancedStaticMeshComponent* Component = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
	Component->SetStaticMesh(InMesh);
	if (InMaterial) Component->SetMaterial(0, InMaterial);

	// Crops are small and the player walks through them: they neither block nor affect navigation.
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCanEverAffectNavigation(false);
	Component->SetupAttachment(RootComponent);
	Component->RegisterComponent();
	AddInstanceComponent(Component);

	Components.Add(Key, Component);
	return Component;
}

void ACropVisuals::RebuildAll()
{
	for (const TPair<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Pair : Components)
	{
		if (Pair.Value) Pair.Value->ClearInstances();
	}

	const UCropSubsystem* CropWorld = Crops.Get();
	const UTileWorldSubsystem* TileWorld = CropWorld ? CropWorld->GetTileWorld() : nullptr;
	if (!TileWorld || !TileWorld->HasTiles()) return;

	const FTileGridLayout& Layout = TileWorld->GetLayout();
	TMap<UHierarchicalInstancedStaticMeshComponent*, TArray<FTransform>> Instances;

	for (const TPair<FIntPoint, FPlantedCrop>& Pair : CropWorld->GetCrops())
	{
		const UCropType* Type = Pair.Value.Type;
		if (!Type || !Type->Stages.IsValidIndex(Pair.Value.State.Stage)) continue;

		const FCropStage& Stage = Type->Stages[Pair.Value.State.Stage];
		const FTileData* Tile = TileWorld->GetGrid().FindTile(Pair.Key);
		if (!Stage.Mesh || !Tile) continue;

		// Stand the mesh on the surface, centred on the tile, whatever its pivot.
		const FBox Bounds = Stage.Mesh->GetBoundingBox();
		const FVector Offset(-Bounds.GetCenter().X * Stage.Scale.X, -Bounds.GetCenter().Y * Stage.Scale.Y, -Bounds.Min.Z * Stage.Scale.Z);
		const FVector Location = Layout.TileCenter(Pair.Key, Tile->Height) + Offset;

		Instances.FindOrAdd(GetComponent(Stage.Mesh, Stage.Material)).Add(FTransform(FQuat::Identity, Location, Stage.Scale));
	}

	for (TPair<UHierarchicalInstancedStaticMeshComponent*, TArray<FTransform>>& Pair : Instances)
	{
		Pair.Key->AddInstances(Pair.Value, false, true);
	}
}
