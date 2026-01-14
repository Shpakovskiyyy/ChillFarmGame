#include "TileGrid.h"
#include "TilesVisualDataSet.h"

#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"


ATileGrid::ATileGrid()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void ATileGrid::CreateChunk(const FIntPoint& ChunkIndex, const TMap<FTileVisualKey, TArray<FTransform>>& InTransforms)
{
	auto& Chunk = Chunks.FindOrAdd(ChunkIndex);
	Chunk.ChunkCoordinate = ChunkIndex;
	
	for (auto& [TileVisualKey, Transforms] : InTransforms)
	{
		if (Transforms.IsEmpty()) continue;

		const auto TileData = TilesVisualSet->TileTypeVisuals[TileVisualKey.TileType];
		const auto Variant = TileData->FindVisualVariant(TileVisualKey);

		const auto Name =
			FString::Printf(TEXT("Chunk_X%d_Y%d_%s_%s_Rot%d"),
				ChunkIndex.X,
				ChunkIndex.Y,
				*UEnum::GetValueAsString(TileVisualKey.TileType),
				*UEnum::GetValueAsString(TileVisualKey.VisualPositionType),
				TileVisualKey.Rotation);

		auto HISMC = NewObject<UHierarchicalInstancedStaticMeshComponent>(
			this,
			FName(Name)
		);
		
		HISMC->SetupAttachment(RootComponent);
		HISMC->RegisterComponent();

		HISMC->SetMobility(EComponentMobility::Static);
		HISMC->SetCollisionProfileName(TEXT("BlockAll"), true);
		HISMC->SetCollisionObjectType(ECC_WorldStatic);
		HISMC->SetCollisionResponseToAllChannels(ECR_Block);
		HISMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		
		HISMC->SetStaticMesh(Variant->Mesh);
		HISMC->SetMaterial(0, Variant->Material);
		HISMC->SetCanEverAffectNavigation(true);

		HISMC->AddInstances(Transforms, false, true, true);
		HISMC->UpdateBounds();

		Chunk.Meshes.Add(TileVisualKey, HISMC);
	}
}

void ATileGrid::ClearAllChunks()
{
	for (auto& [TileType, TileChunk] : Chunks)
	{
		if (TileChunk.Meshes.IsEmpty()) continue;
		for (auto& [_, Mesh] : TileChunk.Meshes)
		{
			while (Mesh->GetInstanceCount() > 0)
			{
				Mesh->RemoveInstance(0);
			}
		}
		TileChunk.Meshes.Empty();
	}
	Chunks.Empty();
}

void ATileGrid::RebuildChunk(const FIntPoint& ChunkCoordinates, UTileDataGrid* TileGridData, int32 ChunkSize, float TileSize)
{
	// auto Chunk = Chunks.Find(ChunkCoordinates);
	// if (!Chunk) return;
	//
	// for (auto& [Type, HISMC] : Chunk->Meshes) HISMC->ClearInstances();
	//
	// for (auto X = 0; X < ChunkSize; X++)
	// {
	// 	for (auto Y = 0; Y < ChunkSize; Y++)
	// 	{
	// 		int32 TileX = ChunkCoordinates.X * ChunkSize + X;
	// 		int32 TileY = ChunkCoordinates.Y * ChunkSize + Y;
	//
	// 		if (!TileGridData->IsValidTile(TileX, TileY)) continue;
	//
	// 		auto Tile = TileGridData->GetTileChecked(TileX, TileY);
	// 		if (Tile->Type == ETileType::ETileType_MAX) continue;
	//
	// 		FVector WorldPos = TileGridData->TileToWorld(TileX, TileY);
	//
	// 		Chunk->Meshes[Tile->Type]->AddInstance(
	// 			FTransform(WorldPos)
	// 		);
	// 	}
	// }
	//
	// if (auto NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	// {
	// 	NavSys->AddDirtyArea(
	// 		Chunk->Meshes.begin()->Value->Bounds.GetBox(),
	// 		ENavigationDirtyFlag::All
	// 	);
	// }
}

void ATileGrid::FinalizeNavigation()
{
	// UE_LOG(LogTemp, Warning, TEXT("Size of Chunks map in FinalizeNavigation - %d"), Chunks.Num())
	// for (const auto& [ChunkIndex, ChunkVisualRepresentation] : Chunks)
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("Size of Meshes map in FinalizeNavigation for chunk #%d%d - %d"), ChunkIndex.X, ChunkIndex.Y, ChunkVisualRepresentation.Meshes.Num())
	// 	for (const auto [TileVisualKey, Component] : ChunkVisualRepresentation.Meshes)
	// 	{
	// 		UE_LOG(LogTemp, Warning, TEXT("Instance count in ChunkVisualRepresentation #%d%d - %d"), ChunkIndex.X, ChunkIndex.Y, Component->GetInstanceCount());
	// 	}
	// }
	
	if (auto NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		NavSys->AddDirtyArea(
			this->GetComponentsBoundingBox(),
			ENavigationDirtyFlag::All
		);
		NavSys->OnNavigationBoundsUpdated(NavMeshBoundsVolume.Get());
	}
}

void ATileGrid::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	NavMeshBoundsVolume = Cast<ANavMeshBoundsVolume>(UGameplayStatics::GetActorOfClass(this, ANavMeshBoundsVolume::StaticClass()));
}