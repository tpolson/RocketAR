#include "RocketARCameraManager.h"
#include "RocketDefinition.h"
#include "RocketARModule.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#if WITH_CESIUM
#include "CesiumGeoreference.h"
#endif

URocketARCameraManager::URocketARCameraManager()
{
	PrimaryComponentTick.bCanEverTick = false;
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
	// No-op if rig cameras are already managing attachment
	if (SpawnedRigActors.Num() > 0) return;
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

ACineCameraActor* URocketARCameraManager::GetCameraActor() const
{
	if (SpawnedRigActors.IsValidIndex(ActiveRigIndex))
	{
		return SpawnedRigActors[ActiveRigIndex];
	}
	return CameraActor;
}

void URocketARCameraManager::SpawnRigsFromDefinition(URocketDefinition* Definition, USceneComponent* Parent)
{
	for (ACineCameraActor* OldActor : SpawnedRigActors)
	{
		if (OldActor) OldActor->Destroy();
	}
	SpawnedRigActors.Empty();

	if (!Definition || !Parent || !GetWorld()) return;

	for (const FRocketCameraRig& Rig : Definition->CameraRigs)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACineCameraActor* NewCamera = GetWorld()->SpawnActor<ACineCameraActor>(
			ACineCameraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (NewCamera)
		{
			NewCamera->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
			ApplyRigTransform(NewCamera, Rig);
			SpawnedRigActors.Add(NewCamera);
		}
	}

	// Clamp index in case new definition has fewer rigs
	ActiveRigIndex = FMath::Clamp(ActiveRigIndex, 0, FMath::Max(SpawnedRigActors.Num() - 1, 0));

	UE_LOG(LogRocketAR, Log, TEXT("CameraManager: spawned %d rig(s) for '%s'"),
		SpawnedRigActors.Num(), *Definition->RocketName.ToString());
}

void URocketARCameraManager::ApplyRigTransform(ACineCameraActor* Camera, const FRocketCameraRig& Rig)
{
	if (!Camera) return;

	const FQuat MountRotQuat = Rig.MountRotation.Quaternion();
	const FVector AimedForward = MountRotQuat.GetForwardVector();
	const FQuat OpticalRollQuat(AimedForward, FMath::DegreesToRadians(Rig.OpticalRoll));
	const FQuat FinalRot = OpticalRollQuat * MountRotQuat;

	Camera->SetActorRelativeLocation(Rig.MountOffset);
	Camera->SetActorRelativeRotation(FinalRot.Rotator());

	UCineCameraComponent* CineComp = Camera->GetCineCameraComponent();
	if (CineComp)
	{
		CineComp->SetFieldOfView(Rig.HFOV);
	}
}

void URocketARCameraManager::SetActiveRigIndex(int32 NewIndex)
{
	if (!SpawnedRigActors.IsValidIndex(NewIndex)) return;

	ActiveRigIndex = NewIndex;

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PC)
	{
		PC->SetViewTarget(SpawnedRigActors[NewIndex]);
		UE_LOG(LogRocketAR, Log, TEXT("CameraManager: active rig set to index %d"), NewIndex);
	}
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

