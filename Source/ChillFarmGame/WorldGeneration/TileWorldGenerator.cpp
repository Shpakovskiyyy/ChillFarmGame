#include "TileWorldGenerator.h"

#include "TileVisualGrid.h"
#include "TilesGenerationRuleSet.h"
#include "TileDataGrid.h"
#include "TileNeighborResolver.h"

#include "BiomeTypeData.h"
#include "BiomeTypeDataSet.h"
#include "TileWorldPostProcessor.h"

#include "Kismet/GameplayStatics.h"

#include "Stats/StatsMisc.h"
#include "TileMutationHistory/TileMutationHelper.h"


DEFINE_LOG_CATEGORY(LogTileWorldGenerator);


ATileWorldGenerator::ATileWorldGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	DataGrid = CreateDefaultSubobject<UTileDataGrid>(TEXT("Logical Grid"));
}

void ATileWorldGenerator::GenerateWorld()
{
	auto TimeElapsed = 0.0;
	{
		SCOPE_SECONDS_COUNTER(TimeElapsed);

		if (!DataGrid)
		{
			DataGrid = NewObject<UTileDataGrid>(this, TEXT("Logical Grid"));
		}
		DataGrid->ClearAllChunks();
		
		if (!TileGrid)
		{
			TileGrid = Cast<ATileVisualGrid>(UGameplayStatics::GetActorOfClass(this, ATileVisualGrid::StaticClass()));
		}
		TileGrid->Initialize(ChunkSize, GridTileSize);
		
		DataGrid->Initialize(FIntPoint(GridSizeX, GridSizeY), GridTileSize);

		FRandomStream RNG(WorldSeed);
		FVector2D NoiseOffset(RNG.FRand() * NoiseOffsetRange, RNG.FRand() * NoiseOffsetRange);
		FVector2D WarpOffset(RNG.FRand() * WarpOffsetRange, RNG.FRand() * WarpOffsetRange);
		
		for (auto CX = 0; CX < GridSizeX; CX += ChunkSize)
		{
			for (auto CY = 0; CY < GridSizeY; CY += ChunkSize)
			{
				ProcessChunk(CX, CY, NoiseOffset, WarpOffset);
			}
		}
		
		DataGrid->RecalculateNeighbors();
	
		if (!PostProcessor) PostProcessor = NewObject<UTileWorldPostProcessor>(this, TEXT("WorldPostProcessor"));
		PostProcessor->ProcessWorld(DataGrid);

		for (int32 CX = 0; CX < GridSizeX; CX += ChunkSize)
		{
			for (int32 CY = 0; CY < GridSizeY; CY += ChunkSize)
			{
				VisualizeChunk(CX, CY);
			}
		}
	}
	
	UE_LOG(LogTileWorldGenerator, Display, TEXT("WORLD GENERATED IN %f sec"), TimeElapsed);
	OnTerrainGenerated.Broadcast();
}

void ATileWorldGenerator::CleanGeneratedWorld()
{
	if (!TileGrid) return;
	TileGrid->ClearAllChunks();
}

FOnTerrainGenerated& ATileWorldGenerator::GetOnTerrainGenerated() { return OnTerrainGenerated; }

void ATileWorldGenerator::ProcessChunk(int32 StartX, int32 StartY, const FVector2D& NoiseOffset, const FVector2D& WarpOffset)
{
	const auto EndX = FMath::Min(StartX + ChunkSize, GridSizeX);
	const auto EndY = FMath::Min(StartY + ChunkSize, GridSizeY);

	for (auto X = StartX; X < EndX; X++)
	{
		for (auto Y = StartY; Y < EndY; Y++)
		{
			GenerateTile(X, Y, NoiseOffset, WarpOffset);
			//DataGrid->SetTile(X, Y, TileData);
		}
	}
}

void ATileWorldGenerator::GenerateTile(int32 X, int32 Y, const FVector2D& NoiseOffset, const FVector2D& WarpOffset)
{
	auto NoiseContext = GenerateTileNoise(X, Y, NoiseOffset, WarpOffset);

	FTileMutationHelper TileMutation;
	// TODO Mutation: Actual phases and reasons
	auto MutationContext = FTileMutationContext::Make({X, Y}, ETileMutationSource::WorldGeneration, nullptr, this, TEXT("Base World Generation"), TEXT("Tile Generation"));
	if (NoiseContext.Height <= WaterLevel)
	{
		TileMutation.SetType(ETileType::Water);
		TileMutation.SetMoisture(100);
		TileMutation.SetBiomeTypeData(GetWaterBiomeData());
		MutationContext.BiomeType = GetWaterBiomeData();
		DataGrid->MutateTile(MutationContext, TileMutation);

		// TODO
		//WaterBiome->FillRuntimeTileData(Result, NoiseContext.Height, NoiseContext.Moisture);
		return;
	}
	
	auto FinalBiome = GenerateBiomeData(X, Y, NoiseContext);
	
	const auto RulesSet = FinalBiome && FinalBiome->TilesGenerationRuleSet ? FinalBiome->TilesGenerationRuleSet : DefaultTileGenerationRuleSet;
	auto TileType = RulesSet->ResolveTileType(NoiseContext);
	
	TileMutation.SetBiomeTypeData(FinalBiome);
	TileMutation.SetFertility(FinalBiome->BaseFertility);
	TileMutation.SetMoisture(FMath::RoundToInt(NoiseContext.Moisture * 100.f));
	TileMutation.SetType(TileType);
	
	MutationContext.BiomeType = FinalBiome;
	DataGrid->MutateTile(MutationContext, TileMutation);
}

void ATileWorldGenerator::VisualizeChunk(int32 ChunkX, int32 ChunkY)
{
	TMap<FTileVisualKey, FTileInstancesBatch> Batched;
	
	const auto EndX   = FMath::Min(ChunkX + ChunkSize, GridSizeX);
	const auto EndY   = FMath::Min(ChunkY + ChunkSize, GridSizeY);

	for (auto X = ChunkX; X < EndX; X++)
	{
		for (auto Y = ChunkY; Y < EndY; Y++)
		{
			const auto& Tile = DataGrid->GetTileChecked(X, Y);

			const auto ResolvedKey =
				FTileNeighborResolver::Resolve(Tile->GetType(), Tile->GetNeighborMask());

			FVector LocalPos(
				(X - ChunkX) * GridTileSize,
				(Y - ChunkY) * GridTileSize,
				0.f
			);

			Batched.FindOrAdd(ResolvedKey).Transforms.Emplace(
				FRotator(0.f, ResolvedKey.Rotation, 0.f),
				LocalPos,
				FVector::OneVector
			);

			Batched.FindOrAdd(ResolvedKey).Coordinates.Emplace( FIntPoint(X, Y) );
		}
	}

	TileGrid->CreateChunk(
		FIntPoint(ChunkX / ChunkSize, ChunkY / ChunkSize),
		Batched
	);
}

UBiomeTypeData* ATileWorldGenerator::GetWaterBiomeData()
{
	for (const auto BiomeData : BiomeDataSet->BiomeData) if (BiomeData->BiomeType == EBiomeType::Water) return BiomeData;
	return nullptr;
}

STileNoiseContext ATileWorldGenerator::GenerateTileNoise(int32 X, int32 Y, const FVector2D& NoiseOffset, const FVector2D& WarpOffset)
{
	const auto PX = X * NoiseScale + NoiseOffset.X;
	const auto PY = Y * NoiseScale + NoiseOffset.Y;
	
	const auto WarpX = FMath::PerlinNoise2D({ PX + WarpOffset.X, PY + WarpOffset.Y });
	const auto WarpY = FMath::PerlinNoise2D({ PX - WarpOffset.Y, PY + WarpOffset.X });

	const FVector2D Warped(
		PX + WarpX * WarpStrength,
		PY + WarpY * WarpStrength
	);

	const auto HeightNoise = FMath::PerlinNoise2D(Warped) * 0.5f + 0.5f;
	const auto MoistureNoise = FMath::PerlinNoise2D(Warped + FVector2D(1000.f)) * 0.5f + 0.5f;
	const auto SecondaryNoise = GetSecondaryNoise(X, Y);

	return STileNoiseContext{.Height = HeightNoise, .Moisture = MoistureNoise, .Secondary = SecondaryNoise};
}

UBiomeTypeData* ATileWorldGenerator::GenerateBiomeData(int32 InX, int32 InY, const STileNoiseContext& NoiseContext)
{
	const auto BiomeBlend = BiomeDataSet->ResolveBiomeBlend(NoiseContext.Height, NoiseContext.Moisture);

	const auto BiomeA = BiomeBlend.Primary;
	const auto BiomeB = BiomeBlend.Secondary;
	
	const auto BlendNoise = GetSecondaryNoise(InX, InY);
	
	return (BiomeB && BlendNoise < BiomeBlend.BlendAlpha) ? BiomeB : BiomeA;
}

float ATileWorldGenerator::GetSecondaryNoise(int32 X, int32 Y)
{
	const auto SX = X * SecondaryNoiseScale;
	const auto SY = Y * SecondaryNoiseScale;

	const auto Noise = FMath::PerlinNoise2D({ SX, SY });
	return (Noise + 1.f) * 0.5f * SecondaryNoiseStrength; // 0..1
}