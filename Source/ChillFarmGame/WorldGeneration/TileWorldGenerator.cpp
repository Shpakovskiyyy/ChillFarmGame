#include "TileWorldGenerator.h"
#include "TileGrid.h"
#include "AI/NavigationSystemBase.h"

#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogTileWorldGenerator);

ATileWorldGenerator::ATileWorldGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATileWorldGenerator::GenerateWorld()
{
	auto TimeElapsed = 0.0;
	{
		SCOPE_SECONDS_COUNTER(TimeElapsed);

		if (!TileGrid)
		{
			TileGrid = Cast<ATileGrid>(UGameplayStatics::GetActorOfClass(this, ATileGrid::StaticClass()));
		}

		TileGrid->ClearAllInstances();
		TileGrid->SetupMeshComponents();
		
		FRandomStream RNG(WorldSeed);
		FVector2D NoiseOffset(RNG.FRand() * NoiseOffsetRange, RNG.FRand() * NoiseOffsetRange);
		FVector2D WarpOffset(RNG.FRand() * NoiseOffsetRange, RNG.FRand() * WarpOffsetRange);
		
		for (auto ChunkX = 0; ChunkX < GridSizeX; ChunkX += ChunkSize)
		{
			for (auto ChunkY = 0; ChunkY < GridSizeY; ChunkY += ChunkSize)
			{
				ProcessChunk(ChunkX, ChunkY, NoiseOffset, WarpOffset);
			}
		}
	}
	
	if (OnTerrainGenerated.IsBound()) OnTerrainGenerated.Broadcast();
	UE_LOG(LogTileWorldGenerator, Display, TEXT("WORLD GENERATED IN %f sec"), TimeElapsed);
}

void ATileWorldGenerator::CleanGeneratedWorld()
{
	if (!TileGrid) return;
	TileGrid->ClearAllInstances();
}

FOnTerrainGenerated& ATileWorldGenerator::GetOnTerrainGenerated()
{
	return OnTerrainGenerated;
}

void ATileWorldGenerator::ProcessChunk(int32 StartX, int32 StartY, const FVector2D& NoiseOffset, const FVector2D& WarpOffset)
{
	auto EndX = FMath::Min(StartX + ChunkSize, GridSizeX);
	auto EndY = FMath::Min(StartY + ChunkSize, GridSizeY);
	
	TMap<ETileType, TArray<FTransform>> RowBuffers;
	for (auto TileType : TEnumRange<ETileType>()) RowBuffers.Add(TileType, TArray<FTransform>());
	
	auto TileMeshScaleFactor = GridTileSize / DefaultTileMeshSize;
	ParallelFor(EndX - StartX, [&](int32 LocalX)
	{
		auto X = StartX + LocalX;
		
		TMap<ETileType, TArray<FTransform>> LocalBuffers;
		for (ETileType T : TEnumRange<ETileType>()) LocalBuffers.Add(T, TArray<FTransform>());

		for (auto Y = StartY; Y < EndY; Y++)
		{
			auto TileType = GenerateTile(X, Y, NoiseOffset, WarpOffset);

			FVector WorldPos(
				X * GridTileSize,
				Y * GridTileSize,
				0.f
			);

			LocalBuffers[TileType].Add(FTransform(FRotator::ZeroRotator, WorldPos, FVector(TileMeshScaleFactor, TileMeshScaleFactor, 1.)));
		}
		
		for (auto& [TileType, TransformBuffer] : LocalBuffers)
		{
			FScopeLock Lock(&TileGridMutex);
			RowBuffers[TileType].Append(TransformBuffer);
		}
	});
	
	for (const auto& [TileType, Transforms] : RowBuffers)
	{
		if (Transforms.Num() > 0) TileGrid->AddInstances(TileType, Transforms);
	}
}

ETileType ATileWorldGenerator::GenerateTile(int32 X, int32 Y, const FVector2D& NoiseOffset, const FVector2D& WarpOffset)
{
	auto PX = X * NoiseScale + NoiseOffset.X * 0.01f;
	auto PY = Y * NoiseScale + NoiseOffset.Y * 0.01f;
	
	auto WarpX = FMath::PerlinNoise2D({ PX + WarpOffset.X, PY + WarpOffset.Y });
	auto WarpY = FMath::PerlinNoise2D({ PX - WarpOffset.Y, PY + WarpOffset.X });

	FVector2D Warped(
		PX + WarpX * WarpStrength,
		PY + WarpY * WarpStrength
	);
	
	auto Noise = FMath::PerlinNoise2D(Warped);
	auto NoiseNormalized = (Noise + 1.f) * 0.5f;

	if (NoiseNormalized < 0.35f) return ETileType::Water;
	if (NoiseNormalized < 0.55f) return ETileType::Grass;
	return ETileType::Farmable;
}

void ATileWorldGenerator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	GenerateWorld();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
