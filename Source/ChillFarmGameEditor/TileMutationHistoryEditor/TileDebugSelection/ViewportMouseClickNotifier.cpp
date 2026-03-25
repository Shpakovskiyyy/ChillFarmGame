#include "ViewportMouseClickNotifier.h"

#include "SceneView.h"
#include "LevelEditorViewport.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"

static constexpr auto sWorldTraceDistance = 100000.f;


void FViewportMouseClickNotifier::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) { }

bool FViewportMouseClickNotifier::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return false;
	
	if(!GEditor) return false;
	
	auto EditorActiveViewport = GEditor->GetActiveViewport();
	
	auto MousePosition = FIntPoint::ZeroValue;
	EditorActiveViewport->GetMousePos(MousePosition);
	if ( !IsInsideTheViewport(MousePosition, EditorActiveViewport) ) return false;

	auto ViewportClient = EditorActiveViewport->GetClient();
	auto EditorViewportClient = static_cast<FEditorViewportClient*>( ViewportClient );
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		EditorActiveViewport,
		EditorViewportClient->GetScene(),
		*ViewportClient->GetEngineShowFlags()
		)
	);

	auto SceneView = EditorViewportClient->CalcSceneView(&ViewFamily);
	
	FVector WorldOrigin;
	FVector WorldDirection;
	SceneView->DeprojectFVector2D(MousePosition, /*out*/ WorldOrigin, /*out*/ WorldDirection);
	
	FHitResult Hit;
	auto TraceEnd = WorldOrigin + WorldDirection * sWorldTraceDistance;
	if (ViewportClient->GetWorld()->LineTraceSingleByChannel(Hit, WorldOrigin, TraceEnd, ECC_Visibility)) { OnViewportMouseClick.Broadcast(Hit); }

	return false;
}

bool FViewportMouseClickNotifier::IsInsideTheViewport(const FVector2D& InCursorPosition, FViewport* InViewport) const
{
	const auto ViewportOrigin = InViewport->GetInitialPositionXY();
	const auto ViewportSize = InViewport->GetSizeXY();
	return	InCursorPosition.X >= ViewportOrigin.X && 
			InCursorPosition.Y >= ViewportOrigin.Y && 
			InCursorPosition.X <= ViewportOrigin.X + ViewportSize.X && 
			InCursorPosition.Y <= ViewportOrigin.Y + ViewportSize.Y;
}