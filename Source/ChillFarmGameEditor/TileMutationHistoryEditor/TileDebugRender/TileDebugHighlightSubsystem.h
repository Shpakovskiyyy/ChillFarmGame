#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "TileDebugRender/TileDebugDrawTile.h"

#include "TileDebugHighlightSubsystem.generated.h"

class UTileDebugHighlightComponent;


// Draws debug boxes over tiles in an editor world (the inspector's selection).
UCLASS()
class UTileDebugHighlightSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

public:
	// Replaces everything that is drawn. An empty array clears the highlight.
	void SetTiles(const TArray<FTileDebugDrawTile>& InTiles);

private:
	UTileDebugHighlightComponent* GetOrCreateRenderComponent();

private:
	UPROPERTY(Transient)
	TObjectPtr<UTileDebugHighlightComponent> RenderComponent = nullptr;
};
