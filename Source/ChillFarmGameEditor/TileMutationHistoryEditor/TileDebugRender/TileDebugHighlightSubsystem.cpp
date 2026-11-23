#include "TileDebugRender/TileDebugHighlightSubsystem.h"
#include "TileDebugRender/TileDebugHighlightComponent.h"

#include "Engine/World.h"


bool UTileDebugHighlightSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Outer ? Outer->GetWorld() : nullptr;
	return World && World->IsEditorWorld() && !World->IsGameWorld();
}

void UTileDebugHighlightSubsystem::Deinitialize()
{
	if (RenderComponent && RenderComponent->IsRegistered()) RenderComponent->UnregisterComponent();
	RenderComponent = nullptr;
	Super::Deinitialize();
}

void UTileDebugHighlightSubsystem::SetTiles(const TArray<FTileDebugDrawTile>& InTiles)
{
	if (UTileDebugHighlightComponent* Component = GetOrCreateRenderComponent())
	{
		Component->SetTiles(InTiles);
	}
}

UTileDebugHighlightComponent* UTileDebugHighlightSubsystem::GetOrCreateRenderComponent()
{
	if (RenderComponent) return RenderComponent;

	UWorld* World = GetWorld();
	if (!World) return nullptr;

	// Created on first use in this subsystem's own world, and registered without an owning actor:
	// nothing appears in the outliner and nothing is saved with the level. It used to be spawned on
	// OnMapOpened into whatever world the editor had, which left new unsaved levels without one.
	RenderComponent = NewObject<UTileDebugHighlightComponent>(this, NAME_None, RF_Transient);
	RenderComponent->RegisterComponentWithWorld(World);
	return RenderComponent;
}
