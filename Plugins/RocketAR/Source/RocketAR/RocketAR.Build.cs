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

		// ---- Optional dependencies ----

		// Cesium for Unreal — ECEF coordinate conversion
		PublicDependencyModuleNames.Add("CesiumRuntime");
		PublicDefinitions.Add("WITH_CESIUM=1");

		// Blackmagic — DeckLink fill/key output
		PrivateDependencyModuleNames.Add("MediaIOCore");
		PrivateDependencyModuleNames.Add("BlackmagicMedia");
		PrivateDependencyModuleNames.Add("BlackmagicMediaOutput");
		PublicDefinitions.Add("WITH_BLACKMAGIC=1");
	}
}
