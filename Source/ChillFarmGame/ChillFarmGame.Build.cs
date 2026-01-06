using UnrealBuildTool;

public class ChillFarmGame : ModuleRules
{
	public ChillFarmGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] {
			"ChillFarmGame/Crops",
			"ChillFarmGame/Interfaces",
			"ChillFarmGame/Persistent",
			"ChillFarmGame/Player",
			"ChillFarmGame/WorldGeneration"
		});
        PublicDependencyModuleNames.AddRange(new string[] { 
	        "Core",
	        "CoreUObject",
	        "Engine",
	        "InputCore",
	        "NavigationSystem",
	        "AIModule",
	        "Niagara",
	        "EnhancedInput",
        });
    }
}
