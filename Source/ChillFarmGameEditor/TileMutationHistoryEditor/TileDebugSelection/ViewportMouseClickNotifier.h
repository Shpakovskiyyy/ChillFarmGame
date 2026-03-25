#pragma once

#include <UnrealClient.h>

#include "Framework/Application/IInputProcessor.h"
#include "Engine/HitResult.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnViewportMouseClick, const FHitResult&)

// This helper class is used to route viewport mouse click event to subscribed objects 
class FViewportMouseClickNotifier : public IInputProcessor
{
public:
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	
private:
	bool IsInsideTheViewport(const FVector2D& InCursorPosition, FViewport* InViewport) const;

public:
	FOnViewportMouseClick OnViewportMouseClick;
	
};