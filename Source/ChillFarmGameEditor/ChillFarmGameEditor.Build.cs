using UnrealBuildTool;

public class ChillFarmGameEditor : ModuleRules
{
	public ChillFarmGameEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		OverridePackageType = PackageOverrideType.EditorOnly;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(new [] 
		{
			"ChillFarmGameEditor/TileMutationHistoryEditor",
			"ChillFarmGameEditor/TileMutationHistoryEditor/Widgets"
		});
		
		PublicDependencyModuleNames.AddRange(new []
		{
			"Core",
			"CoreUObject",
			"Engine"
		});
		
        PrivateDependencyModuleNames.AddRange(new [] 
        { 
	        "UnrealEd",
	        "EditorSubsystem",
	        "Slate",
	        "SlateCore",
	        "ToolMenus",
	        "LevelEditor",
	        "InputCore",
	        "ChillFarmGame",
        });
    }
}
