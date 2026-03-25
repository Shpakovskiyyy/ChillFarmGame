#include "TileVisualGrid.h"
#include "TilesVisualDataSet.h"

#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"


ATileVisualGrid::ATileVisualGrid()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void ATileVisualGrid::Initialize(uint32 InChunkSize, float InTileSize)
{
	ChunkSize = InChunkSize;
	TileSize = InTileSize;
	ClearAllChunks();
}

void ATileVisualGrid::CreateChunk(const FIntPoint& ChunkIndex, const TMap<FTileVisualKey, FTileInstancesBatch>& InInstancesBatch)
{
	auto& Chunk = Chunks.FindOrAdd(ChunkIndex);
	Chunk.ChunkCoordinate = ChunkIndex;
	
	for (auto& [TileVisualKey, InstancesBatch] : InInstancesBatch)
	{
		if (InstancesBatch.Transforms.IsEmpty()) continue;

		const auto TileData = TilesVisualSet->TileTypeVisuals[TileVisualKey.TileType];
		const auto Variant = TileData->FindVisualVariant(TileVisualKey);

		auto FoundHISMC = Chunk.Meshes.FindOrAdd(TileVisualKey);
		if (!FoundHISMC)
		{
			const auto Name =
				FString::Printf(TEXT("Chunk_X%d_Y%d_%s_%s_Rot%d"),
					ChunkIndex.X,
					ChunkIndex.Y,
					*UEnum::GetValueAsString(TileVisualKey.TileType),
					*UEnum::GetValueAsString(TileVisualKey.VisualPositionType),
					TileVisualKey.Rotation
				);

			auto HISMC = NewObject<UHierarchicalInstancedStaticMeshComponent>(
				this,
				FName(Name)
			);
		
			HISMC->SetupAttachment(RootComponent);
			HISMC->RegisterComponent();

			const FVector ChunkOrigin(
				ChunkIndex.X * ChunkSize * TileSize,
				ChunkIndex.Y * ChunkSize * TileSize,
				0.f
			);
			HISMC->SetWorldLocation(ChunkOrigin);

			HISMC->SetMobility(EComponentMobility::Static);
			HISMC->SetCollisionProfileName(TEXT("BlockAll"), true);
			HISMC->SetCollisionObjectType(ECC_WorldStatic);
			HISMC->SetCollisionResponseToAllChannels(ECR_Block);
			HISMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		
			HISMC->SetStaticMesh(Variant->Mesh);
			HISMC->SetMaterial(0, Variant->Material);
			HISMC->SetCanEverAffectNavigation(true);
			
			Chunk.Meshes.Add(TileVisualKey, HISMC);

			FoundHISMC = HISMC;
		}

		auto& Mapping = InstanceToTile.FindOrAdd(FoundHISMC);
		// Might be used for partial highlighting of a specific batch or pass
		//const auto BatchStartIndex = Mapping.Num();
		Mapping.Append(InstancesBatch.Coordinates);
		
		FoundHISMC->AddInstances(InstancesBatch.Transforms, false, false, true);
		FoundHISMC->UpdateBounds();
	}
}

void ATileVisualGrid::ClearAllChunks()
{
	for (const auto& [ChunkIndex, Chunk] : Chunks)
	{
		for (const auto& [VisualKey, Mesh] : Chunk.Meshes)
		{
			if (Mesh) Mesh->DestroyComponent();
		}
	}
	Chunks.Empty();
}

void ATileVisualGrid::RebuildChunk(const FIntPoint& ChunkCoordinates, UTileDataGrid* TileGridData, uint32 InChunkSize, float InTileSize)
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

void ATileVisualGrid::FinalizeNavigation()
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

const FIntPoint& ATileVisualGrid::GetTileCoordinatesFromHISMC(UHierarchicalInstancedStaticMeshComponent* InHISMC, uint32 InInstanceIndex)
{
	const auto& AllInstanceCoordinates = InstanceToTile.Find(InHISMC);
	if (!AllInstanceCoordinates) return FIntPoint::ZeroValue;

	if (!AllInstanceCoordinates->IsValidIndex(InInstanceIndex)) return FIntPoint::ZeroValue;
	return (*AllInstanceCoordinates)[InInstanceIndex];
}

void ATileVisualGrid::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	NavMeshBoundsVolume = Cast<ANavMeshBoundsVolume>(UGameplayStatics::GetActorOfClass(this, ANavMeshBoundsVolume::StaticClass()));
}
