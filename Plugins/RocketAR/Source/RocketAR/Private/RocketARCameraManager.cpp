#include "RocketARCameraManager.h"
#include "RocketDefinition.h"
#include "RocketARModule.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
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

	const FQuat MountRotQuat = CameraMountRotation.Quaternion();
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

UTextureRenderTarget2D* URocketARCameraManager::GetActiveProductionRenderTarget() const
{
	if (RigRenderTargets.IsValidIndex(ActiveRigIndex))
	{
		return RigRenderTargets[ActiveRigIndex];
	}
	return nullptr;
}

void URocketARCameraManager::SpawnRigsFromDefinition(URocketDefinition* Definition, USceneComponent* Parent)
{
	TeardownRigCaptures();
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
			SpawnCaptureForRig(NewCamera, Rig.HFOV);
		}
	}

	ActiveRigIndex = FMath::Clamp(ActiveRigIndex, 0, FMath::Max(SpawnedRigActors.Num() - 1, 0));

	UE_LOG(LogRocketAR, Log, TEXT("CameraManager: spawned %d rig(s) for '%s' (%dx%d production RT)"),
		SpawnedRigActors.Num(), *Definition->RocketName.ToString(),
		ProductionRenderResolution.X, ProductionRenderResolution.Y);

	OnActiveRigChanged.Broadcast(GetActiveProductionRenderTarget());
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

void URocketARCameraManager::SpawnCaptureForRig(ACineCameraActor* Camera, float HFOV)
{
	if (!Camera) return;

	UCineCameraComponent* CineComp = Camera->GetCineCameraComponent();
	if (!CineComp)
	{
		UE_LOG(LogRocketAR, Error, TEXT("CameraManager: rig %s has no cine camera component"), *Camera->GetName());
		RigCaptures.Add(nullptr);
		RigRenderTargets.Add(nullptr);
		return;
	}

	// Render target: floating-point RGBA to preserve alpha through HDR pipeline.
	UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this);
	RT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f;
	RT->ClearColor = FLinearColor::Transparent;
	RT->bAutoGenerateMips = false;
	RT->bGPUSharedFlag = false;
	RT->InitAutoFormat(ProductionRenderResolution.X, ProductionRenderResolution.Y);
	RT->UpdateResourceImmediate(true);

	// SceneCapture attached to the cine camera — inherits transform, FOV applied below.
	USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(Camera);
	Capture->SetupAttachment(CineComp);
	Capture->RegisterComponent();
	Capture->TextureTarget = RT;
	Capture->FOVAngle = HFOV;

	ConfigureAlphaSafeCapture(Capture, RT);

	RigCaptures.Add(Capture);
	RigRenderTargets.Add(RT);

	UE_LOG(LogRocketAR, Log, TEXT("CameraManager: capture+RT spawned for rig %s (HFOV=%.1f)"),
		*Camera->GetName(), HFOV);
}

void URocketARCameraManager::ConfigureAlphaSafeCapture(USceneCaptureComponent2D* Capture, UTextureRenderTarget2D* RT)
{
	if (!Capture) return;

	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorHDR;
	Capture->bCaptureEveryFrame = true;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
	Capture->CompositeMode = ESceneCaptureCompositeMode::SCCM_Overwrite;

	// Show flags — strip everything that perturbs alpha or pumps exposure.
	FEngineShowFlags& Flags = Capture->ShowFlags;
	Flags.SetAtmosphere(false);
	Flags.SetFog(false);
	Flags.SetVolumetricFog(false);
	Flags.SetMotionBlur(false);
	Flags.SetBloom(false);
	Flags.SetEyeAdaptation(false);
	Flags.SetToneCurve(false);
	Flags.SetColorGrading(false);
	Flags.SetLensFlares(false);
	Flags.SetVignette(false);
	Flags.SetGrain(false);
	Flags.SetSceneColorFringe(false);
	Flags.SetScreenSpaceReflections(false);
	Flags.SetAmbientOcclusion(false);
	Flags.SetDepthOfField(false);

	// PostProcess settings — explicit overrides to guarantee parity with main view.
	FPostProcessSettings& PP = Capture->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = 0.0f;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.0f;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.0f;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.0f;
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = 0.0f;
	PP.bOverride_SceneFringeIntensity = true;
	PP.SceneFringeIntensity = 0.0f;
	PP.bOverride_AmbientOcclusionIntensity = true;
	PP.AmbientOcclusionIntensity = 0.0f;
	PP.bOverride_ScreenSpaceReflectionIntensity = true;
	PP.ScreenSpaceReflectionIntensity = 0.0f;
	PP.bOverride_DepthOfFieldFstop = true;
	PP.DepthOfFieldFstop = 32.0f;
}

void URocketARCameraManager::TeardownRigCaptures()
{
	for (USceneCaptureComponent2D* Capture : RigCaptures)
	{
		if (Capture)
		{
			Capture->TextureTarget = nullptr;
			Capture->DestroyComponent();
		}
	}
	RigCaptures.Empty();

	for (UTextureRenderTarget2D* RT : RigRenderTargets)
	{
		if (RT)
		{
			RT->ReleaseResource();
		}
	}
	RigRenderTargets.Empty();
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

	OnActiveRigChanged.Broadcast(GetActiveProductionRenderTarget());
}

void URocketARCameraManager::UpdateFromTelemetry(const FProcessedTelemetryData& Data)
{
	if (!CameraActor) return;

	if (bAttachedToParent)
	{
		UpdateRelativeTransform();
		return;
	}

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
