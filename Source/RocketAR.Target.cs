using UnrealBuildTool;

public class RocketARTarget : TargetRules
{
	public RocketARTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "RocketARGame" });
	}
}
