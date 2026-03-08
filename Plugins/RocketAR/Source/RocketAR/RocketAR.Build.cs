using UnrealBuildTool;

public class RocketAR : ModuleRules
{
	public RocketAR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"UMG",
			"Slate",
			"SlateCore",
			"ProceduralMeshComponent",
			"CinematicCamera",
			"RenderCore",
			"RHI"
		});

		// MediaIOCore — only needed when DeckLink hardware is available
		// Uncomment when BlackmagicMedia plugin is enabled:
		// PrivateDependencyModuleNames.Add("MediaIOCore");

		// ---- Optional dependencies ----
		// Uncomment these when the plugins are installed:

		// Cesium for Unreal — ECEF coordinate conversion
		PublicDependencyModuleNames.Add("CesiumRuntime");
		PublicDefinitions.Add("WITH_CESIUM=1");

		// Blackmagic — DeckLink fill/key output (uncomment when hardware available)
		// PrivateDependencyModuleNames.Add("BlackmagicMedia");
		// PublicDefinitions.Add("WITH_BLACKMAGIC=1");
		PublicDefinitions.Add("WITH_BLACKMAGIC=0");
	}
}
