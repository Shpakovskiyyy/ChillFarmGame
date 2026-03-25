#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Engine/HitResult.h"
#include "TileDebugSelectionEditorSubsystem.generated.h"

class FViewportMouseClickNotifier;
class ATileVisualGrid;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTileSelected, const FIntPoint&);


UCLASS()
class UTileDebugSelectionEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	FIntPoint GetSelectedTile() const;

private:
	void HandleOnMapOpened(const FString&, bool);
	void HandleViewportMouseClick(const FHitResult& InHitResult);

public:
	FOnTileSelected OnTileSelected;

private:
	FIntPoint SelectedTile = FIntPoint::NoneValue;

private:
	TSharedPtr<FViewportMouseClickNotifier> ClickProcessor = nullptr;

private:
	UPROPERTY()
	TWeakObjectPtr<ATileVisualGrid> TileVisualGrid = nullptr;
	
};