#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Engine/HitResult.h"
#include "TileDebugSelectionEditorSubsystem.generated.h"

class FViewportMouseClickNotifier;
class UWorld;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTileSelected, const FIntPoint&);


// Turns clicks on the drawn tile map into a selected tile coordinate.
//
// Works from the clicked world position and the tile layout, not from which mesh instance was hit,
// so it does not care how tiles are rendered.
UCLASS()
class UTileDebugSelectionEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	// FIntPoint::NoneValue when nothing is selected.
	FIntPoint GetSelectedTile() const { return SelectedTile; }

	// Selects a tile of the editor world by coordinate (used by the inspector's text field too).
	void SelectTile(UWorld* InWorld, const FIntPoint& InTile);

private:
	void HandleViewportMouseClick(const FHitResult& InHitResult);
	void HandleMapOpened(const FString& InFilename, bool bAsTemplate);

public:
	FOnTileSelected OnTileSelected;

private:
	FIntPoint SelectedTile = FIntPoint::NoneValue;
	TSharedPtr<FViewportMouseClickNotifier> ClickProcessor;
};
