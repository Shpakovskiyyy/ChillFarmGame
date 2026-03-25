#pragma once

#include "Framework/Docking/TabManager.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogChillFarmGameEditor, Log, All);


class FChillFarmGameEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

protected:
	void RegisterMenus();
	void RegisterTabSpawner();
	void UnregisterTabSpawner();
	
protected:
	TSharedRef<SDockTab> SpawnTileInspectorTab(const FSpawnTabArgs& SpawnTabArgs);

protected:
	void OnGenerateWorld();
	bool CanGenerateWorld();

protected:
	inline static const FName TileInspectorTabName = TEXT("Tile Inspector");
	
};

IMPLEMENT_MODULE(FChillFarmGameEditorModule, ChillFarmGameEditor);