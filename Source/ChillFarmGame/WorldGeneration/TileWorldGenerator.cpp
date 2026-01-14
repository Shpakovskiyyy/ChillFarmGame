#include "TileWorldGenerator.h"

#include "TileGrid.h"
#include "TilesGenerationRuleSet.h"
#include "TileDataGrid.h"
#include "TileNeighborResolver.h"

#include "BiomeTypeData.h"
#include "BiomeTypeDataSet.h"

#include "Kismet/GameplayStatics.h"

#include "Stats/StatsMisc.h"


DEFINE_LOG_CATEGORY(LogTileWorldGenerator);


ATileWorldGenerator::ATileWorldGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	LogicalGrid = CreateDefaultSubobject<UTileDataGrid>(TEXT("Logical Grid"));
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
		TileGrid->ClearAllChunks();

		if (!LogicalGrid)
		{
			LogicalGrid = NewObject<UTileDataGrid>(this, TEXT("Logical Grid"));
		}
		LogicalGrid->ClearAllChunks();
		LogicalGrid->Initialize(FIntPoint(GridSizeX, GridSizeY), GridTileSize);

		FRandomStream RNG(WorldSeed);
		FVector2D NoiseOffset(RNG.FRand() * NoiseOffsetRange, RNG.FRand() * NoiseOffsetRange);
		FVector2D WarpOffset(RNG.FRand() * WarpOffsetRange, RNG.FRand() * WarpOffsetRange);

		TMap<FIntPoint, TMap<FTileVisualKey, TArray<FTransform>>> BatchedChunkVisualizations;
		for (auto CX = 0; CX < GridSizeX; CX += ChunkSize)
		{
			for (auto CY = 0; CY < GridSizeY; CY += ChunkSize)
			{
				ProcessChunk(CX, CY, NoiseOffset, WarpOffset);
			}
		}
		
		//TileGrid->FinalizeNavigation();
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
			auto TileData = GenerateTile(X, Y, NoiseOffset, WarpOffset);
			LogicalGrid->SetTile(X, Y, TileData);
		}
	}
	
	LogicalGrid->RecalculateNeighbors();
	ApplySmoothing();

	TMap<FTileVisualKey, TArray<FTransform>> Batched;
	for (auto X = StartX; X < EndX; X++)
	{
		for (auto Y = StartY; Y < EndY; Y++)
		{
			auto Tile = LogicalGrid->GetTileChecked(X, Y);
			auto ResolvedKey = FTileNeighborResolver::Resolve(Tile->Type, Tile->NeighborMask);

			FVector WorldPos(
				X * GridTileSize,
				Y * GridTileSize,
				0.f
			);

			Batched.FindOrAdd(ResolvedKey).Emplace(
				FRotator(0, ResolvedKey.Rotation, 0),
				WorldPos,
				FVector::OneVector
			);
		}
	}

	TileGrid->CreateChunk(
		FIntPoint(StartX / ChunkSize, StartY / ChunkSize),
		Batched
	);
}

FTileData ATileWorldGenerator::GenerateTile(int32 X, int32 Y, const FVector2D& NoiseOffset, const FVector2D& WarpOffset)
{
	FTileData Result {.TileCoordinate = FIntPoint(X, Y), .bIsOccupied = false};

	auto NoiseContext = GenerateTileNoise(X, Y, NoiseOffset, WarpOffset);

	if (NoiseContext.Height <= WaterLevel)
	{
		Result.Type = ETileType::Water;
		Result.Moisture = 100;
		return Result;
	}
	
	auto FinalBiome = GenerateBiomeData(X, Y, NoiseContext);
	Result.BiomeData = FinalBiome;
	
	const auto RulesSet = FinalBiome && FinalBiome->TilesGenerationRuleSet ? FinalBiome->TilesGenerationRuleSet : DefaultTileGenerationRuleSet;
	auto TileType = RulesSet->ResolveTileType(NoiseContext);
	
	// const auto BorderFactor = GetBiomeBorderFactor(NoiseContext.Height, NoiseContext.Moisture, FinalBiome);
	// if (FinalBiome && BorderFactor > 0.f)
	// {
	// 	const auto BorderNoise = GetBiomeBorderNoise(X, Y);
	// 	if (BorderNoise < BorderFactor * BiomeBorderStrength) TileType = FinalBiome->ResolveBorderTile(NoiseContext.Height, TileType, WaterLevel);
	// }
	//
	// if (FinalBiome && FinalBiome->IsShore(NoiseContext.Height)) TileType = FinalBiome->ShoreTile;

	if (FinalBiome)
	{
		Result.SmoothingOverride = FinalBiome->ResolveSmoothingOverride(TileType);
		FinalBiome->FillRuntimeTileData(Result, NoiseContext.Height, NoiseContext.Moisture);
	}
	else
	{
		Result.SmoothingOverride = ETileType::ETileType_MAX;
	}
	Result.Fertility = FinalBiome->BaseFertility;
	Result.Moisture  = FMath::RoundToInt(NoiseContext.Moisture * 100.f);
	Result.Type = TileType;
	
	return Result;
}

float ATileWorldGenerator::GetBiomeBorderFactor(float Height, float Moisture, const UBiomeTypeData* Biome)
{
	const auto DH = DistanceToRange(Height, Biome->HeightRange);
	const auto DM = DistanceToRange(Moisture, Biome->MoistureRange);

	const auto MinDist = FMath::Min(DH, DM);

	return FMath::Clamp(
		1.f - (MinDist / BiomeBorderWidth),
		0.f,
		1.f
	);
}

float ATileWorldGenerator::GetBiomeBorderNoise(int32 X, int32 Y)
{
	const auto NX = X * BiomeBorderNoiseScale;
	const auto NY = Y * BiomeBorderNoiseScale;

	return (FMath::PerlinNoise2D({ NX, NY }) + 1.f) * 0.5f;
}

void ATileWorldGenerator::ApplySmoothing()
{
	ApplyWaterEdgeSmoothing();
	// TODO Move to biome ???
	ApplyBiomeSmoothing();
	ApplyTileSmoothing();
}

void ATileWorldGenerator::ApplyWaterEdgeSmoothing()
{
	for (auto X = 0; X < GridSizeX; X++)
	{
		for (auto Y = 0; Y < GridSizeY; Y++)
		{
			auto Tile = LogicalGrid->GetTileChecked(X, Y);

			if (Tile->Type == ETileType::Water) continue;

			if (LogicalGrid->HasNeighborOfType(X, Y, ETileType::Water))
			{
				Tile->Type = ETileType::Sand;
			}
		}
	}
}

void ATileWorldGenerator::ApplyTileSmoothing()
{
	TArray<FIntPoint> ToChange;

	for (auto X = 0; X < GridSizeX; X++)
	{
		for (auto Y = 0; Y < GridSizeY; Y++)
		{
			const auto TileData = LogicalGrid->GetTileChecked(X, Y);
			const auto TileType = TileData->Type;

			auto Same = 0;
			auto Total = 0;

			TArray<FIntPoint> Offsets = { {1,0}, {-1,0}, {0,1}, {0,-1} };
			for (const auto& Offset :  Offsets)
			{
				const auto& OffsetTile = LogicalGrid->GetTileChecked(X + Offset.X, Y + Offset.Y);
				if (!OffsetTile) continue;
				Total++;
				if (OffsetTile->Type == TileType) Same++;
			}
			
			if (Same <= 1 && TileData->SmoothingOverride != ETileType::ETileType_MAX)
			{
				LogicalGrid->SetTileType(X, Y, TileData->SmoothingOverride);
				auto ReplacementType = TileData->SmoothingOverride;
				ToChange.Add({ X, Y });
				LogicalGrid->SetTileType(X, Y, ReplacementType);
			}
		}
	}
}

void ATileWorldGenerator::ApplyBiomeSmoothing()
{
	for (auto X = 1; X < GridSizeX - 1; X++)
	{
		for (auto Y = 1; Y < GridSizeY - 1; Y++)
		{
			const auto Tile = LogicalGrid->GetTileChecked(X, Y);
			const auto BiomeTypeData = Tile->BiomeData;
			if (!BiomeTypeData) continue;

			auto bNearDifferentBiome = false;

			for (const auto& Offset : { FIntPoint(1,0), {-1,0}, {0,1}, {0,-1} })
			{
				const auto OffsetTile = LogicalGrid->GetTileChecked(X + Offset.X, Y + Offset.Y);
				
				if (OffsetTile->BiomeData && OffsetTile->BiomeData->BiomeType != BiomeTypeData->BiomeType)
				{
					bNearDifferentBiome = true;
					break;
				}
			}

			if (!bNearDifferentBiome) continue;
			
			switch (BiomeTypeData->BiomeType)
			{
				case EBiomeType::Forest:
					if (Tile->Type == ETileType::Rock) Tile->Type = ETileType::Dirt;
					break;

				case EBiomeType::Plains:
					if (Tile->Type == ETileType::Grass) Tile->Type = ETileType::Dirt;
					break;

				case EBiomeType::Desert:
					Tile->Type = ETileType::Sand;
					break;

				default:
					break;
			}
		}
	}
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

float ATileWorldGenerator::DistanceToRange(float InValue, const FVector2D& Range)
{
	return FMath::Min(
		FMath::Abs(InValue - Range.X),
		FMath::Abs(Range.Y - InValue)
	);
}