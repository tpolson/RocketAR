#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TelemetryProvider.h"
#include "TelemetryTypes.h"
#include "FlightEventTypes.h"
#include "RocketARSetupActor.generated.h"

class UTelemetrySubsystem;
class UFlightEventDetector;
class UBannerManager;
class URocketARCameraManager;
class ACesiumGeoreference;
class ACineCameraActor;
class ACSVTelemetryProvider;
class ABannerActor;
class AAltitudeMarkerActor;
class ADevVisualizationActor;
class ARocketARHUD;

/**
 * Master setup actor that wires all RocketAR systems together.
 * Implements ITelemetryProvider using exposed Blueprint variables (Method B)
 * so clients can directly set telemetry values without implementing the interface themselves.
 *
 * All configuration is exposed as UPROPERTY for easy Blueprint editing.
 */
UCLASS(BlueprintType, Blueprintable)
class ROCKETAR_API ARocketARSetupActor : public AActor, public ITelemetryProvider
{
	GENERATED_BODY()

public:
	ARocketARSetupActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// --- ITelemetryProvider interface (Method B: direct Blueprint variables) ---
	virtual FTelemetryInputData GetTelemetryData_Implementation() const override;
	virtual bool IsTelemetryAvailable_Implementation() const override;
	virtual int32 GetProviderPriority_Implementation() const override;

	/** Convenience function: set all telemetry data at once */
	UFUNCTION(BlueprintCallable, Category = "Telemetry Input")
	void SetTelemetryData(const FTelemetryInputData& InData);

	// --- Telemetry Input Variables (Method B) ---
	// The client can wire their Blueprint plugin directly to these variables.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry Input")
	FVector InputVehiclePosition = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry Input")
	FQuat InputVehicleRotation = FQuat::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry Input")
	FVector InputVehicleVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry Input")
	FVector InputVehicleAcceleration = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry Input")
	TArray<float> InputEngineThrustPercent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry Input")
	double InputMissionElapsedTime = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry Input")
	bool bInputTelemetryValid = false;

	// --- Launch Site Configuration ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch Site")
	double LaunchPadLatitude = 34.5811;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch Site")
	double LaunchPadLongitude = -120.6257;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch Site")
	double LaunchPadAltitude = 150.0;

	// --- Camera Configuration ---

	/** Body-frame offset from vehicle center (cm). Z=along rocket axis toward nose, Y=lateral. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FVector CameraMountOffset = FVector(0.0f, 500.0f, 4000.0f);

	/** Body-frame rotation (degrees). Pitch down to look back along rocket body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FRotator CameraMountRotation = FRotator(-80.0f, -10.0f, 0.0f);

	/** Roll around the camera's optical axis (degrees, positive=CW, negative=CCW) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraOpticalRoll = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta=(ClampMin="1.0", ClampMax="180.0"))
	float CameraHFOV = 110.0f;

	// --- Banner Configuration (flight events only) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	float BannerArcRadius = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	float BannerArcAngle = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	float BannerArcHeight = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	float BannerLifetimeSeconds = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	int32 MaxActiveBanners = 20;

	/** Seconds ahead to place banner using current velocity (rocket flies into it) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	float BannerLeadTimeSeconds = 3.0f;

	/** Additional rotation offset for banners (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	FRotator BannerRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	UMaterialInterface* BannerMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	UFont* BannerFont = nullptr;

	// --- Altitude Marker Configuration ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	bool bShowAltitudeMarkers = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	float MarkerArcRadius = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	float MarkerArcAngle = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	float MarkerArcHeight = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	float AltitudeMarkerInterval = 10000.0f;

	/** Additional rotation offset for altitude markers (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	FRotator MarkerRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	UMaterialInterface* MarkerMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	UFont* MarkerFont = nullptr;

	// --- Dev / Debug ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dev")
	bool bDevVisualization = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dev")
	bool bShowEventDisks = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dev")
	bool bShowHUD = true;

	/** Freeze-frame mode: places rocket at TestAltitude with a test banner. No CSV playback. Tweak visuals live. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dev|Freeze Frame")
	bool bFreezeFrameMode = false;

	/** Altitude for freeze-frame rocket position (meters above launch pad) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dev|Freeze Frame", meta=(EditCondition="bFreezeFrameMode"))
	float FreezeFrameAltitude = 30000.0f;

	/** Label for the test banner in freeze-frame mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dev|Freeze Frame", meta=(EditCondition="bFreezeFrameMode"))
	FString FreezeFrameEventLabel = TEXT("MAX Q");

	// --- CSV Mode ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSV")
	bool bUseCSVProvider = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSV")
	FString CSVFilePath = TEXT("Data/SimulatedTelemetry.csv");


	// --- Extrapolation ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry")
	float ExtrapolationTimeout = 1.0f;

	// --- Flight Event Configuration ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	FFlightEventConfig EventConfig;

	// --- Accessors ---

	UFUNCTION(BlueprintCallable, Category = "RocketAR")
	UFlightEventDetector* GetEventDetector() const { return EventDetector; }

	UFUNCTION(BlueprintCallable, Category = "RocketAR")
	UBannerManager* GetBannerManager() const { return BannerManager; }

	UFUNCTION(BlueprintCallable, Category = "RocketAR")
	URocketARCameraManager* GetCameraManager() const { return CameraManager; }

	UFUNCTION(BlueprintCallable, Category = "RocketAR")
	ACSVTelemetryProvider* GetCSVProvider() const { return CSVProvider; }

	UFUNCTION(BlueprintCallable, Category = "RocketAR")
	ADevVisualizationActor* GetDevVisActor() const { return DevVisActor; }

private:
	void SetupGeoreference();
	void SetupCamera();
	void SetupCSVProvider();
	void SetupDevVisualization();
	void SetupHUD();
	void SetupFreezeFrame();
	void WireSubsystems();
	bool bDevVisLastState = false;
	bool bCameraViewSet = false;
	float FreezeFrameAltitudeLast = -1.0f;
	FString FreezeFrameEventLabelLast;

	UFUNCTION()
	void OnTelemetryUpdated(const FProcessedTelemetryData& Data);

	UFUNCTION()
	void OnFlightEventDetected(const FFlightEventData& EventData);

	/** Cached from last telemetry update for event disk and banner placement */
	FVector LastVehicleUEPosition = FVector::ZeroVector;
	FVector LastVehicleUEVelocity = FVector::ZeroVector;
	FQuat LastVehicleUERotation = FQuat::Identity;
	double LastAltitudeASL = 0.0;
	double LastVerticalVelocity = 0.0;

	/** Pre-placed altitude banners: altitude (m) → banner actor */
	TMap<double, AAltitudeMarkerActor*> PrePlacedAltitudeMarkers;
	double HighestPrePlacedAlt = 0.0;

	/** How many altitude markers ahead to pre-place */
	static constexpr int32 PrePlaceLookAheadCount = 5;

	void UpdatePrePlacedBanners();
	bool IsAltitudeBasedEvent(EFlightEvent EventType) const;

	UPROPERTY()
	UFlightEventDetector* EventDetector = nullptr;

	UPROPERTY()
	UBannerManager* BannerManager = nullptr;

	UPROPERTY()
	URocketARCameraManager* CameraManager = nullptr;

	UPROPERTY()
	ACesiumGeoreference* Georeference = nullptr;

	UPROPERTY()
	ACineCameraActor* CameraActor = nullptr;

	UPROPERTY()
	ACSVTelemetryProvider* CSVProvider = nullptr;

	UPROPERTY()
	UTelemetrySubsystem* TelemetrySubsystem = nullptr;

	UPROPERTY()
	ADevVisualizationActor* DevVisActor = nullptr;

	UPROPERTY()
	ARocketARHUD* HUDOverlay = nullptr;
};
