#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TelemetryTypes.h"
#include "RocketDefinition.h"
#include "RocketARCameraManager.generated.h"

class ACineCameraActor;
class ACesiumGeoreference;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActiveRigChanged, UTextureRenderTarget2D*, NewRenderTarget);

/**
 * Manages the CG camera: ECEF position + quaternion → UE transform,
 * with configurable body-fixed mounting offset and FOV.
 *
 * Each rig owns a SceneCaptureComponent2D + TextureRenderTarget2D pair so the
 * broadcast feed renders independently of the game viewport (the viewport is
 * free for an operator UI / debug overlays without bleeding into SDI).
 */
UCLASS(BlueprintType, meta=(BlueprintSpawnableComponent))
class ROCKETAR_API URocketARCameraManager : public UActorComponent
{
	GENERATED_BODY()

public:
	URocketARCameraManager();

	/** Set the Cesium georeference for ECEF→UE conversion */
	void SetGeoreference(ACesiumGeoreference* InGeoreference);

	/** Attach camera to a scene component (e.g., rocket mesh). Camera follows it automatically. */
	void AttachToComponent(USceneComponent* Parent);

	/** Update camera from processed telemetry data (only used when not attached) */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void UpdateFromTelemetry(const FProcessedTelemetryData& Data);

	/** Get the active camera actor (active rig if rigs are spawned, otherwise legacy camera) */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	ACineCameraActor* GetCameraActor() const;

	/** Set or create the camera actor (legacy single-camera path) */
	void SetCameraActor(ACineCameraActor* InCamera);

	/**
	 * Spawn all camera rigs from a rocket definition, attached to Parent.
	 * All rigs are active simultaneously; ActiveRigIndex selects which feeds output.
	 * Replaces any previously spawned rigs.
	 */
	void SpawnRigsFromDefinition(URocketDefinition* Definition, USceneComponent* Parent);

	/** Switch the active output camera. Calls SetViewTarget on the first PlayerController. */
	UFUNCTION(BlueprintCallable, Category = "Camera Rigs")
	void SetActiveRigIndex(int32 NewIndex);

	/** Render target of the currently active rig. Feeds the DeckLink MediaCapture. */
	UFUNCTION(BlueprintCallable, Category = "Camera Rigs")
	UTextureRenderTarget2D* GetActiveProductionRenderTarget() const;

	/** Broadcast when the active rig changes; consumers re-bind to the new RT. */
	UPROPERTY(BlueprintAssignable, Category = "Camera Rigs")
	FOnActiveRigChanged OnActiveRigChanged;

	/** Index into the spawned rig array that currently feeds the output */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rigs", meta = (ClampMin = "0"))
	int32 ActiveRigIndex = 0;

	/** Resolution of each rig's production render target. Must match the DeckLink output resolution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rigs")
	FIntPoint ProductionRenderResolution = FIntPoint(1920, 1080);

	// --- Configuration ---

	/** Body-frame offset from vehicle center (cm). Z=along rocket axis toward nose, Y=lateral. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Config")
	FVector CameraMountOffset = FVector(0.0f, 500.0f, 4000.0f);

	/** Body-frame rotation (degrees). Pitch down to look back along rocket body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Config")
	FRotator CameraMountRotation = FRotator(-80.0f, -10.0f, 0.0f);

	/** Roll around the camera's optical axis (degrees, positive=CW, negative=CCW) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Config")
	float CameraOpticalRoll = 45.0f;

	/** Horizontal field of view (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Config", meta=(ClampMin="1.0", ClampMax="180.0"))
	float CameraHFOV = 110.0f;

	/** Apply mount offset and optical roll to the attached camera */
	void UpdateRelativeTransform();

	/**
	 * Configure a SceneCaptureComponent2D + RenderTarget for alpha-safe broadcast capture.
	 * Public so other systems (e.g. operator preview window) can reuse the same profile.
	 */
	static void ConfigureAlphaSafeCapture(USceneCaptureComponent2D* Capture, UTextureRenderTarget2D* RT);

private:
	UPROPERTY()
	ACineCameraActor* CameraActor = nullptr;

	UPROPERTY()
	TArray<ACineCameraActor*> SpawnedRigActors;

	/** Per-rig SceneCapture components, parallel to SpawnedRigActors. */
	UPROPERTY()
	TArray<USceneCaptureComponent2D*> RigCaptures;

	/** Per-rig render targets, parallel to SpawnedRigActors. */
	UPROPERTY()
	TArray<UTextureRenderTarget2D*> RigRenderTargets;

	UPROPERTY()
	ACesiumGeoreference* Georeference = nullptr;

	bool bAttachedToParent = false;

	void ApplyRigTransform(ACineCameraActor* Camera, const FRocketCameraRig& Rig);

	/** Spawn + attach a SceneCapture/RT pair for one rig camera. */
	void SpawnCaptureForRig(ACineCameraActor* Camera, float HFOV);

	/** Destroy all rig SceneCapture/RT pairs (called before re-spawning). */
	void TeardownRigCaptures();

	FVector ECEFToUE(const FVector& ECEFPos) const;
	FQuat ECEFRotToUE(const FQuat& ECEFRot) const;
};
