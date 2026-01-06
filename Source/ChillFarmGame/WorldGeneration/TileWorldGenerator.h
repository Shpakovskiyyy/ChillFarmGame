#pragma once

#include "CoreMinimal.h"
#include "TileBase.h"
#include "GameFramework/Actor.h"
#include "TileWorldGenerator.generated.h"

class ATileGrid;

DECLARE_LOG_CATEGORY_EXTERN(LogTileWorldGenerator, Log, All);

DECLARE_MULTICAST_DELEGATE(FOnTerrainGenerated);

UCLASS()
class CHILLFARMGAME_API ATileWorldGenerator : public AActor
{
	GENERATED_BODY()

public:
	ATileWorldGenerator();

public:
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Generation")
	void GenerateWorld();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Generation")
	void CleanGeneratedWorld();

public:
	FOnTerrainGenerated& GetOnTerrainGenerated();

protected:
	void ProcessChunk(int32 StartX, int32 StartY, const FVector2D& NoiseOffset, const FVector2D& WarpOffset);
	ETileType GenerateTile(int32 X, int32 Y, const FVector2D& NoiseOffset, const FVector2D& WarpOffset);

protected:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

protected:
	/* Grid settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
	int32 WorldSeed = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
	int32 GridSizeX = 200;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
	int32 GridSizeY = 200;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
	float GridTileSize = 200.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Grid Settings")
	int32 DefaultTileMeshSize = 100;
	/* Grid settings */

	/* Generation Settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation Settings")
	int32 ChunkSize = 64;

	
	/* Generation Settings */

	/* Noise Settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Settings")
	float NoiseScale = 0.025f; // Perlin sampling scale

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Settings")
	float WarpStrength = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Settings")
	float NoiseOffsetRange = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Settings")
	float WarpOffsetRange = 2000.f;

	// UPROPERTY(EditAnywhere, Category = "Noise Settings")
	// float DirtDistributionFactor = 0.35f;
	//
	// UPROPERTY(EditAnywhere, Category = "Noise Settings")
	// float DirtDistributionFactor = 0.35f;
	//
	// UPROPERTY(EditAnywhere, Category = "Noise Settings")
	// float DirtDistributionFactor = 0.35f;
	/* Noise Settings */

protected:
	FOnTerrainGenerated OnTerrainGenerated;

private:
	FCriticalSection TileGridMutex;

private:
	FVector2D SeedOffset = FVector2D::ZeroVector;
	
private:
	UPROPERTY()
	ATileGrid* TileGrid = nullptr;
	
};