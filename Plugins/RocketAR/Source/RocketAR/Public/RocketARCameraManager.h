#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TelemetryTypes.h"
#include "RocketDefinition.h"
#include "RocketARCameraManager.generated.h"

class ACineCameraActor;
class ACesiumGeoreference;

/**
 * Manages the CG camera: ECEF position + quaternion → UE transform,
 * with configurable body-fixed mounting offset and FOV.
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

	/** Index into the spawned rig array that currently feeds the output */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rigs", meta = (ClampMin = "0"))
	int32 ActiveRigIndex = 0;

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

private:
	UPROPERTY()
	ACineCameraActor* CameraActor = nullptr;

	UPROPERTY()
	TArray<ACineCameraActor*> SpawnedRigActors;

	UPROPERTY()
	ACesiumGeoreference* Georeference = nullptr;

	bool bAttachedToParent = false;

	void ApplyRigTransform(ACineCameraActor* Camera, const FRocketCameraRig& Rig);

	FVector ECEFToUE(const FVector& ECEFPos) const;
	FQuat ECEFRotToUE(const FQuat& ECEFRot) const;
};
