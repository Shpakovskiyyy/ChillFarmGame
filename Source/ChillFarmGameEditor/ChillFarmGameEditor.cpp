#include "ChillFarmGameEditor.h"

#include "Editor.h"
#include "EngineUtils.h"

#include "Modules/ModuleManager.h"

#include "ToolMenus.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

#include "TileMutationHistoryEditor/Widgets/STileInspectorWindow.h"
#include "TileWorldGenerator.h"


DEFINE_LOG_CATEGORY(LogChillFarmGameEditor)


void FChillFarmGameEditorModule::StartupModule()
{
	IModuleInterface::StartupModule();
	UE_LOG(LogChillFarmGameEditor, Log, TEXT("Starting ChillFarmGameEditor module..."))

	RegisterTabSpawner();
	
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
		this, &FChillFarmGameEditorModule::RegisterMenus)
	);
}

void FChillFarmGameEditorModule::ShutdownModule()
{
	IModuleInterface::ShutdownModule();
	UE_LOG(LogChillFarmGameEditor, Log, TEXT("Shutting down ChillFarmGameEditor module..."))

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	UnregisterTabSpawner();
}

// TODO Move all of the tool menus somewhere, and then make it better, like an actual button
void FChillFarmGameEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	
	auto Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");

	FToolMenuSection& Section = Menu->AddSection(
		TileInspectorTabName,
		FText::FromString("Tile Tools")
	);

	Section.AddMenuEntry(
		"OpenTileInspector",
		FText::FromName(TileInspectorTabName),
		FText::FromString("Open Tile Inspector Window"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]
			{
				FGlobalTabmanager::Get()->TryInvokeTab(TileInspectorTabName);
			})
		)
	);
	

	Section.AddMenuEntry(
		FName("Generate World"),
		FText::FromString("Generate World"),
		FText::FromString("Generates tile world using ATileWorldGenerator if one is present on the Level"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Recompile"),
		FToolUIActionChoice(
			FUIAction(
				FExecuteAction::CreateRaw(this, &FChillFarmGameEditorModule::OnGenerateWorld),
				FCanExecuteAction::CreateRaw(this, &FChillFarmGameEditorModule::CanGenerateWorld)
			)
		)
	);
}

void FChillFarmGameEditorModule::RegisterTabSpawner()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TileInspectorTabName,
		FOnSpawnTab::CreateRaw(this, &FChillFarmGameEditorModule::SpawnTileInspectorTab)
	)
	.SetDisplayName(NSLOCTEXT("TileInspector", "TabTitle", "Tile Inspector"))
	.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FChillFarmGameEditorModule::UnregisterTabSpawner()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TileInspectorTabName);
}

TSharedRef<SDockTab> FChillFarmGameEditorModule::SpawnTileInspectorTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab).TabRole(NomadTab)
	[
		SNew(STileInspectorWindow)	
	];
}

void FChillFarmGameEditorModule::OnGenerateWorld()
{
	if (!GEditor) return;

	auto World = GEditor->GetEditorWorldContext().World();
	if (!World) return;

	for (TActorIterator<ATileWorldGenerator> It(World); It; ++It)
	{
		It->GenerateWorld();
		return;
	}
}

bool FChillFarmGameEditorModule::CanGenerateWorld()
{
	if (!GEditor) return false;

	auto World = GEditor->GetEditorWorldContext().World();
	if (!World) return false;

	for (TActorIterator<ATileWorldGenerator> It(World); It; ++It) return true;

	return false;
}