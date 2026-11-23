#include "TileVisualGrid.h"

#include "TileGrid.h"
#include "TileCliffs.h"
#include "TileNeighborResolver.h"
#include "TileWorldSubsystem.h"
#include "TilesVisualDataSet.h"

#include "NavigationSystem.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogTileVisualGrid, Log, All);


ATileVisualGrid::ATileVisualGrid()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void ATileVisualGrid::RebuildAll(UTileWorldSubsystem& InTileWorld, int32 InChunkSize)
{
	ChunkSize = FMath::Max(1, InChunkSize);

	if (TileWorld.Get() != &InTileWorld)
	{
		UnbindFromTileWorld();
		TileWorld = &InTileWorld;
		InTileWorld.OnTilesChanged.AddUObject(this, &ATileVisualGrid::HandleTilesChanged);
		InTileWorld.OnWorldCleared.AddUObject(this, &ATileVisualGrid::HandleWorldCleared);
	}

	ClearAllChunks();

	const FTileGrid& Grid = InTileWorld.GetGrid();
	if (Grid.IsEmpty()) return;

	const FIntPoint Size = Grid.GetSize();
	for (int32 CY = 0; CY * ChunkSize < Size.Y; ++CY)
	{
		for (int32 CX = 0; CX * ChunkSize < Size.X; ++CX)
		{
			BuildChunk(Grid, FIntPoint(CX, CY));
		}
	}

	uint8 MaxLevel = 0;
	for (const FTileData& Tile : Grid.GetTiles()) MaxLevel = FMath::Max(MaxLevel, Tile.Height);
	FinalizeNavigation(Grid.GetLayout().GetWorldBounds(MaxLevel).ExpandBy(Grid.GetLayout().TileSize));
}

void ATileVisualGrid::RebuildChunks(const TSet<FIntPoint>& InChunks)
{
	const UTileWorldSubsystem* World = TileWorld.Get();
	if (!World || !World->HasTiles()) return;

	const FTileGrid& Grid = World->GetGrid();

	FBox DirtyArea(ForceInit);
	for (const FIntPoint& Chunk : InChunks)
	{
		DestroyChunk(Chunk);
		BuildChunk(Grid, Chunk);
		DirtyArea += GetChunkBounds(Grid, Chunk);
	}

	if (DirtyArea.IsValid) FinalizeNavigation(DirtyArea);
}

void ATileVisualGrid::ClearAllChunks()
{
	TArray<FIntPoint> ChunkKeys;
	Chunks.GetKeys(ChunkKeys);
	for (const FIntPoint& Chunk : ChunkKeys) DestroyChunk(Chunk);
	Chunks.Empty();

	// Older versions kept chunk components in a saved property, so a level saved back then can still
	// contain some. This actor owns no other instanced meshes, so anything left over is stale.
	TArray<UHierarchicalInstancedStaticMeshComponent*> Leftovers;
	GetComponents(Leftovers);
	for (UHierarchicalInstancedStaticMeshComponent* Component : Leftovers)
	{
		if (Component) Component->DestroyComponent();
	}
}

void ATileVisualGrid::BuildChunk(const FTileGrid& InGrid, const FIntPoint& InChunk)
{
	if (!TilesVisualSet)
	{
		UE_LOG(LogTileVisualGrid, Error, TEXT("'%s' Cannot draw tiles: TilesVisualSet is not assigned"), *GetNameSafe(this));
		return;
	}

	const FTileGridLayout& Layout = InGrid.GetLayout();
	const FIntPoint First(InChunk.X * ChunkSize, InChunk.Y * ChunkSize);
	const FIntPoint End(FMath::Min(First.X + ChunkSize, Layout.Size.X), FMath::Min(First.Y + ChunkSize, Layout.Size.Y));

	// Group the chunk's tiles by the piece they need. The key ignores rotation on purpose (all
	// rotations of a piece share one component), so each tile keeps its own rotation next to it.
	struct FPlacedTile
	{
		FIntPoint Tile;
		int32 Rotation;
		uint8 Slope;
	};
	TMap<FTileVisualKey, TArray<FPlacedTile>> TilesByKey;

	// Slopes the visual set has a piece for, per shape. Edge, OuterCorner and InnerCorner.
	constexpr int32 NumPieceShapes = 3;
	const ETileSlopeShape PieceShapes[NumPieceShapes] = { ETileSlopeShape::Edge, ETileSlopeShape::OuterCorner, ETileSlopeShape::InnerCorner };
	TMap<FTileVisualKey, TArray<FPlacedTile>> SlopePiecesByKey[NumPieceShapes];

	for (int32 Y = First.Y; Y < End.Y; ++Y)
	{
		for (int32 X = First.X; X < End.X; ++X)
		{
			const FIntPoint Tile(X, Y);
			const FTileData& Data = InGrid.GetTile(Tile);
			if (!Data.IsGenerated()) continue;

			// A slope is drawn with the plain centre piece's material: edge pieces would not line up with it.
			FTileVisualKey Key = Data.IsSlope()
				? FTileNeighborResolver::Resolve(Data.Type, 0xFF)
				: FTileNeighborResolver::Resolve(Data.Type, FTileNeighborResolver::ComputeSameTypeMask(InGrid, Tile));
			Key.bWatered = Data.HasFlag(ETileFlags::Watered);

			TMap<FTileVisualKey, TArray<FPlacedTile>>* Group = &TilesByKey;
			if (Data.IsSlope())
			{
				FIntPoint MeshFacing;
				const ETileSlopeShape Shape = TileData::GetSlopeShape(Data.Slope);
				for (int32 PieceIndex = 0; PieceIndex < NumPieceShapes; ++PieceIndex)
				{
					if (PieceShapes[PieceIndex] == Shape && TilesVisualSet->GetSlopeMesh(Shape, MeshFacing)) Group = &SlopePiecesByKey[PieceIndex];
				}

				// An inner corner without its own piece: two edge pieces rising to its two raised edges.
				// Together they are solid and close the corner's sides, so no gap opens under it.
				if (Group == &TilesByKey && Shape == ETileSlopeShape::InnerCorner && TilesVisualSet->GetSlopeMesh(ETileSlopeShape::Edge, MeshFacing))
				{
					const FIntPoint LowCorner = TileData::GetSlopeFacing(Data.Slope);
					TArray<FPlacedTile>& Edges = SlopePiecesByKey[0].FindOrAdd(Key);
					Edges.Add({ Tile, Key.Rotation, TileData::MakeEdgeSlope(FIntPoint(-LowCorner.X, 0)) });
					Edges.Add({ Tile, Key.Rotation, TileData::MakeEdgeSlope(FIntPoint(0, -LowCorner.Y)) });
					continue;
				}
			}
			Group->FindOrAdd(Key).Add({ Tile, Key.Rotation, Data.Slope });
		}
	}

	FTileChunkVisualData& Chunk = Chunks.FindOrAdd(InChunk);
	Chunk.ChunkCoordinate = InChunk;

	for (const auto& [Key, Tiles] : TilesByKey)
	{
		const UTileTypeVisualData* const* TypeVisualsPtr = TilesVisualSet->TileTypeVisuals.Find(Key.TileType);
		const UTileTypeVisualData* TypeVisuals = TypeVisualsPtr ? *TypeVisualsPtr : nullptr;
		const FTileTypeVisualVariant* Variant = TypeVisuals ? TypeVisuals->FindVisualVariantOrFallback(Key) : nullptr;
		if (!Variant || !Variant->Mesh)
		{
			UE_LOG(LogTileVisualGrid, Warning, TEXT("'%s' Has no mesh for tile type %s, %d tiles are not drawn"),
				*GetNameSafe(TilesVisualSet), *UEnum::GetValueAsString(Key.TileType), Tiles.Num());
			continue;
		}

		// Place the mesh so the centre of its bounds sits on the tile centre and its top is flush with
		// the tile's level. Works whatever the mesh pivot is (centred plane, corner pivot, cube).
		const FBox MeshBounds = Variant->Mesh->GetBoundingBox();
		const FVector MeshAnchor(MeshBounds.GetCenter().X, MeshBounds.GetCenter().Y, MeshBounds.Max.Z);

		TArray<FTransform> Transforms;
		Transforms.Reserve(Tiles.Num());
		for (const FPlacedTile& Placed : Tiles)
		{
			const uint8 Level = InGrid.GetTile(Placed.Tile).Height;

			// A slope without a piece: tilt the tile for an edge, otherwise draw it flat, an inner
			// corner (mostly high) on the level above.
			const ETileSlopeShape Shape = TileData::GetSlopeShape(Placed.Slope);
			if (Shape == ETileSlopeShape::Edge)
			{
				Transforms.Add(TileCliffs::MakeTiltedTileTransform(Layout, Placed.Tile, Level, TileData::GetSlopeFacing(Placed.Slope), MeshBounds));
				continue;
			}

			const FQuat Rotation(FRotator(0.f, static_cast<float>(Placed.Rotation), 0.f));
			const FVector TileCenter = Layout.TileCenter(Placed.Tile, Shape == ETileSlopeShape::InnerCorner ? Level + 1 : Level);
			Transforms.Emplace(Rotation, TileCenter - Rotation.RotateVector(MeshAnchor));
		}

		const FString BaseName = FString::Printf(TEXT("Chunk_X%d_Y%d_%s_%s%s"),
			InChunk.X, InChunk.Y,
			*UEnum::GetValueAsString(Key.TileType),
			*UEnum::GetValueAsString(Key.VisualPositionType),
			Key.bWatered ? TEXT("_Watered") : TEXT(""));

		UHierarchicalInstancedStaticMeshComponent* Component = CreateChunkComponent(BaseName, Variant->Mesh, Variant->GetMaterial(Key.bWatered));
		Component->AddInstances(Transforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);

		Chunk.Meshes.Add(Key, Component);
	}

	// Slope pieces: the shape's mesh, painted like the tile type it belongs to.
	const TCHAR* PieceNames[NumPieceShapes] = { TEXT("SlopeEdge"), TEXT("SlopeOuter"), TEXT("SlopeInner") };
	for (int32 PieceIndex = 0; PieceIndex < NumPieceShapes; ++PieceIndex)
	{
		FIntPoint MeshFacing;
		UStaticMesh* PieceMesh = TilesVisualSet->GetSlopeMesh(PieceShapes[PieceIndex], MeshFacing);
		if (!PieceMesh) continue;
		const FBox MeshBounds = PieceMesh->GetBoundingBox();

		for (const auto& [Key, Tiles] : SlopePiecesByKey[PieceIndex])
		{
			const UTileTypeVisualData* const* TypeVisualsPtr = TilesVisualSet->TileTypeVisuals.Find(Key.TileType);
			const FTileTypeVisualVariant* Variant = TypeVisualsPtr && *TypeVisualsPtr ? (*TypeVisualsPtr)->FindVisualVariantOrFallback(Key) : nullptr;

			TArray<FTransform> Transforms;
			Transforms.Reserve(Tiles.Num());
			for (const FPlacedTile& Placed : Tiles)
			{
				Transforms.Add(TileCliffs::MakeSlopePieceTransform(Layout, Placed.Tile, InGrid.GetTile(Placed.Tile).Height,
					TileData::GetSlopeFacing(Placed.Slope), MeshBounds, MeshFacing));
			}

			const FString BaseName = FString::Printf(TEXT("Chunk_X%d_Y%d_%s_%s%s"),
				InChunk.X, InChunk.Y, *UEnum::GetValueAsString(Key.TileType), PieceNames[PieceIndex], Key.bWatered ? TEXT("_Watered") : TEXT(""));
			UHierarchicalInstancedStaticMeshComponent* Component = CreateChunkComponent(BaseName, PieceMesh, Variant ? Variant->GetMaterial(Key.bWatered) : nullptr);
			Component->AddInstances(Transforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);

			Chunk.Slopes.Add(Component);
		}
	}

	BuildChunkCliffs(InGrid, InChunk, First, End);
}

void ATileVisualGrid::BuildChunkCliffs(const FTileGrid& InGrid, const FIntPoint& InChunk, const FIntPoint& InFirst, const FIntPoint& InEnd)
{
	TArray<FTileCliffFace> Faces;
	TileCliffs::CollectFaces(InGrid, InFirst, InEnd, Faces);
	if (Faces.IsEmpty()) return;

	UStaticMesh* Mesh = TilesVisualSet->CliffMesh;
	if (!Mesh)
	{
		UE_LOG(LogTileVisualGrid, Warning, TEXT("'%s' Has no cliff mesh, %d cliff sides in chunk (%d, %d) are not drawn"),
			*GetNameSafe(TilesVisualSet), Faces.Num(), InChunk.X, InChunk.Y);
		return;
	}

	const FTileGridLayout& Layout = InGrid.GetLayout();
	const FBox MeshBounds = Mesh->GetBoundingBox();
	const double Thickness = Layout.TileSize * TilesVisualSet->CliffThickness;

	TArray<FTransform> Transforms;
	Transforms.Reserve(Faces.Num());
	for (const FTileCliffFace& Face : Faces)
	{
		Transforms.Add(TileCliffs::MakeWallTransform(Layout, Face, MeshBounds, Thickness));
	}

	UHierarchicalInstancedStaticMeshComponent* Component = CreateChunkComponent(
		FString::Printf(TEXT("Chunk_X%d_Y%d_Cliffs"), InChunk.X, InChunk.Y), Mesh, TilesVisualSet->CliffMaterial);
	Component->AddInstances(Transforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);

	Chunks.FindOrAdd(InChunk).Cliffs = Component;
}

UHierarchicalInstancedStaticMeshComponent* ATileVisualGrid::CreateChunkComponent(const FString& InBaseName, UStaticMesh* InMesh, UMaterialInterface* InMaterial)
{
	// A destroyed component keeps its name until garbage collection, so ask for a unique one.
	const FName UniqueName = MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(), FName(*InBaseName));
	auto* Component = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, UniqueName, RF_Transient);

	// Configure everything before registering, so render and physics state are created once.
	Component->SetMobility(EComponentMobility::Static);
	Component->SetCollisionProfileName(TEXT("BlockAll"), true);
	Component->SetCanEverAffectNavigation(true);
	Component->SetStaticMesh(InMesh);
	if (InMaterial) Component->SetMaterial(0, InMaterial);

	// Instance transforms are in world space: the tile layout, not this actor, decides where tiles are.
	Component->SetUsingAbsoluteLocation(true);
	Component->SetUsingAbsoluteRotation(true);
	Component->SetUsingAbsoluteScale(true);
	Component->SetupAttachment(RootComponent);
	Component->SetRelativeTransform(FTransform::Identity);
	Component->RegisterComponent();
	return Component;
}

void ATileVisualGrid::DestroyChunk(const FIntPoint& InChunk)
{
	FTileChunkVisualData* Chunk = Chunks.Find(InChunk);
	if (!Chunk) return;

	for (const auto& [Key, Component] : Chunk->Meshes)
	{
		if (Component) Component->DestroyComponent();
	}
	if (Chunk->Cliffs) Chunk->Cliffs->DestroyComponent();
	for (UHierarchicalInstancedStaticMeshComponent* Component : Chunk->Slopes)
	{
		if (Component) Component->DestroyComponent();
	}
	Chunks.Remove(InChunk);
}

FBox ATileVisualGrid::GetChunkBounds(const FTileGrid& InGrid, const FIntPoint& InChunk) const
{
	const FTileGridLayout& Layout = InGrid.GetLayout();
	const FIntPoint First(InChunk.X * ChunkSize, InChunk.Y * ChunkSize);
	const FIntPoint Last(FMath::Min(First.X + ChunkSize, Layout.Size.X) - 1, FMath::Min(First.Y + ChunkSize, Layout.Size.Y) - 1);

	uint8 MaxLevel = 0;
	for (int32 Y = First.Y; Y <= Last.Y; ++Y)
	{
		for (int32 X = First.X; X <= Last.X; ++X)
		{
			MaxLevel = FMath::Max(MaxLevel, InGrid.GetTile(FIntPoint(X, Y)).Height);
		}
	}

	const double HalfTile = Layout.TileSize * 0.5;
	FBox Bounds(ForceInit);
	Bounds += Layout.TileCenter(First, 0) - FVector(HalfTile, HalfTile, Layout.LevelHeight);
	Bounds += Layout.TileCenter(Last, MaxLevel) + FVector(HalfTile, HalfTile, Layout.LevelHeight);
	return Bounds;
}

void ATileVisualGrid::FinalizeNavigation(const FBox& InDirtyArea)
{
	// The navigation system reads HISM instances through the cluster tree, which is normally built
	// asynchronously after AddInstances. Finish it now so the geometry is complete when the dirty
	// area is processed.
	for (const auto& [ChunkIndex, Chunk] : Chunks)
	{
		for (const auto& [Key, Component] : Chunk.Meshes)
		{
			if (Component) Component->BuildTreeIfOutdated(/*Async*/ false, /*ForceUpdate*/ false);
		}
		if (Chunk.Cliffs) Chunk.Cliffs->BuildTreeIfOutdated(/*Async*/ false, /*ForceUpdate*/ false);
		for (UHierarchicalInstancedStaticMeshComponent* Component : Chunk.Slopes)
		{
			if (Component) Component->BuildTreeIfOutdated(/*Async*/ false, /*ForceUpdate*/ false);
		}
	}

	if (auto* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		NavSys->AddDirtyArea(InDirtyArea, ENavigationDirtyFlag::All);
		if (NavMeshBoundsVolume.IsValid()) NavSys->OnNavigationBoundsUpdated(NavMeshBoundsVolume.Get());
	}
}

void ATileVisualGrid::HandleTilesChanged(TConstArrayView<FIntPoint> InTiles)
{
	// A tile's piece depends on its neighbours, so a change can alter the chunk next door too.
	TSet<FIntPoint> DirtyChunks;
	for (const FIntPoint& Tile : InTiles)
	{
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				const FIntPoint Neighbor = Tile + FIntPoint(DX, DY);
				if (Neighbor.X < 0 || Neighbor.Y < 0) continue;
				DirtyChunks.Add(FIntPoint(Neighbor.X / ChunkSize, Neighbor.Y / ChunkSize));
			}
		}
	}

	// Only chunks that exist: tiles past the map edge produce chunk indices that were never built.
	TSet<FIntPoint> ExistingDirtyChunks;
	for (const FIntPoint& Chunk : DirtyChunks)
	{
		if (Chunks.Contains(Chunk)) ExistingDirtyChunks.Add(Chunk);
	}

	RebuildChunks(ExistingDirtyChunks);
}

void ATileVisualGrid::HandleWorldCleared()
{
	ClearAllChunks();
}

void ATileVisualGrid::UnbindFromTileWorld()
{
	if (UTileWorldSubsystem* World = TileWorld.Get())
	{
		World->OnTilesChanged.RemoveAll(this);
		World->OnWorldCleared.RemoveAll(this);
	}
	TileWorld.Reset();
}

void ATileVisualGrid::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	NavMeshBoundsVolume = Cast<ANavMeshBoundsVolume>(UGameplayStatics::GetActorOfClass(this, ANavMeshBoundsVolume::StaticClass()));
}

void ATileVisualGrid::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromTileWorld();
	Super::EndPlay(EndPlayReason);
}
