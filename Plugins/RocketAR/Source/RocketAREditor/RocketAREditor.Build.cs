using UnrealBuildTool;

public class RocketAREditor : ModuleRules
{
	public RocketAREditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"RocketAR",
			"UnrealEd",
			"Slate",
			"SlateCore",
			"PropertyEditor"
		});
	}
}
