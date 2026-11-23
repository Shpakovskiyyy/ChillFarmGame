#include "ViewportMouseClickNotifier.h"

#include "Editor.h"
#include "EditorViewportClient.h"
#include "SEditorViewport.h"
#include "SceneView.h"
#include "UnrealClient.h"
#include "Engine/World.h"
#include "Input/Events.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"

static constexpr auto sWorldTraceDistance = 100000.f;


void FViewportMouseClickNotifier::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) { }

bool FViewportMouseClickNotifier::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return false;

	if (!GEditor) return false;

	// The inspector is an edit-time tool. While a Play In Editor session runs, clicks belong to the game.
	if (GEditor->PlayWorld) return false;

	auto EditorActiveViewport = GEditor->GetActiveViewport();
	if (!EditorActiveViewport) return false;

	// The client used to be static_cast to FEditorViewportClient without any check. Looking it up in
	// the editor's own list is safe for any kind of client and gives nullptr for foreign ones.
	auto EditorViewportClient = FindEditorViewportClient(EditorActiveViewport);
	if (!EditorViewportClient) return false;

	if (!IsCursorOverViewport(SlateApp, MouseEvent, *EditorViewportClient)) return false;

	auto MousePosition = FIntPoint::ZeroValue;
	EditorActiveViewport->GetMousePos(MousePosition);

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		EditorActiveViewport,
		EditorViewportClient->GetScene(),
		// GetEngineShowFlags() is private on FEditorViewportClient; the public member holds the same flags.
		EditorViewportClient->EngineShowFlags
		)
	);

	auto SceneView = EditorViewportClient->CalcSceneView(&ViewFamily);
	if (!SceneView) return false;

	FVector WorldOrigin;
	FVector WorldDirection;
	SceneView->DeprojectFVector2D(MousePosition, /*out*/ WorldOrigin, /*out*/ WorldDirection);

	auto World = EditorViewportClient->GetWorld();
	if (!World) return false;

	FHitResult Hit;
	auto TraceEnd = WorldOrigin + WorldDirection * sWorldTraceDistance;
	if (World->LineTraceSingleByChannel(Hit, WorldOrigin, TraceEnd, ECC_Visibility)) { OnViewportMouseClick.Broadcast(Hit); }

	return false;
}

FEditorViewportClient* FViewportMouseClickNotifier::FindEditorViewportClient(const FViewport* InViewport)
{
	if (!GEditor || !InViewport) return nullptr;

	for (auto ViewportClient : GEditor->GetAllViewportClients())
	{
		if (ViewportClient && ViewportClient->Viewport == InViewport) return ViewportClient;
	}

	return nullptr;
}

bool FViewportMouseClickNotifier::IsCursorOverViewport(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent, FEditorViewportClient& InViewportClient)
{
	const auto ViewportWidget = InViewportClient.GetEditorViewportWidget();
	if (!ViewportWidget.IsValid()) return false;

	// Ask Slate which widgets are really under the cursor, top-most window first. If another tab or
	// window covers the viewport, the viewport widget is simply not part of this path.
	const auto WidgetsUnderCursor = SlateApp.LocateWindowUnderMouse(
		MouseEvent.GetScreenSpacePosition(),
		SlateApp.GetInteractiveTopLevelWindows(),
		false,
		MouseEvent.GetUserIndex()
	);
	if (!WidgetsUnderCursor.IsValid()) return false;

	const SWidget* const ViewportWidgetPtr = ViewportWidget.Get();
	for (auto Index = 0; Index < WidgetsUnderCursor.Widgets.Num(); ++Index)
	{
		if (&WidgetsUnderCursor.Widgets[Index].Widget.Get() == ViewportWidgetPtr) return true;
	}

	return false;
}
