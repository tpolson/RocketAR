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

void URocketARCameraManager::AttachToComponent(USceneComponent* Parent)
{
	if (!CameraActor || !Parent) return;

	CameraActor->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
	bAttachedToParent = true;

	UpdateRelativeTransform();

	UE_LOG(LogRocketAR, Log, TEXT("Camera attached to %s"), *Parent->GetName());
}

void URocketARCameraManager::UpdateRelativeTransform()
{
	if (!CameraActor) return;

	// Mount offset is relative to parent (rocket mesh Z = rocket axis)
	// Mount rotation + optical roll applied as relative rotation
	const FQuat MountRotQuat = CameraMountRotation.Quaternion();

	// Apply optical roll around the aimed forward axis
	const FVector AimedForward = MountRotQuat.GetForwardVector();
	const FQuat OpticalRollQuat(AimedForward, FMath::DegreesToRadians(CameraOpticalRoll));
	const FQuat FinalRot = OpticalRollQuat * MountRotQuat;

	CameraActor->SetActorRelativeLocation(CameraMountOffset);
	CameraActor->SetActorRelativeRotation(FinalRot.Rotator());

	UCineCameraComponent* CineComp = CameraActor->GetCineCameraComponent();
	if (CineComp)
	{
		CineComp->SetFieldOfView(CameraHFOV);
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

	// If attached to rocket mesh, just update relative transform (config may have changed)
	if (bAttachedToParent)
	{
		UpdateRelativeTransform();
		return;
	}

	// Fallback: manual world-space positioning (when no rocket mesh to attach to)
	const FVector VehicleUEPos = Data.UEPosition;
	const FQuat VehicleUERot = Data.UERotation;

	const FVector WorldOffset = VehicleUERot.RotateVector(CameraMountOffset);
	const FVector CameraPos = VehicleUEPos + WorldOffset;

	const FQuat MountRotQuat = CameraMountRotation.Quaternion();
	const FQuat AimedRot = VehicleUERot * MountRotQuat;

	const FVector CameraForward = AimedRot.GetForwardVector();
	const FQuat OpticalRollQuat(CameraForward, FMath::DegreesToRadians(CameraOpticalRoll));
	const FQuat CameraRot = OpticalRollQuat * AimedRot;

	CameraActor->TeleportTo(CameraPos, CameraRot.Rotator(), false, true);

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
}
