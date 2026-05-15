#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StaticMesh.h"
#include "RocketDefinition.generated.h"

/**
 * A named camera rig preset for a specific rocket.
 * All offsets are body-fixed: Z = rocket axis toward nose, Y = lateral.
 */
USTRUCT(BlueprintType)
struct ROCKETAR_API FRocketCameraRig
{
	GENERATED_BODY()

	/** Human-readable name shown in the rig selector (e.g. "Nose Cam", "Chase Cam") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rig")
	FName RigName = NAME_None;

	/** Body-frame offset from vehicle base (cm). Z = rocket axis toward nose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rig")
	FVector MountOffset = FVector(0.0f, 500.0f, 4000.0f);

	/** Body-frame rotation (degrees). Pitch down to look back along rocket body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rig")
	FRotator MountRotation = FRotator(-80.0f, -10.0f, 0.0f);

	/** Roll around the camera's optical axis (degrees, positive = CW) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rig")
	float OpticalRoll = 0.0f;

	/** Horizontal field of view (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rig",
		meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float HFOV = 90.0f;
};

/**
 * Data asset describing a rocket: real-world dimensions, optional 3D mesh,
 * production visibility flag, and an array of named camera rig presets.
 *
 * Create via Content Browser → right-click → Miscellaneous → Data Asset → RocketDefinition.
 * Assign to ARocketARSetupActor::ActiveRocket. When null, the setup actor falls back to
 * legacy cylinder + single-camera behavior with no regression.
 */
UCLASS(BlueprintType)
class ROCKETAR_API URocketDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Display name, e.g. "SLS Block 1", "Falcon 9 Full Thrust" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rocket")
	FName RocketName = NAME_None;

	/** Vehicle body height in meters (pivot at engine base, grows toward nose). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rocket|Dimensions",
		meta = (ClampMin = "1.0"))
	float Height = 98.0f;

	/** Vehicle body radius in meters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rocket|Dimensions",
		meta = (ClampMin = "0.1"))
	float Radius = 4.2f;

	/**
	 * Optional real 3D mesh for this rocket (soft reference — not loaded until needed).
	 * If not assigned, DevVisualizationActor falls back to the procedural cylinder using Height/Radius.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rocket|Mesh")
	TSoftObjectPtr<UStaticMesh> RocketMesh;

	/**
	 * When true, the rocket mesh is included in the broadcast SDI output as a CG element.
	 * Useful as a fill-in when the live rocket is not in frame.
	 * When false, the mesh uses an additive material that is visible in the viewport
	 * but does not write to the broadcast key channel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rocket|Production")
	bool bRocketVisibleInProduction = false;

	/**
	 * All camera rigs for this rocket. All rigs spawn simultaneously as CineCameraActors
	 * attached to the rocket mount point. ARocketARSetupActor::ActiveCameraRigIndex
	 * selects which one feeds the output.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rocket|Camera Rigs")
	TArray<FRocketCameraRig> CameraRigs;
};
