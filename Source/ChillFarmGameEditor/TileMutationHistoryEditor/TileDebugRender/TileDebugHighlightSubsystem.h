#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "TileDebugRender/TileDebugDrawTile.h"

#include "TileDebugHighlightSubsystem.generated.h"

class UTileDebugHighlightComponent;


UCLASS()
class UTileDebugHighlightSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

public:
	void UpdateTiles(const TArray<FTileDebugDrawTile>& InTiles);
	void SetSelectedTile(int32 InSelectedTileIndex);

protected:
	void HandleOnMapOpened(const FString&, bool);

private:
	UPROPERTY()
	AActor* DummyActor = nullptr;

	UPROPERTY()
	UTileDebugHighlightComponent* RenderComponent = nullptr;

};
