#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TileGridLayout.h"
#include "TileWorldGenerator.generated.h"

class ATileVisualGrid;
class FTileGrid;
class UBiomeTypeDataSet;
class UTileGenerationPass;
class UTilesGenerationRuleSet;
class UTileWorldSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogTileWorldGenerator, Log, All);

DECLARE_MULTICAST_DELEGATE(FOnTerrainGenerated);


// Builds the tile map of the level from a seed.
//
// GenerateWorld runs the passes below in order on a fresh grid, hands the result to the world's
// UTileWorldSubsystem and asks the ATileVisualGrid to draw it. The same seed and settings always
// give the same map.
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
	FOnTerrainGenerated& GetOnTerrainGenerated() { return OnTerrainGenerated; }

	// The layout the next generation will use.
	FTileGridLayout MakeLayout() const;

	// Regenerate when the level is opened in the editor (the map itself is not saved, only the seed).
	bool ShouldGenerateOnEditorLoad() const { return bGenerateWhenLevelOpens; }

private:
	ATileVisualGrid* FindVisualGrid();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	int32 WorldSeed = 1;

	// Regenerate automatically when this level is opened in the editor.
	UPROPERTY(EditAnywhere, Category = "Generation")
	bool bGenerateWhenLevelOpens = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1", ClampMax = "2048"))
	int32 GridSizeX = 128;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1", ClampMax = "2048"))
	int32 GridSizeY = 128;

	// Edge length of one tile in world units.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1"))
	float GridTileSize = 100.f;

	// World height of one elevation level.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1"))
	float LevelHeight = 100.f;

	// World position of the outer corner of tile (0, 0).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid")
	FVector GridOrigin = FVector::ZeroVector;

	// Tiles per side of one visual chunk. Smaller chunks redraw faster after a change, larger ones mean fewer components.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "8", ClampMax = "256"))
	int32 ChunkSize = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	UBiomeTypeDataSet* BiomeDataSet = nullptr;

	// Used for biomes that have no rule set of their own.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	UTilesGenerationRuleSet* DefaultTileGenerationRuleSet = nullptr;

	// Run in order. Each pass builds on the results of the ones before it.
	UPROPERTY(EditAnywhere, Instanced, Category = "Passes")
	TArray<TObjectPtr<UTileGenerationPass>> Passes;

private:
	UPROPERTY(Transient)
	TObjectPtr<ATileVisualGrid> VisualGrid = nullptr;

	FOnTerrainGenerated OnTerrainGenerated;
};
