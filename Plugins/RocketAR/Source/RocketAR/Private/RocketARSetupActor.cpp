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
#include "AltitudeMarkerActor.h"
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

	// Attach camera to unscaled rocket mount point so it moves rigidly with the rocket
	if (CameraManager && DevVisActor && DevVisActor->GetRocketMountPoint())
	{
		CameraManager->AttachToComponent(DevVisActor->GetRocketMountPoint());
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

	// Spawn a test banner
	FFlightEventData TestEvent;
	TestEvent.EventType = EFlightEvent::MaxQ;
	TestEvent.EventLabel = FreezeFrameEventLabel;
	TestEvent.MET = 60.0;
	TestEvent.Altitude = FreezeFrameAltitude;
	TestEvent.Velocity = 500.0;

	if (BannerManager)
	{
		ABannerActor* TestBanner = BannerManager->SpawnBanner(TestEvent);
		if (TestBanner)
		{
			TestBanner->BannerRotationOffset = BannerRotationOffset;
		}
	}

	// Also fire the event disk if enabled
	if (DevVisActor && bDevVisualization && bShowEventDisks)
	{
		DevVisActor->SpawnEventDisk(RocketUEPos, FQuat::Identity, FreezeFrameEventLabel);
	}

	// Update HUD
	if (HUDOverlay && bShowHUD)
	{
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

	// Configure banner manager (setup actor handles event→banner spawning, not BannerManager directly)
	if (BannerManager)
	{
		BannerManager->BannerArcRadius = BannerArcRadius;
		BannerManager->BannerArcAngle = BannerArcAngle;
		BannerManager->BannerArcHeight = BannerArcHeight;
		BannerManager->BannerLifetimeSeconds = BannerLifetimeSeconds;
		BannerManager->MaxActiveBanners = MaxActiveBanners;
		BannerManager->BannerMaterial = BannerMaterial;
		BannerManager->BannerFont = BannerFont;
	}

	// Wire event detector — setup actor is sole handler for event→banner/disk/HUD
	if (EventDetector)
	{
		EventDetector->OnFlightEvent.AddDynamic(this, &ARocketARSetupActor::OnFlightEventDetected);
	}
}

void ARocketARSetupActor::OnTelemetryUpdated(const FProcessedTelemetryData& Data)
{
	// Cache vehicle transform for event disk placement
	LastVehicleUEPosition = Data.UEPosition;
	LastVehicleUERotation = Data.UERotation;
	LastAltitudeASL = Data.AltitudeASL;
	LastVerticalVelocity = Data.VerticalVelocity;

	// Convert ECEF velocity to UE space for predictive banner placement
	// Transform (pos + vel) to UE, subtract transformed pos — gives UE velocity vector
	const FVector ECEFPos = Data.RawData.VehiclePosition;
	const FVector ECEFVel = Data.RawData.VehicleVelocity;
#if WITH_CESIUM
	if (Georeference && !ECEFVel.IsNearlyZero())
	{
		const FVector PosAheadUE = Georeference->TransformEarthCenteredEarthFixedPositionToUnreal(ECEFPos + ECEFVel);
		LastVehicleUEVelocity = PosAheadUE - Data.UEPosition; // UE units per second (cm/s)
	}
	else
#endif
	{
		LastVehicleUEVelocity = FVector::ZeroVector;
	}

	// Update pre-placed altitude banners along current trajectory
	UpdatePrePlacedBanners();

	// Update banner manager with current position
	if (BannerManager)
	{
		BannerManager->UpdateVehiclePosition(Data.UEPosition);
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
	if (HUDOverlay && bShowHUD)
	{
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
	if (!HUDOverlay && bShowHUD)
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
		BannerManager->BannerArcRadius = BannerArcRadius;
		BannerManager->BannerArcAngle = BannerArcAngle;
		BannerManager->BannerArcHeight = BannerArcHeight;
		BannerManager->BannerLifetimeSeconds = BannerLifetimeSeconds;
		BannerManager->MaxActiveBanners = MaxActiveBanners;
		BannerManager->BannerMaterial = BannerMaterial;
		BannerManager->BannerFont = BannerFont;
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
		// Clear old test banners and disks
		if (BannerManager) BannerManager->DestroyAllBanners();
		if (DevVisActor) DevVisActor->ClearEventDisks();

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
	// Skip altitude markers entirely when disabled
	if (EventData.EventType == EFlightEvent::AltitudeMarker && !bShowAltitudeMarkers)
	{
		return;
	}

	UE_LOG(LogRocketAR, Log, TEXT("Flight event: %s at MET=%.1f, Alt=%.0fm"),
		*EventData.EventLabel, EventData.MET, EventData.Altitude);

	// Spawn banner at predicted position (velocity * lead time ahead of current position)
	if (EventData.EventType != EFlightEvent::AltitudeMarker)
	{
		FVector SpawnPosition = LastVehicleUEPosition;
		if (BannerLeadTimeSeconds > 0.0f && !LastVehicleUEVelocity.IsNearlyZero())
		{
			SpawnPosition = LastVehicleUEPosition + LastVehicleUEVelocity * BannerLeadTimeSeconds;
		}

		if (DevVisActor && bDevVisualization && bShowEventDisks)
		{
			DevVisActor->SpawnEventDisk(SpawnPosition, LastVehicleUERotation, EventData.EventLabel);
		}

		if (BannerManager)
		{
			ABannerActor* Banner = BannerManager->SpawnBannerAtPosition(EventData, SpawnPosition);
			if (Banner)
			{
				Banner->BannerRotationOffset = BannerRotationOffset;
			}
		}
	}

	// Show event on HUD overlay
	if (HUDOverlay && bShowHUD)
	{
		HUDOverlay->ShowEvent(EventData);
	}
}

bool ARocketARSetupActor::IsAltitudeBasedEvent(EFlightEvent EventType) const
{
	return EventType == EFlightEvent::AltitudeMarker;
}

void ARocketARSetupActor::UpdatePrePlacedBanners()
{
	if (!bShowAltitudeMarkers) return;
	if (LastVerticalVelocity <= 10.0) return; // Only during ascent with meaningful velocity
	if (LastVehicleUEVelocity.IsNearlyZero()) return;

	const float Interval = AltitudeMarkerInterval;
	if (Interval <= 0.0f) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// Find the next N altitude markers above current altitude
	const double NextMarkerBase = FMath::CeilToDouble(LastAltitudeASL / Interval) * Interval;

	for (int32 i = 0; i < PrePlaceLookAheadCount; ++i)
	{
		const double MarkerAlt = NextMarkerBase + i * Interval;
		if (MarkerAlt <= 0.0) continue;

		// Already have a marker for this altitude?
		if (PrePlacedAltitudeMarkers.Contains(MarkerAlt))
		{
			// Update its position along current trajectory
			AAltitudeMarkerActor* Existing = PrePlacedAltitudeMarkers[MarkerAlt];
			if (Existing && IsValid(Existing))
			{
				const double TimeToReach = (MarkerAlt - LastAltitudeASL) / FMath::Max(LastVerticalVelocity, 1.0);
				const FVector PredictedPos = LastVehicleUEPosition + LastVehicleUEVelocity * TimeToReach;
				Existing->SetActorLocation(PredictedPos);
			}
			continue;
		}

		// Don't pre-place markers we've already passed
		if (MarkerAlt <= LastAltitudeASL) continue;

		// Predict position for this altitude marker
		const double TimeToReach = (MarkerAlt - LastAltitudeASL) / FMath::Max(LastVerticalVelocity, 1.0);
		const FVector PredictedPos = LastVehicleUEPosition + LastVehicleUEVelocity * TimeToReach;

		// Format label
		FString Label;
		if (MarkerAlt >= 1000.0)
		{
			Label = FString::Printf(TEXT("%.0f km"), MarkerAlt / 1000.0);
		}
		else
		{
			Label = FString::Printf(TEXT("%.0f m"), MarkerAlt);
		}

		// Spawn altitude marker actor directly
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AAltitudeMarkerActor* Marker = World->SpawnActor<AAltitudeMarkerActor>(
			AAltitudeMarkerActor::StaticClass(), PredictedPos, FRotator::ZeroRotator, SpawnParams);

		if (Marker)
		{
			Marker->InitMarker(Label, MarkerAlt,
				MarkerArcAngle, MarkerArcRadius, MarkerArcHeight, 32,
				MarkerMaterial, MarkerFont);
			Marker->LifetimeSeconds = 0.0f; // We manage lifecycle, not auto-fade
			Marker->MarkerRotationOffset = MarkerRotationOffset;
			PrePlacedAltitudeMarkers.Add(MarkerAlt, Marker);

			UE_LOG(LogRocketAR, Log, TEXT("Pre-placed altitude marker: %s at predicted pos (%.0f, %.0f, %.0f) T=%.1fs ahead"),
				*Label, PredictedPos.X, PredictedPos.Y, PredictedPos.Z, TimeToReach);
		}
	}

	// Clean up markers we've passed (below current altitude minus some margin)
	TArray<double> ToRemove;
	for (auto& Pair : PrePlacedAltitudeMarkers)
	{
		if (Pair.Key < LastAltitudeASL - Interval)
		{
			if (Pair.Value && IsValid(Pair.Value))
			{
				Pair.Value->StartFadeOut();
			}
			ToRemove.Add(Pair.Key);
		}
	}
	for (double Alt : ToRemove)
	{
		PrePlacedAltitudeMarkers.Remove(Alt);
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
