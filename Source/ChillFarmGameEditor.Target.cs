using UnrealBuildTool;

public class ChillFarmGameEditorTarget : TargetRules
{
	public ChillFarmGameEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ChillFarmGame");
	}
}
