#include "TileDebugRender/TileDebugHighlightSubsystem.h"
#include "TileDebugRender/TileDebugHighlightComponent.h"

#include "Editor.h"
#include "Engine/World.h"


void UTileDebugHighlightSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FEditorDelegates::OnMapOpened.AddUObject(this, &UTileDebugHighlightSubsystem::HandleOnMapOpened);
}

bool UTileDebugHighlightSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Outer && Outer->GetWorld() && Outer->GetWorld()->IsEditorWorld();
}

void UTileDebugHighlightSubsystem::UpdateTiles(const TArray<FTileDebugDrawTile>& InTiles)
{
	RenderComponent->AddHighlightedTiles(InTiles);
}

void UTileDebugHighlightSubsystem::SetSelectedTile(int32 InSelectedTileIndex)
{
	RenderComponent->AddHighlightedTile(InSelectedTileIndex);
}

void UTileDebugHighlightSubsystem::HandleOnMapOpened(const FString&, bool)
{
	auto World = GEditor->GetEditorWorldContext().World();
	check(World);

	DummyActor = World->SpawnActor<AActor>();
	DummyActor->SetFlags(RF_Transient);
	DummyActor->SetActorHiddenInGame(false);

	RenderComponent = NewObject<UTileDebugHighlightComponent>(DummyActor);
	RenderComponent->RegisterComponent();
	DummyActor->SetRootComponent(RenderComponent);
}