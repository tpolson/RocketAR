#include "RocketARSetupActor.h"
#include "RocketDefinition.h"
#include "RocketARInputComponent.h"
#include "RocketARMediaOutput.h"
#include "RocketAROperatorSettings.h"
#include "OperatorConsoleWidget.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "TelemetrySubsystem.h"
#include "FlightEventDetector.h"
#include "BannerManager.h"
#include "RocketARCameraManager.h"
#include "CSVTelemetryProvider.h"
#include "DevVisualizationActor.h"
#include "RocketARModule.h"
#include "RocketARHUD.h"
#include "BannerActor.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

#if WITH_CESIUM
#include "CesiumGeoreference.h"
#endif

ARocketARSetupActor::ARocketARSetupActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Explicitly create a plain UInputComponent — UE 5.7 defaults to Enhanced Input,
	// whose UEnhancedInputComponent does not process legacy BindKey calls.
	InputComponent = CreateDefaultSubobject<UInputComponent>(TEXT("ActorInputComponent0"));

	// Create components
	BannerManager = CreateDefaultSubobject<UBannerManager>(TEXT("BannerManager"));
	CameraManager = CreateDefaultSubobject<URocketARCameraManager>(TEXT("CameraManager"));
	RocketARInput = CreateDefaultSubobject<URocketARInputComponent>(TEXT("RocketARInput"));
	MediaOutputComponent = CreateDefaultSubobject<URocketARMediaOutput>(TEXT("MediaOutput"));
	MediaOutputComponent->bManualInit = true; // Defer until SetupDeckLink()
}

void ARocketARSetupActor::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogRocketAR, Log, TEXT("RocketAR Setup Actor: BeginPlay"));

	// Load persisted operator settings before any subsystem init so values apply.
	LoadOperatorSettings();

	// Pre-compile banner materials to avoid checkerboard flash on first spawn
	ABannerActor::WarmUpMaterials();

	// Create event detector
	EventDetector = NewObject<UFlightEventDetector>(this);
	EventDetector->Config = EventConfig;

	// Setup subsystems and infrastructure
	SetupGeoreference();
	SetupCamera();

	if (bUseCSVProvider && !bFreezeFrameMode)
	{
		SetupCSVProvider();
	}

	SetupDevVisualization();

	// Propagate DeckLink output resolution to the camera manager so each rig's
	// production render target matches the SDI output (avoids MediaCapture resize).
	if (CameraManager && bEnableDeckLink)
	{
		switch (DeckLinkOutputResolution)
		{
		case ERocketAROutputResolution::Res_1080p60:
		case ERocketAROutputResolution::Res_1080p5994:
			CameraManager->ProductionRenderResolution = FIntPoint(1920, 1080);
			break;
		case ERocketAROutputResolution::Res_2160p60:
		case ERocketAROutputResolution::Res_2160p5994:
			CameraManager->ProductionRenderResolution = FIntPoint(3840, 2160);
			break;
		}
	}

	// Attach camera and banners to unscaled rocket mount point so they move rigidly with the rocket
	if (DevVisActor && DevVisActor->GetRocketMountPoint())
	{
		if (ActiveRocket)
		{
			// Rocket library path: apply definition mesh/visibility, spawn all rig cameras
			DevVisActor->ApplyRocketDefinition(ActiveRocket);
			if (CameraManager)
			{
				CameraManager->SpawnRigsFromDefinition(ActiveRocket, DevVisActor->GetRocketMountPoint());
				CameraManager->SetActiveRigIndex(ActiveCameraRigIndex);
			}
		}
		else
		{
			// Legacy path: single camera attached to mount point
			if (CameraManager)
			{
				CameraManager->AttachToComponent(DevVisActor->GetRocketMountPoint());
			}
		}

		if (BannerManager)
		{
			BannerManager->SetAttachTarget(DevVisActor->GetRocketMountPoint());
		}
	}

	WireSubsystems();

	if (bEnableDeckLink)
	{
		SetupDeckLink();
	}

	SetupHUD();

	SetupOperatorConsole();

	if (bFreezeFrameMode)
	{
		SetupFreezeFrame();
	}

	UE_LOG(LogRocketAR, Log, TEXT("RocketAR Setup Actor: Initialization complete"));
}

void ARocketARSetupActor::SetupGeoreference()
{
#if WITH_CESIUM
	UWorld* World = GetWorld();
	if (!World) return;

	// Find existing georeference or create one
	for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
	{
		Georeference = *It;
		break;
	}

	if (!Georeference)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Georeference = World->SpawnActor<ACesiumGeoreference>(
			ACesiumGeoreference::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	}

	if (Georeference)
	{
		Georeference->SetOriginLatitude(LaunchPadLatitude);
		Georeference->SetOriginLongitude(LaunchPadLongitude);
		Georeference->SetOriginHeight(LaunchPadAltitude);
		UE_LOG(LogRocketAR, Log, TEXT("Georeference set to: Lat=%.4f, Lon=%.4f, Alt=%.1f"),
			LaunchPadLatitude, LaunchPadLongitude, LaunchPadAltitude);

		// Earth center (0,0,0 ECEF) must map to a large UE-space vector (~6371 km below origin).
		// Identity / near-zero result means Cesium failed to initialize the transform.
		const FVector EarthCenterUE = Georeference->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector);
		const double EarthCenterDistCm = EarthCenterUE.Size();
		constexpr double MinExpectedEarthRadiusCm = 6.0e8;
		if (EarthCenterDistCm < MinExpectedEarthRadiusCm)
		{
			UE_LOG(LogRocketAR, Error,
				TEXT("Georeference sanity check FAILED: Earth center -> UE=%s (%.0f cm, expected > %.0f cm). Cesium may not be initialized."),
				*EarthCenterUE.ToString(), EarthCenterDistCm, MinExpectedEarthRadiusCm);
		}
		else
		{
			UE_LOG(LogRocketAR, Log, TEXT("Georeference sanity check OK: Earth center %.0f km from UE origin"),
				EarthCenterDistCm / 1.0e5);
		}
	}
	else
	{
		UE_LOG(LogRocketAR, Error, TEXT("Failed to spawn ACesiumGeoreference — ECEF conversion will not work"));
	}
#else
	UE_LOG(LogRocketAR, Warning, TEXT("Cesium for Unreal not available — ECEF conversion will be approximate"));
#endif
}

void ARocketARSetupActor::SetupCamera()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Find existing CineCameraActor or spawn one
	for (TActorIterator<ACineCameraActor> It(World); It; ++It)
	{
		CameraActor = *It;
		break;
	}

	if (!CameraActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		CameraActor = World->SpawnActor<ACineCameraActor>(
			ACineCameraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	}

	if (CameraManager && CameraActor)
	{
		CameraManager->CameraMountOffset = CameraMountOffset;
		CameraManager->CameraMountRotation = CameraMountRotation;
		CameraManager->CameraOpticalRoll = CameraOpticalRoll;
		CameraManager->CameraHFOV = CameraHFOV;
		CameraManager->SetCameraActor(CameraActor);
		CameraManager->SetGeoreference(Georeference);

		// Set this camera as the active view target so the viewport uses it on Play
		APlayerController* PC = World->GetFirstPlayerController();
		if (PC)
		{
			PC->SetViewTarget(CameraActor);
		}
	}
}

void ARocketARSetupActor::SetupCSVProvider()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	CSVProvider = World->SpawnActor<ACSVTelemetryProvider>(
		ACSVTelemetryProvider::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (CSVProvider)
	{
		CSVProvider->CSVFilePath = CSVFilePath;
		CSVProvider->bAutoPlay = true;
		UE_LOG(LogRocketAR, Log, TEXT("CSV provider spawned: %s"), *CSVFilePath);
	}
}

void ARocketARSetupActor::SetupDevVisualization()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	DevVisActor = World->SpawnActor<ADevVisualizationActor>(
		ADevVisualizationActor::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (DevVisActor)
	{
		// Compute Earth center and pole direction in UE space via Cesium
#if WITH_CESIUM
		if (Georeference)
		{
			const FVector EarthCenterUE = Georeference->TransformEarthCenteredEarthFixedPositionToUnreal(
				FVector(0.0, 0.0, 0.0));
			// North pole is at ECEF (0, 0, EarthRadius)
			const FVector NorthPoleUE = Georeference->TransformEarthCenteredEarthFixedPositionToUnreal(
				FVector(0.0, 0.0, 6378137.0));
			const FVector PoleDirection = (NorthPoleUE - EarthCenterUE).GetSafeNormal();
			DevVisActor->SetEarthTransform(EarthCenterUE, PoleDirection);
		}
#endif
		DevVisActor->RocketHeight = RocketHeight;
		DevVisActor->RocketRadius = RocketRadius;
		DevVisActor->SetVisible(bDevVisualization);
		bDevVisLastState = bDevVisualization;
		UE_LOG(LogRocketAR, Log, TEXT("Dev visualization actor spawned (visible=%s)"),
			bDevVisualization ? TEXT("true") : TEXT("false"));
	}

	// Spawn dev camera (always created, toggled via bUseDevCamera)
	SetupDevCamera();

	// Spawn a directional light (sun) for dev lighting
	// UE origin is at launch pad with Cesium ENU: X=East, Y=South, Z=Up
	// Sun coming from above and slightly south/east — illuminates rocket and Earth
	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(
		ADirectionalLight::StaticClass(),
		FVector::ZeroVector,
		FRotator(-60.0f, 145.0f, 0.0f),
		SpawnParams);
	if (Sun)
	{
		Sun->GetLightComponent()->SetIntensity(10.0f);
		Sun->GetLightComponent()->SetLightColor(FLinearColor(1.0f, 0.95f, 0.85f));
		UE_LOG(LogRocketAR, Log, TEXT("Dev sun light spawned"));
	}

	// Spawn a sky light for ambient fill
	ASkyLight* Sky = World->SpawnActor<ASkyLight>(
		ASkyLight::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (Sky)
	{
		USkyLightComponent* SkyComp = Sky->GetLightComponent();
		SkyComp->SetIntensity(2.0f);
		SkyComp->SetLightColor(FLinearColor(0.6f, 0.7f, 1.0f));
		SkyComp->bLowerHemisphereIsBlack = false;
		SkyComp->RecaptureSky();
		UE_LOG(LogRocketAR, Log, TEXT("Dev sky light spawned"));
	}
}


void ARocketARSetupActor::SetupDevCamera()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	DevCameraActor = World->SpawnActor<ACineCameraActor>(
		ACineCameraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (DevCameraActor && DevVisActor && DevVisActor->GetRocketMountPoint())
	{
		DevCameraActor->AttachToComponent(
			DevVisActor->GetRocketMountPoint(),
			FAttachmentTransformRules::KeepRelativeTransform);
		DevCameraActor->SetActorRelativeLocation(DevCameraOffset);
		DevCameraActor->SetActorRelativeRotation(DevCameraRotation);

		UCineCameraComponent* CineComp = DevCameraActor->GetCineCameraComponent();
		if (CineComp)
		{
			CineComp->SetFieldOfView(DevCameraFOV);
		}

		UE_LOG(LogRocketAR, Log, TEXT("Dev camera spawned and attached to rocket mount"));
	}

	// If bUseDevCamera is already true at startup, switch view target
	if (bUseDevCamera && DevCameraActor)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		if (PC)
		{
			PC->SetViewTarget(DevCameraActor);
			bCameraViewSet = true;
		}
	}
	bDevCameraLastState = bUseDevCamera;
}

void ARocketARSetupActor::SetupFreezeFrame()
{
	UE_LOG(LogRocketAR, Log, TEXT("=== FREEZE FRAME MODE === Alt=%.0fm Label=%s"),
		FreezeFrameAltitude, *FreezeFrameEventLabel);

	// Position rocket straight up at the configured altitude (cm)
	const FVector RocketUEPos = FVector(0.0, 0.0, FreezeFrameAltitude * 100.0);

	// Place the dev visualization rocket
	if (DevVisActor)
	{
		FProcessedTelemetryData FakeData;
		FakeData.UEPosition = RocketUEPos;
		FakeData.UERotation = FQuat::Identity; // Vertical (rocket on pad)
		FakeData.AltitudeASL = FreezeFrameAltitude;
		FakeData.VelocityMagnitude = 500.0;
		FakeData.RawData.MissionElapsedTime = 60.0;
		FakeData.RawData.bTelemetryValid = true;
		DevVisActor->UpdateFromTelemetry(FakeData);
	}

	// Update camera mount point
	if (CameraManager)
	{
		FProcessedTelemetryData FakeData;
		FakeData.UEPosition = RocketUEPos;
		CameraManager->UpdateFromTelemetry(FakeData);
	}

	// Cache position for banner manager
	LastVehicleUEPosition = RocketUEPos;
	if (BannerManager)
	{
		BannerManager->UpdateVehiclePosition(RocketUEPos);
	}

	// Spawn a test banner with SlideSpeed=0 so it stays in frame for visual tuning
	FFlightEventData TestEvent;
	TestEvent.EventType = EFlightEvent::MaxQ;
	TestEvent.EventLabel = FreezeFrameEventLabel;
	TestEvent.MET = 60.0;
	TestEvent.Altitude = FreezeFrameAltitude;
	TestEvent.Velocity = 500.0;

	if (BannerManager)
	{
		// Temporarily set slide speed to 0 for freeze-frame test banner
		const float SavedSlideSpeed = BannerManager->SlideSpeed;
		BannerManager->SlideSpeed = 0.0f;
		ABannerActor* TestBanner = BannerManager->SpawnBanner(TestEvent);
		BannerManager->SlideSpeed = SavedSlideSpeed;
		// Banner is parented to rocket — no extra rotation needed
	}

	// Update HUD
	if (HUDOverlay)
	{
		HUDOverlay->bShowTelemetry = bShowHUDTelemetry;
		HUDOverlay->bShowEvents = bShowHUDEvents;
		FProcessedTelemetryData FakeData;
		FakeData.AltitudeASL = FreezeFrameAltitude;
		FakeData.VelocityMagnitude = 500.0;
		FakeData.RawData.MissionElapsedTime = 60.0;
		HUDOverlay->UpdateTelemetry(FakeData);
		HUDOverlay->ShowEvent(TestEvent);
	}

	FreezeFrameAltitudeLast = FreezeFrameAltitude;
	FreezeFrameEventLabelLast = FreezeFrameEventLabel;

	UE_LOG(LogRocketAR, Log, TEXT("Freeze frame: rocket at (%.0f, %.0f, %.0f), banner spawned"),
		RocketUEPos.X, RocketUEPos.Y, RocketUEPos.Z);
}

void ARocketARSetupActor::SetActiveCameraRig(int32 RigIndex)
{
	ActiveCameraRigIndex = RigIndex;
	if (CameraManager)
	{
		CameraManager->SetActiveRigIndex(RigIndex);
	}
}

void ARocketARSetupActor::WireSubsystems()
{
	UWorld* World = GetWorld();
	if (!World) return;

	TelemetrySubsystem = World->GetSubsystem<UTelemetrySubsystem>();
	if (TelemetrySubsystem)
	{
		TelemetrySubsystem->SetGeoreference(Georeference);
		TelemetrySubsystem->ExtrapolationTimeout = ExtrapolationTimeout;

		// Listen for processed telemetry updates
		TelemetrySubsystem->OnTelemetryUpdated.AddDynamic(this, &ARocketARSetupActor::OnTelemetryUpdated);

		UE_LOG(LogRocketAR, Log, TEXT("Wired to TelemetrySubsystem"));
	}
	else
	{
		UE_LOG(LogRocketAR, Error, TEXT("TelemetrySubsystem not found!"));
	}

	// Configure banner manager
	if (BannerManager)
	{
		BannerManager->BannerWidth = BannerWidth;
		BannerManager->BannerHeight = BannerHeight;
		BannerManager->MaxActiveBanners = MaxActiveBanners;
		BannerManager->BannerFadeOutDuration = BannerFadeOutDuration;
		BannerManager->TriggerTimeOffset = TriggerTimeOffset;
		BannerManager->SlideSpeed = SlideSpeed;
		BannerManager->SlideDuration = SlideDuration;
		BannerManager->FadeInDuration = BannerFadeInDuration;
		BannerManager->BannerSpawnZOffset = BannerSpawnZOffset;
		BannerManager->MarkerSpawnZOffset = MarkerSpawnZOffset;
		BannerManager->AnticipationSeconds = AnticipationSeconds;
		BannerManager->BannerTextSize = BannerTextSize;
		BannerManager->BannerTextOffset = BannerTextOffset;
		BannerManager->TextSDFSharpness = TextSDFSharpness;
		BannerManager->BannerRotationYaw = BannerRotationYaw;
	}

	// Wire event detector — setup actor is sole handler for event→banner/HUD
	if (EventDetector)
	{
		EventDetector->OnFlightEvent.AddDynamic(this, &ARocketARSetupActor::OnFlightEventDetected);
	}

	// Sync marker geometry, text config, and debug flag to banner manager
	if (BannerManager)
	{
		BannerManager->MarkerWidth = MarkerWidth;
		BannerManager->MarkerHeight = MarkerHeight;
		BannerManager->MarkerColor = MarkerColor;
		BannerManager->MarkerRotationYaw = MarkerRotationYaw;
		BannerManager->MarkerTextSize = MarkerTextSize;
		BannerManager->MarkerTextOffset = MarkerTextOffset;
		BannerManager->MarkerSDFSharpness = MarkerSDFSharpness;
		BannerManager->bShowDebugMessages = bShowDebugMessages;
		BannerManager->bDevOpaqueBanners = bDevOpaqueBanners;
		BannerManager->BannerImage = BannerImage;
		BannerManager->MarkerImage = MarkerImage;
	}

	// Sync full event config, then overlay convenience properties
	if (EventDetector)
	{
		EventDetector->Config = EventConfig;
		EventDetector->Config.AltitudeMarkerInterval = AltitudeMarkerInterval;
		EventDetector->Config.AltitudeMarkerAnticipation = AltitudeMarkerAnticipation;
	}
}

void ARocketARSetupActor::SetupDeckLink()
{
	if (!MediaOutputComponent) return;

	// Configure from setup actor properties
	MediaOutputComponent->OutputResolution = DeckLinkOutputResolution;
	MediaOutputComponent->bFillAndKey = bDeckLinkFillAndKey;
	MediaOutputComponent->bAutoStart = bDeckLinkAutoStart;
	MediaOutputComponent->bEnableGenlock = bDeckLinkGenlock;
	MediaOutputComponent->bEnableTimecode = bDeckLinkTimecode;

	// Wire the broadcast feed to the active rig's render target (set before Initialize()
	// so auto-start finds a valid source). Re-wires automatically on rig switch.
	if (CameraManager)
	{
		UTextureRenderTarget2D* ActiveRT = CameraManager->GetActiveProductionRenderTarget();
		MediaOutputComponent->SetSourceRenderTarget(ActiveRT);
		CameraManager->OnActiveRigChanged.AddDynamic(MediaOutputComponent, &URocketARMediaOutput::OnActiveRigChanged);

		if (!ActiveRT)
		{
			UE_LOG(LogRocketAR, Warning,
				TEXT("DeckLink: No active rig render target — SDI output will not start. "
				     "Assign an ActiveRocket with at least one camera rig."));
		}
	}

	// Trigger deferred init — creates output, sets up genlock/timecode, starts capture
	MediaOutputComponent->Initialize();

	UE_LOG(LogRocketAR, Log, TEXT("DeckLink: Setup complete (status: %s)"),
		*MediaOutputComponent->GetStatusString());
}

void ARocketARSetupActor::OnTelemetryUpdated(const FProcessedTelemetryData& Data)
{
	// Cache vehicle transform for event disk placement
	LastVehicleUEPosition = Data.UEPosition;
	LastVehicleUERotation = Data.UERotation;
	LastAltitudeASL = Data.AltitudeASL;
	LastVerticalVelocity = Data.VerticalVelocity;

	// Convert ECEF velocity to UE space for predictive banner/marker placement
	const FVector ECEFPos = Data.RawData.VehiclePosition;
	const FVector ECEFVel = Data.RawData.VehicleVelocity;
	bool bVelocitySet = false;
#if WITH_CESIUM
	if (Georeference && !ECEFVel.IsNearlyZero())
	{
		const FVector PosAheadUE = Georeference->TransformEarthCenteredEarthFixedPositionToUnreal(ECEFPos + ECEFVel);
		LastVehicleUEVelocity = PosAheadUE - Data.UEPosition; // UE units per second (cm/s)
		bVelocitySet = true;
	}
#endif
	// Fallback: derive UE velocity from position deltas
	if (!bVelocitySet)
	{
		if (!PrevUEPosition.IsZero())
		{
			const float DT = FMath::Max(GetWorld()->GetDeltaSeconds(), 0.001f);
			LastVehicleUEVelocity = (Data.UEPosition - PrevUEPosition) / DT;
		}
	}
	PrevUEPosition = Data.UEPosition;

	// Update banner manager with current position and velocity
	if (BannerManager)
	{
		BannerManager->UpdateVehiclePosition(Data.UEPosition);
		BannerManager->UpdateVehicleVelocity(LastVehicleUEVelocity);
	}

	// Feed processed telemetry to event detector
	if (EventDetector)
	{
		EventDetector->ProcessTelemetry(Data);
	}

	// Update camera
	if (CameraManager)
	{
		CameraManager->UpdateFromTelemetry(Data);
	}

	// Update dev visualization
	if (DevVisActor && DevVisActor->IsVisible())
	{
		DevVisActor->UpdateFromTelemetry(Data);
	}

	// Update HUD overlay
	if (HUDOverlay)
	{
		HUDOverlay->bShowTelemetry = bShowHUDTelemetry;
		HUDOverlay->bShowEvents = bShowHUDEvents;
		HUDOverlay->UpdateTelemetry(Data);
	}
}

void ARocketARSetupActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Set camera as view target on first available tick (PC may not exist during BeginPlay)
	if (!bCameraViewSet)
	{
		APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if (PC)
		{
			ACineCameraActor* Target = nullptr;
			if (bUseDevCamera && DevCameraActor)
				Target = DevCameraActor;
			else if (CameraManager)
				Target = CameraManager->GetCameraActor(); // returns active rig or legacy camera
			if (Target)
			{
				PC->SetViewTarget(Target);
				bCameraViewSet = true;
				UE_LOG(LogRocketAR, Log, TEXT("Camera view target set to %s"),
					bUseDevCamera ? TEXT("DevCamera") : TEXT("ProductionCamera"));
			}
		}
	}

	// Retry HUD setup if PC wasn't ready during BeginPlay
	if (!HUDOverlay && (bShowHUDTelemetry || bShowHUDEvents))
	{
		SetupHUD();
	}

	// Sync camera config at runtime — legacy single-camera mode only
	if (CameraManager && !ActiveRocket)
	{
		CameraManager->CameraMountOffset = CameraMountOffset;
		CameraManager->CameraMountRotation = CameraMountRotation;
		CameraManager->CameraOpticalRoll = CameraOpticalRoll;
		CameraManager->CameraHFOV = CameraHFOV;
	}

	// Live-sync banner config so new banners use current values
	if (BannerManager)
	{
		BannerManager->BannerWidth = BannerWidth;
		BannerManager->BannerHeight = BannerHeight;
		BannerManager->MaxActiveBanners = MaxActiveBanners;
		BannerManager->BannerFadeOutDuration = BannerFadeOutDuration;
		BannerManager->TriggerTimeOffset = TriggerTimeOffset;
		BannerManager->SlideSpeed = SlideSpeed;
		BannerManager->SlideDuration = SlideDuration;
		BannerManager->FadeInDuration = BannerFadeInDuration;
		BannerManager->BannerSpawnZOffset = BannerSpawnZOffset;
		BannerManager->MarkerSpawnZOffset = MarkerSpawnZOffset;
		BannerManager->AnticipationSeconds = AnticipationSeconds;
		BannerManager->BannerTextSize = BannerTextSize;
		BannerManager->BannerTextOffset = BannerTextOffset;
		BannerManager->TextSDFSharpness = TextSDFSharpness;
	}

	// Live-sync marker geometry, text config, and debug flag
	if (BannerManager)
	{
		BannerManager->MarkerWidth = MarkerWidth;
		BannerManager->MarkerHeight = MarkerHeight;
		BannerManager->MarkerColor = MarkerColor;
		BannerManager->MarkerRotationYaw = MarkerRotationYaw;
		BannerManager->MarkerTextSize = MarkerTextSize;
		BannerManager->MarkerTextOffset = MarkerTextOffset;
		BannerManager->MarkerSDFSharpness = MarkerSDFSharpness;
		BannerManager->bShowDebugMessages = bShowDebugMessages;
		BannerManager->bDevOpaqueBanners = bDevOpaqueBanners;
		BannerManager->BannerImage = BannerImage;
		BannerManager->MarkerImage = MarkerImage;
	}

	// Live-sync HUD flags
	if (HUDOverlay)
	{
		HUDOverlay->bShowTelemetry = bShowHUDTelemetry;
		HUDOverlay->bShowEvents = bShowHUDEvents;
	}

	// Live-sync full event config, then overlay convenience properties
	if (EventDetector)
	{
		EventDetector->Config = EventConfig;
		EventDetector->Config.AltitudeMarkerInterval = AltitudeMarkerInterval;
		EventDetector->Config.AltitudeMarkerAnticipation = AltitudeMarkerAnticipation;
	}

	// Live-sync rocket dimensions for runtime tweaking — legacy mode only
	// (when ActiveRocket is set, dimensions come from the definition)
	if (DevVisActor && !ActiveRocket)
	{
		DevVisActor->RocketHeight = RocketHeight;
		DevVisActor->RocketRadius = RocketRadius;
		DevVisActor->UpdateRocketDimensions();
	}

	// Toggle dev visualization when the bool changes
	if (DevVisActor && bDevVisualization != bDevVisLastState)
	{
		DevVisActor->SetVisible(bDevVisualization);
		bDevVisLastState = bDevVisualization;
	}

	// Live-sync dev camera transform and FOV
	if (DevCameraActor)
	{
		DevCameraActor->SetActorRelativeLocation(DevCameraOffset);
		DevCameraActor->SetActorRelativeRotation(DevCameraRotation);
		UCineCameraComponent* DevCineComp = DevCameraActor->GetCineCameraComponent();
		if (DevCineComp)
		{
			DevCineComp->SetFieldOfView(DevCameraFOV);
		}
	}

	// Toggle between dev camera and production camera
	if (bUseDevCamera != bDevCameraLastState)
	{
		APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if (PC)
		{
			if (bUseDevCamera && DevCameraActor)
			{
				PC->SetViewTarget(DevCameraActor);
				UE_LOG(LogRocketAR, Log, TEXT("Switched to dev camera"));
			}
			else if (CameraActor)
			{
				PC->SetViewTarget(CameraActor);
				UE_LOG(LogRocketAR, Log, TEXT("Switched to production camera"));
			}
		}
		bDevCameraLastState = bUseDevCamera;
	}

	// Live-sync freeze frame — re-run when altitude or label changes
	if (bFreezeFrameMode &&
		(FreezeFrameAltitude != FreezeFrameAltitudeLast || FreezeFrameEventLabel != FreezeFrameEventLabelLast))
	{
		if (BannerManager) BannerManager->DestroyAllBanners();

		SetupFreezeFrame();
		FreezeFrameAltitudeLast = FreezeFrameAltitude;
		FreezeFrameEventLabelLast = FreezeFrameEventLabel;
	}
}

void ARocketARSetupActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (TelemetrySubsystem)
	{
		TelemetrySubsystem->OnTelemetryUpdated.RemoveDynamic(this, &ARocketARSetupActor::OnTelemetryUpdated);
	}

	Super::EndPlay(EndPlayReason);
}

void ARocketARSetupActor::SetupHUD()
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (PC)
	{
		// Replace the default HUD with our custom canvas HUD
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		HUDOverlay = World->SpawnActor<ARocketARHUD>(
			ARocketARHUD::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

		if (HUDOverlay)
		{
			// Set as the player's HUD
			PC->MyHUD = HUDOverlay;
			HUDOverlay->SetOwner(PC);
			UE_LOG(LogRocketAR, Log, TEXT("RocketAR HUD overlay spawned"));
		}
	}
}

// ITelemetryProvider implementation (Method B: direct Blueprint variables)
FTelemetryInputData ARocketARSetupActor::GetTelemetryData_Implementation() const
{
	FTelemetryInputData Data;
	Data.VehiclePosition = InputVehiclePosition;
	Data.VehicleRotation = InputVehicleRotation;
	Data.VehicleVelocity = InputVehicleVelocity;
	Data.VehicleAcceleration = InputVehicleAcceleration;
	Data.EngineThrustPercent = InputEngineThrustPercent;
	Data.MissionElapsedTime = InputMissionElapsedTime;
	Data.bTelemetryValid = bInputTelemetryValid;
	return Data;
}

bool ARocketARSetupActor::IsTelemetryAvailable_Implementation() const
{
	return bInputTelemetryValid;
}

int32 ARocketARSetupActor::GetProviderPriority_Implementation() const
{
	return 50;
}

void ARocketARSetupActor::OnFlightEventDetected(const FFlightEventData& EventData)
{
	// Skip altitude markers when disabled
	if (EventData.EventType == EFlightEvent::AltitudeMarker && !bShowAltitudeMarkers)
	{
		return;
	}

	UE_LOG(LogRocketAR, Log, TEXT("Flight event: %s at MET=%.1f, Alt=%.0fm"),
		*EventData.EventLabel, EventData.MET, EventData.Altitude);

	// All events (including altitude markers) go through the unified banner pipeline
	if (BannerManager)
	{
		BannerManager->QueueBanner(EventData, LastVehicleUEVelocity);
	}

	// Show event on HUD overlay
	if (HUDOverlay && bShowHUDEvents)
	{
		HUDOverlay->ShowEvent(EventData);
	}
}

void ARocketARSetupActor::SetTelemetryData(const FTelemetryInputData& InData)
{
	InputVehiclePosition = InData.VehiclePosition;
	InputVehicleRotation = InData.VehicleRotation;
	InputVehicleVelocity = InData.VehicleVelocity;
	InputVehicleAcceleration = InData.VehicleAcceleration;
	InputEngineThrustPercent = InData.EngineThrustPercent;
	InputMissionElapsedTime = InData.MissionElapsedTime;
	bInputTelemetryValid = InData.bTelemetryValid;
}

// -------- Operator Console / Persistent Settings --------

void ARocketARSetupActor::LoadOperatorSettings()
{
	URocketAROperatorSettings* Settings = URocketAROperatorSettings::LoadFromSlot(OperatorSettingsSlot);
	if (!Settings) return;
	ApplyOperatorSettings(Settings);
}

bool ARocketARSetupActor::SaveOperatorSettings()
{
	URocketAROperatorSettings* Settings =
		Cast<URocketAROperatorSettings>(UGameplayStatics::CreateSaveGameObject(URocketAROperatorSettings::StaticClass()));
	if (!Settings) return false;
	CopyStateToOperatorSettings(Settings);
	return Settings->SaveToSlot(OperatorSettingsSlot);
}

void ARocketARSetupActor::ApplyOperatorSettings(URocketAROperatorSettings* S)
{
	if (!S) return;

	LaunchPadLatitude  = S->LaunchPadLatitude;
	LaunchPadLongitude = S->LaunchPadLongitude;
	LaunchPadAltitude  = S->LaunchPadAltitude;

	CameraMountOffset   = S->CameraMountOffset;
	CameraMountRotation = S->CameraMountRotation;
	CameraOpticalRoll   = S->CameraOpticalRoll;
	CameraHFOV          = S->CameraHFOV;
	ActiveCameraRigIndex = S->ActiveCameraRigIndex;

	BannerWidth          = S->BannerWidth;
	BannerHeight         = S->BannerHeight;
	BannerRotationYaw    = S->BannerRotationYaw;
	MaxActiveBanners     = S->MaxActiveBanners;
	BannerTextSize       = S->BannerTextSize;
	TextSDFSharpness     = S->TextSDFSharpness;
	SlideSpeed           = S->SlideSpeed;
	SlideDuration        = S->SlideDuration;
	BannerFadeInDuration = S->BannerFadeInDuration;
	BannerFadeOutDuration = S->BannerFadeOutDuration;
	BannerSpawnZOffset   = S->BannerSpawnZOffset;
	AnticipationSeconds  = S->AnticipationSeconds;

	bShowAltitudeMarkers   = S->bShowAltitudeMarkers;
	AltitudeMarkerInterval = S->AltitudeMarkerInterval;
	MarkerColor            = S->MarkerColor;

	bShowHUDTelemetry  = S->bShowHUDTelemetry;
	bShowHUDEvents     = S->bShowHUDEvents;
	bShowDebugMessages = S->bShowDebugMessages;

	bEnableDeckLink         = S->bEnableDeckLink;
	DeckLinkOutputResolution = S->DeckLinkOutputResolution;
	bDeckLinkFillAndKey     = S->bDeckLinkFillAndKey;
	bDeckLinkAutoStart      = S->bDeckLinkAutoStart;
	bDeckLinkGenlock        = S->bDeckLinkGenlock;
	bDeckLinkTimecode       = S->bDeckLinkTimecode;

	bUseCSVProvider = S->bUseCSVProvider;
	CSVFilePath     = S->CSVFilePath;

	UE_LOG(LogRocketAR, Log, TEXT("OperatorSettings applied to setup actor"));
}

void ARocketARSetupActor::CopyStateToOperatorSettings(URocketAROperatorSettings* S) const
{
	if (!S) return;

	S->LaunchPadLatitude  = LaunchPadLatitude;
	S->LaunchPadLongitude = LaunchPadLongitude;
	S->LaunchPadAltitude  = LaunchPadAltitude;

	S->CameraMountOffset   = CameraMountOffset;
	S->CameraMountRotation = CameraMountRotation;
	S->CameraOpticalRoll   = CameraOpticalRoll;
	S->CameraHFOV          = CameraHFOV;
	S->ActiveCameraRigIndex = ActiveCameraRigIndex;

	S->BannerWidth          = BannerWidth;
	S->BannerHeight         = BannerHeight;
	S->BannerRotationYaw    = BannerRotationYaw;
	S->MaxActiveBanners     = MaxActiveBanners;
	S->BannerTextSize       = BannerTextSize;
	S->TextSDFSharpness     = TextSDFSharpness;
	S->SlideSpeed           = SlideSpeed;
	S->SlideDuration        = SlideDuration;
	S->BannerFadeInDuration = BannerFadeInDuration;
	S->BannerFadeOutDuration = BannerFadeOutDuration;
	S->BannerSpawnZOffset   = BannerSpawnZOffset;
	S->AnticipationSeconds  = AnticipationSeconds;

	S->bShowAltitudeMarkers   = bShowAltitudeMarkers;
	S->AltitudeMarkerInterval = AltitudeMarkerInterval;
	S->MarkerColor            = MarkerColor;

	S->bShowHUDTelemetry  = bShowHUDTelemetry;
	S->bShowHUDEvents     = bShowHUDEvents;
	S->bShowDebugMessages = bShowDebugMessages;

	S->bEnableDeckLink         = bEnableDeckLink;
	S->DeckLinkOutputResolution = DeckLinkOutputResolution;
	S->bDeckLinkFillAndKey     = bDeckLinkFillAndKey;
	S->bDeckLinkAutoStart      = bDeckLinkAutoStart;
	S->bDeckLinkGenlock        = bDeckLinkGenlock;
	S->bDeckLinkTimecode       = bDeckLinkTimecode;

	S->bUseCSVProvider = bUseCSVProvider;
	S->CSVFilePath     = CSVFilePath;
}

void ARocketARSetupActor::SetupOperatorConsole()
{
	if (!OperatorConsoleClass) return;

	UWorld* World = GetWorld();
	if (!World) return;
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	OperatorConsole = CreateWidget<UOperatorConsoleWidget>(PC, OperatorConsoleClass);
	if (!OperatorConsole)
	{
		UE_LOG(LogRocketAR, Warning, TEXT("OperatorConsole: failed to create widget from %s"),
			*OperatorConsoleClass->GetName());
		return;
	}

	OperatorConsole->SetSetupActor(this);
	OperatorConsole->AddToViewport(50); // Z-order above HUD canvas
	OperatorConsole->SetVisibility(ESlateVisibility::Collapsed);
	bOperatorConsoleVisible = false;

	UE_LOG(LogRocketAR, Log, TEXT("OperatorConsole spawned (press F12 to toggle)"));
}

void ARocketARSetupActor::ToggleOperatorConsole()
{
	if (!OperatorConsole) return;

	bOperatorConsoleVisible = !bOperatorConsoleVisible;
	OperatorConsole->SetVisibility(bOperatorConsoleVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (bOperatorConsoleVisible)
	{
		OperatorConsole->RefreshFromSetupActor();
	}

	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PC->bShowMouseCursor = bOperatorConsoleVisible;
		if (bOperatorConsoleVisible)
		{
			FInputModeGameAndUI Mode;
			Mode.SetWidgetToFocus(OperatorConsole->TakeWidget());
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(Mode);
		}
		else
		{
			PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));
		}
	}

	UE_LOG(LogRocketAR, Log, TEXT("OperatorConsole: %s"), bOperatorConsoleVisible ? TEXT("SHOWN") : TEXT("HIDDEN"));
}
