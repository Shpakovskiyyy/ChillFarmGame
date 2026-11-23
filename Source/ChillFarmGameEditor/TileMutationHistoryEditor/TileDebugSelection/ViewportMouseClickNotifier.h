#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "Engine/HitResult.h"

class FEditorViewportClient;
class FSlateApplication;
class FViewport;
struct FPointerEvent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnViewportMouseClick, const FHitResult&)

// This helper class is used to route viewport mouse click event to subscribed objects
class FViewportMouseClickNotifier : public IInputProcessor
{
public:
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;

private:
	// The editor viewport client that drives the given viewport, or nullptr when it is driven by
	// something else (the game viewport client during Play In Editor, for example).
	static FEditorViewportClient* FindEditorViewportClient(const FViewport* InViewport);

	// True only when the widget under the cursor belongs to the given viewport. A purely geometric
	// test is not enough: a floating tab on top of the viewport is inside its rectangle too.
	static bool IsCursorOverViewport(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent, FEditorViewportClient& InViewportClient);

public:
	FOnViewportMouseClick OnViewportMouseClick;

};
