#include "RocketARCameraManager.h"
#include "RocketARModule.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Engine/World.h"

#if WITH_CESIUM
#include "CesiumGeoreference.h"
#endif

URocketARCameraManager::URocketARCameraManager()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URocketARCameraManager::SetGeoreference(ACesiumGeoreference* InGeoreference)
{
	Georeference = InGeoreference;
}

void URocketARCameraManager::SetCameraActor(ACineCameraActor* InCamera)
{
	CameraActor = InCamera;

	if (CameraActor)
	{
		UCineCameraComponent* CineComp = CameraActor->GetCineCameraComponent();
		if (CineComp)
		{
			CineComp->SetFieldOfView(CameraHFOV);
		}
	}
}

FVector URocketARCameraManager::ECEFToUE(const FVector& ECEFPos) const
{
#if WITH_CESIUM
	if (Georeference)
	{
		return Georeference->TransformEarthCenteredEarthFixedPositionToUnreal(ECEFPos);
	}
#endif
	return ECEFPos * 100.0; // fallback: meters to cm
}

FQuat URocketARCameraManager::ECEFRotToUE(const FQuat& ECEFRot) const
{
#if WITH_CESIUM
	if (Georeference)
	{
		FMatrix ECEFToUEMatrix = Georeference->ComputeEarthCenteredEarthFixedToUnrealTransformation();
		ECEFToUEMatrix.RemoveScaling();
		const FQuat ECEFToUEQuat = ECEFToUEMatrix.ToQuat();
		return ECEFToUEQuat * ECEFRot;
	}
#endif
	return ECEFRot;
}

void URocketARCameraManager::UpdateFromTelemetry(const FProcessedTelemetryData& Data)
{
	if (!CameraActor) return;

	// Vehicle position and rotation in UE space
	const FVector VehicleUEPos = Data.UEPosition;
	const FQuat VehicleUERot = Data.UERotation;

	// Apply body-fixed mounting offset:
	// CameraWorldPos = VehicleWorldPos + VehicleWorldRot.RotateVector(MountOffset)
	const FVector WorldOffset = VehicleUERot.RotateVector(CameraMountOffset);
	const FVector CameraPos = VehicleUEPos + WorldOffset;

	// Apply mounting rotation:
	// CameraWorldRot = VehicleWorldRot * MountRotation
	const FQuat MountRotQuat = CameraMountRotation.Quaternion();
	const FQuat CameraRot = VehicleUERot * MountRotQuat;

	CameraActor->SetActorLocation(CameraPos);
	CameraActor->SetActorRotation(CameraRot);

	// Update FOV
	UCineCameraComponent* CineComp = CameraActor->GetCineCameraComponent();
	if (CineComp)
	{
		CineComp->SetFieldOfView(CameraHFOV);
	}
}

void URocketARCameraManager::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Camera updates are driven by UpdateFromTelemetry called from the setup actor
	// No autonomous tick logic needed
}
