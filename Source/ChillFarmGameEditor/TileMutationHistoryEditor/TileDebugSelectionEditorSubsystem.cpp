#include "TileDebugSelectionEditorSubsystem.h"

#include "TileDebugSelection/ViewportMouseClickNotifier.h"
#include "TileDebugRender/TileDebugDrawTile.h"
#include "TileDebugRender/TileDebugHighlightSubsystem.h"
#include "TileVisualGrid.h"
#include "TileWorldSubsystem.h"
#include "Widgets/STileInspectorWindow.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogTileDebugSelectionSubsystem, Log, All)


void UTileDebugSelectionEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (FSlateApplication::IsInitialized())
	{
		ClickProcessor = MakeShared<FViewportMouseClickNotifier>();
		ClickProcessor->OnViewportMouseClick.AddUObject(this, &UTileDebugSelectionEditorSubsystem::HandleViewportMouseClick);
		FSlateApplication::Get().RegisterInputPreProcessor(ClickProcessor);
	}

	FEditorDelegates::OnMapOpened.AddUObject(this, &UTileDebugSelectionEditorSubsystem::HandleMapOpened);
}

void UTileDebugSelectionEditorSubsystem::Deinitialize()
{
	if (ClickProcessor)
	{
		ClickProcessor->OnViewportMouseClick.RemoveAll(this);
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(ClickProcessor);
		ClickProcessor.Reset();
	}

	FEditorDelegates::OnMapOpened.RemoveAll(this);
	Super::Deinitialize();
}

void UTileDebugSelectionEditorSubsystem::SelectTile(UWorld* InWorld, const FIntPoint& InTile)
{
	const UTileWorldSubsystem* TileWorld = InWorld ? InWorld->GetSubsystem<UTileWorldSubsystem>() : nullptr;
	if (!TileWorld) return;

	SelectedTile = InTile;

	if (UTileDebugHighlightSubsystem* Highlight = InWorld->GetSubsystem<UTileDebugHighlightSubsystem>())
	{
		TArray<FTileDebugDrawTile> TilesToDraw;

		if (const FTileData* Tile = TileWorld->GetGrid().FindTile(InTile))
		{
			// The box comes from the layout, so it matches the tile whatever mesh draws it.
			const FTileGridLayout& Layout = TileWorld->GetLayout();
			FTileDebugDrawTile& DrawTile = TilesToDraw.AddDefaulted_GetRef();
			DrawTile.Center = Layout.TileCenter(InTile, Tile->Height);
			DrawTile.Extent = FVector(Layout.TileSize * 0.5, Layout.TileSize * 0.5, 5.0);
			DrawTile.Color = FColor::Yellow;
		}

		Highlight->SetTiles(TilesToDraw);
	}

	OnTileSelected.Broadcast(SelectedTile);
}

void UTileDebugSelectionEditorSubsystem::HandleViewportMouseClick(const FHitResult& InHitResult)
{
	// Only clicks on the tile map select tiles; clicks on other actors keep normal editor behaviour.
	const AActor* HitActor = InHitResult.GetActor();
	if (!HitActor || !HitActor->IsA<ATileVisualGrid>()) return;

	UWorld* World = HitActor->GetWorld();
	const UTileWorldSubsystem* TileWorld = World ? World->GetSubsystem<UTileWorldSubsystem>() : nullptr;
	if (!TileWorld || !TileWorld->HasTiles()) return;

	// Step slightly into the surface that was hit. On a tile top that is the tile itself; on the side
	// of a cliff it is the higher tile the cliff belongs to, not the one in front of it.
	const FVector PointInsideSurface = InHitResult.ImpactPoint - InHitResult.ImpactNormal * 1.0;

	FIntPoint Coordinates;
	if (!TileWorld->FindTileAt(PointInsideSurface, &Coordinates)) return;

	SelectTile(World, Coordinates);

	// Open the inspector on a viewport click only, and only when it is closed: invoking an open tab
	// focuses it, which would take keyboard focus away from the viewport on every click. A freshly
	// spawned inspector reads the current selection itself, so the order with SelectTile does not matter.
	if (!FGlobalTabmanager::Get()->FindExistingLiveTab(STileInspectorWindow::TabName).IsValid())
	{
		FGlobalTabmanager::Get()->TryInvokeTab(STileInspectorWindow::TabName);
	}
}

void UTileDebugSelectionEditorSubsystem::HandleMapOpened(const FString& InFilename, bool bAsTemplate)
{
	// Coordinates of the previous map mean nothing in the new one.
	SelectedTile = FIntPoint::NoneValue;
	OnTileSelected.Broadcast(SelectedTile);
}
