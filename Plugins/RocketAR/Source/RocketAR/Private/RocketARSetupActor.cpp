#include "RocketARSetupActor.h"
#include "TelemetrySubsystem.h"
#include "FlightEventDetector.h"
#include "BannerManager.h"
#include "RocketARCameraManager.h"
#include "CSVTelemetryProvider.h"
#include "DevVisualizationActor.h"
#include "RocketARModule.h"
#include "CineCameraActor.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
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

	if (bUseCSVProvider)
	{
		SetupCSVProvider();
	}

	SetupDevVisualization();
	WireSubsystems();

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
		DevVisActor->SetVisible(bDevVisualization);
		bDevVisLastState = bDevVisualization;
		UE_LOG(LogRocketAR, Log, TEXT("Dev visualization actor spawned (visible=%s)"),
			bDevVisualization ? TEXT("true") : TEXT("false"));
	}

	// Spawn a directional light (sun) for dev lighting
	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(
		ADirectionalLight::StaticClass(),
		FVector::ZeroVector,
		FRotator(-45.0f, -30.0f, 0.0f),
		SpawnParams);
	if (Sun)
	{
		Sun->GetLightComponent()->SetIntensity(3.14f);
		Sun->GetLightComponent()->SetLightColor(FLinearColor(1.0f, 0.95f, 0.85f));
		UE_LOG(LogRocketAR, Log, TEXT("Dev sun light spawned"));
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

	// Wire banner manager to event detector
	if (BannerManager && EventDetector)
	{
		BannerManager->BannerArcRadius = BannerArcRadius;
		BannerManager->BannerArcAngle = BannerArcAngle;
		BannerManager->BannerArcHeight = BannerArcHeight;
		BannerManager->BannerLifetimeSeconds = BannerLifetimeSeconds;
		BannerManager->MaxActiveBanners = MaxActiveBanners;
		BannerManager->BannerMaterial = BannerMaterial;
		BannerManager->BannerFont = BannerFont;
		BannerManager->SetEventDetector(EventDetector);
	}
}

void ARocketARSetupActor::OnTelemetryUpdated(const FProcessedTelemetryData& Data)
{
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

	// Sync config changes at runtime
	if (CameraManager)
	{
		CameraManager->CameraMountOffset = CameraMountOffset;
		CameraManager->CameraMountRotation = CameraMountRotation;
		CameraManager->CameraHFOV = CameraHFOV;
	}

	// Toggle dev visualization when the bool changes
	if (DevVisActor && bDevVisualization != bDevVisLastState)
	{
		DevVisActor->SetVisible(bDevVisualization);
		bDevVisLastState = bDevVisualization;
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
