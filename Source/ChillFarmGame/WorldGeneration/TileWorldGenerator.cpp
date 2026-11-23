#include "TileWorldGenerator.h"

#include "TileGrid.h"
#include "TileVisualGrid.h"
#include "TileWorldSubsystem.h"
#include "BiomeTypeDataSet.h"
#include "TilesGenerationRuleSet.h"
#include "Generation/TileGenerationContext.h"
#include "Generation/TileGenerationPass.h"
#include "Generation/TileGenerationPasses.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Stats/StatsMisc.h"


DEFINE_LOG_CATEGORY(LogTileWorldGenerator);


ATileWorldGenerator::ATileWorldGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// The default pipeline. Every generator placed on a level gets its own copies, editable inline.
	Passes.Add(CreateDefaultSubobject<UTerrainNoisePass>(TEXT("TerrainPass")));
	Passes.Add(CreateDefaultSubobject<UBiomeAssignmentPass>(TEXT("BiomePass")));
	Passes.Add(CreateDefaultSubobject<UTileTypePass>(TEXT("TileTypePass")));
	Passes.Add(CreateDefaultSubobject<ULevelConstraintPass>(TEXT("TerracePass")));
	Passes.Add(CreateDefaultSubobject<UShorePass>(TEXT("ShorePass")));
	Passes.Add(CreateDefaultSubobject<UTileSmoothingPass>(TEXT("SmoothingPass")));
	Passes.Add(CreateDefaultSubobject<UFertilityPass>(TEXT("FertilityPass")));
	Passes.Add(CreateDefaultSubobject<USlopePass>(TEXT("SlopePass")));
	Passes.Add(CreateDefaultSubobject<URampPass>(TEXT("RampPass")));
}

FTileGridLayout ATileWorldGenerator::MakeLayout() const
{
	FTileGridLayout Layout;
	Layout.Size = FIntPoint(FMath::Max(1, GridSizeX), FMath::Max(1, GridSizeY));
	Layout.TileSize = FMath::Max(1.f, GridTileSize);
	Layout.LevelHeight = FMath::Max(1.f, LevelHeight);
	Layout.Origin = GridOrigin;
	return Layout;
}

void ATileWorldGenerator::GenerateWorld()
{
	UWorld* World = GetWorld();
	UTileWorldSubsystem* TileWorld = World ? World->GetSubsystem<UTileWorldSubsystem>() : nullptr;
	if (!TileWorld)
	{
		UE_LOG(LogTileWorldGenerator, Error, TEXT("'%s' Cannot generate: the world has no UTileWorldSubsystem"), *GetNameSafe(this));
		return;
	}

	ATileVisualGrid* Visuals = FindVisualGrid();
	if (!Visuals)
	{
		UE_LOG(LogTileWorldGenerator, Warning, TEXT("'%s': There is no ATileVisualGrid on the level, the map will not be drawn"), *GetNameSafe(this));
	}

	double TimeElapsed = 0.0;
	{
		SCOPE_SECONDS_COUNTER(TimeElapsed);

		// Listeners (visuals, the editor's history) drop what they know about the previous map.
		TileWorld->ClearWorld();

		FTileGrid Grid(MakeLayout());
		FTileGenerationContext Context(Grid, WorldSeed);
		Context.Biomes = BiomeDataSet;
		Context.DefaultRuleSet = DefaultTileGenerationRuleSet;
		Context.Instigator = this;

		// Only pay for describing every change when a tool records them.
		if (TileWorld->WantsChangeNotifications())
		{
			TWeakObjectPtr<UTileWorldSubsystem> WeakTileWorld = TileWorld;
			Context.OnChange = [WeakTileWorld](const FTileChange& Change)
			{
				if (const UTileWorldSubsystem* Target = WeakTileWorld.Get()) Target->NotifyExternalChange(Change);
			};
		}

		TArray<const UTileGenerationPass*> PassList;
		for (const UTileGenerationPass* Pass : Passes) PassList.Add(Pass);

		if (!TileGeneration::RunPasses(PassList, Context))
		{
			UE_LOG(LogTileWorldGenerator, Error, TEXT("'%s': Generation stopped, see the errors above"), *GetNameSafe(this));
			return;
		}

		TileWorld->SetGrid(MoveTemp(Grid));

		if (Visuals) Visuals->RebuildAll(*TileWorld, ChunkSize);
	}

	UE_LOG(LogTileWorldGenerator, Display, TEXT("World generated in %.3f s (%d x %d tiles, seed %d)"), TimeElapsed, GridSizeX, GridSizeY, WorldSeed);
	OnTerrainGenerated.Broadcast();
}

void ATileWorldGenerator::CleanGeneratedWorld()
{
	if (UWorld* World = GetWorld())
	{
		if (UTileWorldSubsystem* TileWorld = World->GetSubsystem<UTileWorldSubsystem>())
		{
			// The visual grid listens to this and removes its chunks.
			TileWorld->ClearWorld();
		}
	}

	if (ATileVisualGrid* Visuals = FindVisualGrid()) Visuals->ClearAllChunks();
}

ATileVisualGrid* ATileWorldGenerator::FindVisualGrid()
{
	if (!VisualGrid)
	{
		VisualGrid = Cast<ATileVisualGrid>(UGameplayStatics::GetActorOfClass(this, ATileVisualGrid::StaticClass()));
	}
	return VisualGrid;
}
