#include "TileDebugSelectionEditorSubsystem.h"

#include "TileDebugSelection/ViewportMouseClickNotifier.h"
#include "TileDebugRender/TileDebugDrawTile.h"
#include "TileDebugRender/TileDebugHighlightSubsystem.h"
#include "TileVisualGrid.h"

#include "Editor.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogTileDebugSelectionSubsystem, All, All)


void UTileDebugSelectionEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTileDebugSelectionSubsystem, Log, TEXT("TileDebugSelectionSubsystem initialized"))

	if (!ClickProcessor)
	{
		ClickProcessor = MakeShared<FViewportMouseClickNotifier>();
		FSlateApplication::Get().RegisterInputPreProcessor(ClickProcessor);
	}

	ClickProcessor->OnViewportMouseClick.AddUObject(this, &UTileDebugSelectionEditorSubsystem::HandleViewportMouseClick);
	FEditorDelegates::OnMapOpened.AddUObject(this, &UTileDebugSelectionEditorSubsystem::HandleOnMapOpened);
}

void UTileDebugSelectionEditorSubsystem::Deinitialize()
{
	Super::Deinitialize();
	UE_LOG(LogTileDebugSelectionSubsystem, Log, TEXT("TileDebugSelectionSubsystem deinitialized"))

	if(ClickProcessor) ClickProcessor->OnViewportMouseClick.RemoveAll(this);
	FSlateApplication::Get().UnregisterInputPreProcessor(ClickProcessor);

	FEditorDelegates::OnMapOpened.RemoveAll(this);
}

FIntPoint UTileDebugSelectionEditorSubsystem::GetSelectedTile() const
{
	return SelectedTile;
}

void UTileDebugSelectionEditorSubsystem::HandleOnMapOpened(const FString&, bool)
{
	auto World = GEditor->GetEditorWorldContext().World();
	auto FoundActor = UGameplayStatics::GetActorOfClass(World, ATileVisualGrid::StaticClass());
	if (!FoundActor) return;
	TileVisualGrid = Cast<ATileVisualGrid>(FoundActor);
}

// TODO Cleanup
void UTileDebugSelectionEditorSubsystem::HandleViewportMouseClick(const FHitResult& InHitResult)
{
	if (!TileVisualGrid.Get()) return;
	if (!InHitResult.Component.IsValid()) return;
	
	auto HISMC = Cast<UHierarchicalInstancedStaticMeshComponent>(InHitResult.Component.Get());
	if (!HISMC) return;
	
	FTransform TileTransform;
	HISMC->GetInstanceTransform(InHitResult.Item, TileTransform, true);
	const auto TileLocation = TileTransform.GetLocation();
	
	auto World = GEditor->GetEditorWorldContext().World();
	if (!World) return;
	
	FTileDebugDrawTile DebugTile;
	DebugTile.Center = TileLocation;
	
	const auto& BoxExtent = HISMC->GetStaticMesh() ? HISMC->GetStaticMesh()->GetBoundingBox().GetExtent() : FVector(50.f);
	DebugTile.Extent = BoxExtent;
	DebugTile.Color = FColor::Red;

	if (auto HighlightSubsystem = GWorld->GetSubsystem<UTileDebugHighlightSubsystem>())
	{
		TArray<FTileDebugDrawTile> TilesToHighlight;
		TilesToHighlight.Add(DebugTile);

		HighlightSubsystem->UpdateTiles(TilesToHighlight);
		HighlightSubsystem->SetSelectedTile(0);
	}
	
	const auto& TileCoordinates = TileVisualGrid->GetTileCoordinatesFromHISMC(HISMC, InHitResult.Item);
	OnTileSelected.Broadcast(TileCoordinates);
}