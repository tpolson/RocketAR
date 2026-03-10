#include "RocketARSetupActor.h"
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

	// Create components
	BannerManager = CreateDefaultSubobject<UBannerManager>(TEXT("BannerManager"));
	CameraManager = CreateDefaultSubobject<URocketARCameraManager>(TEXT("CameraManager"));
}

void ARocketARSetupActor::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogRocketAR, Log, TEXT("RocketAR Setup Actor: BeginPlay"));

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

	// Attach camera and banners to unscaled rocket mount point so they move rigidly with the rocket
	if (DevVisActor && DevVisActor->GetRocketMountPoint())
	{
		if (CameraManager)
		{
			CameraManager->AttachToComponent(DevVisActor->GetRocketMountPoint());
		}
		if (BannerManager)
		{
			BannerManager->SetAttachTarget(DevVisActor->GetRocketMountPoint());
		}
	}

	WireSubsystems();
	SetupHUD();

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
		DevVisActor->SetVisible(bDevVisualization);
		bDevVisLastState = bDevVisualization;
		UE_LOG(LogRocketAR, Log, TEXT("Dev visualization actor spawned (visible=%s)"),
			bDevVisualization ? TEXT("true") : TEXT("false"));
	}

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
		FakeData.AltitudeASL = FreezeFrameAltitude;
		FakeData.VelocityMagnitude = 500.0;
		FakeData.RawData.MissionElapsedTime = 60.0;
		FakeData.RawData.bTelemetryValid = true;
		DevVisActor->UpdateFromTelemetry(FakeData);

		// Second update so velocity orientation works (needs two positions)
		FakeData.UEPosition = RocketUEPos + FVector(0.0, 0.0, 100.0);
		DevVisActor->UpdateFromTelemetry(FakeData);

		// Set back to exact position
		FakeData.UEPosition = RocketUEPos;
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
		BannerManager->BannerDiskRadius = BannerDiskRadius;
		BannerManager->BannerDiskThickness = BannerDiskThickness;
		BannerManager->MaxActiveBanners = MaxActiveBanners;
		BannerManager->BannerMaterial = BannerMaterial;
		BannerManager->BannerFont = BannerFont;
		BannerManager->TriggerTimeOffset = TriggerTimeOffset;
		BannerManager->SlideSpeed = SlideSpeed;
		BannerManager->SlideDuration = SlideDuration;
		BannerManager->FadeInDuration = BannerFadeInDuration;
		BannerManager->BannerSpawnZOffset = BannerSpawnZOffset;
		BannerManager->MarkerSpawnZOffset = MarkerSpawnZOffset;
		BannerManager->AnticipationSeconds = AnticipationSeconds;
	}

	// Wire event detector — setup actor is sole handler for event→banner/HUD
	if (EventDetector)
	{
		EventDetector->OnFlightEvent.AddDynamic(this, &ARocketARSetupActor::OnFlightEventDetected);
	}

	// Sync marker geometry and debug flag to banner manager
	if (BannerManager)
	{
		BannerManager->MarkerDiskRadius = MarkerDiskRadius;
		BannerManager->MarkerDiskThickness = MarkerDiskThickness;
		BannerManager->MarkerColor = MarkerColor;
		BannerManager->bShowDebugMessages = bShowDebugMessages;
	}

	// Sync full event config, then overlay convenience properties
	if (EventDetector)
	{
		EventDetector->Config = EventConfig;
		EventDetector->Config.AltitudeMarkerInterval = AltitudeMarkerInterval;
		EventDetector->Config.AltitudeMarkerAnticipation = AltitudeMarkerAnticipation;
	}
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
	if (CameraActor && !bCameraViewSet)
	{
		APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if (PC)
		{
			PC->SetViewTarget(CameraActor);
			bCameraViewSet = true;
			UE_LOG(LogRocketAR, Log, TEXT("Camera view target set to CineCameraActor"));
		}
	}

	// Retry HUD setup if PC wasn't ready during BeginPlay
	if (!HUDOverlay && (bShowHUDTelemetry || bShowHUDEvents))
	{
		SetupHUD();
	}

	// Sync config changes at runtime
	if (CameraManager)
	{
		CameraManager->CameraMountOffset = CameraMountOffset;
		CameraManager->CameraMountRotation = CameraMountRotation;
		CameraManager->CameraOpticalRoll = CameraOpticalRoll;
		CameraManager->CameraHFOV = CameraHFOV;
	}

	// Live-sync banner config so new banners use current values
	if (BannerManager)
	{
		BannerManager->BannerDiskRadius = BannerDiskRadius;
		BannerManager->BannerDiskThickness = BannerDiskThickness;
		BannerManager->MaxActiveBanners = MaxActiveBanners;
		BannerManager->BannerMaterial = BannerMaterial;
		BannerManager->BannerFont = BannerFont;
		BannerManager->TriggerTimeOffset = TriggerTimeOffset;
		BannerManager->SlideSpeed = SlideSpeed;
		BannerManager->SlideDuration = SlideDuration;
		BannerManager->FadeInDuration = BannerFadeInDuration;
		BannerManager->BannerSpawnZOffset = BannerSpawnZOffset;
		BannerManager->MarkerSpawnZOffset = MarkerSpawnZOffset;
		BannerManager->AnticipationSeconds = AnticipationSeconds;
	}

	// Live-sync marker geometry and debug flag
	if (BannerManager)
	{
		BannerManager->MarkerDiskRadius = MarkerDiskRadius;
		BannerManager->MarkerDiskThickness = MarkerDiskThickness;
		BannerManager->MarkerColor = MarkerColor;
		BannerManager->bShowDebugMessages = bShowDebugMessages;
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

	// Toggle dev visualization when the bool changes
	if (DevVisActor && bDevVisualization != bDevVisLastState)
	{
		DevVisActor->SetVisible(bDevVisualization);
		bDevVisLastState = bDevVisualization;
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
