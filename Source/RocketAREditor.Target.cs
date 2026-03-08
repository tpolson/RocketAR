using UnrealBuildTool;

public class RocketAREditorTarget : TargetRules
{
	public RocketAREditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "RocketARGame" });
	}
}
