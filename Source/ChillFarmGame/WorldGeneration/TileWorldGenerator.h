#pragma once

#include "CoreMinimal.h"
#include "TileData.h"
#include "TileNoiseContext.h"
#include "TileVisualKey.h"
#include "GameFramework/Actor.h"
#include "TileWorldGenerator.generated.h"

class UTileWorldPostProcessor;
class UTilesGenerationRuleSet;
class UTileDataGrid;
class ATileVisualGrid;
class UBiomeTypeData;
class UBiomeTypeDataSet;

DECLARE_LOG_CATEGORY_EXTERN(LogTileWorldGenerator, Log, All);

DECLARE_MULTICAST_DELEGATE(FOnTerrainGenerated);

UCLASS()
class CHILLFARMGAME_API ATileWorldGenerator : public AActor
{
	GENERATED_BODY()

public:
	ATileWorldGenerator();

public:
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Generation Settings | Generation")
	void GenerateWorld();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Generation Settings | Generation")
	void CleanGeneratedWorld();

public:
	FOnTerrainGenerated& GetOnTerrainGenerated();

protected:
	void ProcessChunk(int32 StartX, int32 StartY, const FVector2D& NoiseOffset, const FVector2D& WarpOffset);
	void GenerateTile(int32 X, int32 Y, const FVector2D& NoiseOffset, const FVector2D& WarpOffset);

protected:
	void VisualizeChunk(int32 ChunkX, int32 ChunkY);

protected:
	UBiomeTypeData* GetWaterBiomeData();
	STileNoiseContext GenerateTileNoise(int32 X, int32 Y, const FVector2D& NoiseOffset, const FVector2D& WarpOffset);
	UBiomeTypeData* GenerateBiomeData(int32 InX, int32 InY, const STileNoiseContext& NoiseContext);
	float GetSecondaryNoise(int32 X, int32 Y);

private:
	// TODO Maybe move to some utility
	float DistanceToRange(float InValue, const FVector2D& Range);

protected:
	/* Grid settings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings | Grid Settings")
	int32 WorldSeed = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings | Grid Settings")
	int32 ChunkSize = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings | Grid Settings")
	int32 GridSizeX = 128;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings | Grid Settings")
	int32 GridSizeY = 128;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings | Grid Settings")
	float GridTileSize = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Generation Settings | Grid Settings")
	int32 DefaultTileMeshSize = 100;

	UPROPERTY(EditDefaultsOnly)
	float BiomeBorderWidth = 0.08f;

	UPROPERTY(EditDefaultsOnly)
	float BiomeBorderNoiseScale = 0.12f;

	UPROPERTY(EditDefaultsOnly)
	float BiomeBorderStrength = 0.3f;
	/* Grid settings */

protected:
	/* Noise Settings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings | Noise Settings")
	float NoiseScale = 0.025f; // Perlin sampling scale

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings | Noise Settings")
	float HeightNoiseScale = 0.005f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings | Noise Settings")
	float MoistureNoiseScale = 0.005f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings | Noise Settings")
	float SecondaryNoiseScale = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings | Noise Settings")
	float SecondaryNoiseStrength = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings | Noise Settings")
	float WarpStrength = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings | Noise Settings")
	float NoiseOffsetRange = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings | Noise Settings")
	float WarpOffsetRange = 2000.f;
	/* Noise Settings */

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings | World Generation Settings")
	float WaterLevel = 0.3f;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings | World Generation")
	UTilesGenerationRuleSet* DefaultTileGenerationRuleSet;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation Settings | World Generation")
	UBiomeTypeDataSet* BiomeDataSet;

private:
	UPROPERTY()
	UTileDataGrid* DataGrid = nullptr;

	UPROPERTY()
	UTileWorldPostProcessor* PostProcessor = nullptr;
	
	UPROPERTY()
	ATileVisualGrid* TileGrid = nullptr;

private:
	FOnTerrainGenerated OnTerrainGenerated;
	
};