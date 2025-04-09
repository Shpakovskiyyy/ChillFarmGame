using UnrealBuildTool;

public class ChillFarmGameTarget : TargetRules
{
	public ChillFarmGameTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ChillFarmGame");
	}
}
