#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TelemetryTypes.h"
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

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Set the Cesium georeference for ECEF→UE conversion */
	void SetGeoreference(ACesiumGeoreference* InGeoreference);

	/** Update camera from processed telemetry data */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void UpdateFromTelemetry(const FProcessedTelemetryData& Data);

	/** Get the managed camera actor */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	ACineCameraActor* GetCameraActor() const { return CameraActor; }

	/** Set or create the camera actor */
	void SetCameraActor(ACineCameraActor* InCamera);

	// --- Configuration ---

	/** Body-frame offset from vehicle center (cm). Z=along rocket axis toward nose, Y=lateral. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Config")
	FVector CameraMountOffset = FVector(0.0f, 500.0f, 4000.0f);

	/** Body-frame rotation (degrees). Pitch down to look back along rocket body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Config")
	FRotator CameraMountRotation = FRotator(-75.0f, -10.0f, 0.0f);

	/** Horizontal field of view (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Config", meta=(ClampMin="1.0", ClampMax="180.0"))
	float CameraHFOV = 110.0f;

private:
	UPROPERTY()
	ACineCameraActor* CameraActor = nullptr;

	UPROPERTY()
	ACesiumGeoreference* Georeference = nullptr;

	FVector ECEFToUE(const FVector& ECEFPos) const;
	FQuat ECEFRotToUE(const FQuat& ECEFRot) const;
};
