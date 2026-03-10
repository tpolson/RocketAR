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

	// --- Banner Configuration ---

	/** Banner width in cm (100m default, 4:1 ratio with height) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	float BannerWidth = 10000.0f; // cm (100m)

	/** Banner height in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	float BannerHeight = 10000.0f; // cm (100m)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	int32 MaxActiveBanners = 20;

	/** Text size for event banners (cm, UTextRenderComponent WorldSize) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Text")
	float BannerTextSize = 200.0f;

	/** Local offset of text from banner root (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Text")
	FVector BannerTextOffset = FVector::ZeroVector;

	// --- Banner Slide Configuration ---

	/** Delay (seconds) between event detection and banner spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Slide")
	float TriggerTimeOffset = 0.0f;

	/** Constant velocity of slide in cm/s (50 m/s default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Slide")
	float SlideSpeed = 5000.0f;

	/** Time banner slides before fade begins */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Slide")
	float SlideDuration = 10.0f;

	/** Opacity ramp-up time at spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Slide")
	float BannerFadeInDuration = 0.3f;

	/** Opacity ramp-down time before destruction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Slide")
	float BannerFadeOutDuration = 1.0f;

	/** Local Z offset above vehicle center for event banners (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Slide")
	float BannerSpawnZOffset = 8000.0f;

	/** Local Z offset above vehicle center for altitude markers (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Slide")
	float MarkerSpawnZOffset = 6000.0f;

	/** Seconds before trigger time to begin spawn (anticipation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Slide")
	float AnticipationSeconds = 1.5f;

	// --- Altitude Marker Configuration ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	bool bShowAltitudeMarkers = false;

	/** Altitude interval between markers (meters). 1000km = 1000000. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	float AltitudeMarkerInterval = 10000.0f;

	/** Altitude marker width in cm (smaller than event banners) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	float MarkerWidth = 4000.0f; // cm (40m)

	/** Altitude marker height in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	float MarkerHeight = 4000.0f; // cm (40m)

	/** Altitude marker color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	FLinearColor MarkerColor = FLinearColor(0.2f, 0.8f, 1.0f, 1.0f); // cyan

	/** Text size for altitude markers (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker|Text")
	float MarkerTextSize = 150.0f;

	/** Local offset of text from marker root (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker|Text")
	FVector MarkerTextOffset = FVector::ZeroVector;

	/** Seconds of look-ahead for predictive altitude marker firing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	float AltitudeMarkerAnticipation = 2.0f;

	// --- Dev / Debug ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dev Visualization")
	bool bDevVisualization = true;

	/** Use opaque banner material for dev wireframe visibility (no alpha/fade) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dev Visualization")
	bool bDevOpaqueBanners = false;

	/** Rocket body height in meters (pivot at engine end, grows upward). SLS Block 1 = 98m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dev Visualization")
	float RocketHeight = 98.0f;

	/** Rocket body radius in meters. SLS Block 1 core stage = 4.2m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dev Visualization")
	float RocketRadius = 4.2f;

	/** Show bottom-left MET/ALT/VEL telemetry box */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bShowHUDTelemetry = true;

	/** Show top-center event text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bShowHUDEvents = true;

	/** Show top-left debug messages (BANNER:/ALTITUDE: spawn text) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bShowDebugMessages = true;

	// --- Dev Camera ---

	/** Enable the dev inspection camera (parented to rocket, adjustable position/rotation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dev Camera")
	bool bUseDevCamera = false;

	/** Dev camera offset from rocket root (cm). Z = along rocket axis toward nose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dev Camera", meta=(EditCondition="bUseDevCamera"))
	FVector DevCameraOffset = FVector(0.0f, 0.0f, 15000.0f);

	/** Dev camera rotation relative to rocket. Default: looking down the rocket body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dev Camera", meta=(EditCondition="bUseDevCamera"))
	FRotator DevCameraRotation = FRotator(-90.0f, 0.0f, 0.0f);

	/** Dev camera field of view (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dev Camera", meta=(EditCondition="bUseDevCamera", ClampMin="1.0", ClampMax="180.0"))
	float DevCameraFOV = 90.0f;

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
	void SetupDevCamera();
	void SetupHUD();
	void SetupFreezeFrame();
	void WireSubsystems();
	bool bDevVisLastState = false;
	bool bCameraViewSet = false;
	bool bDevCameraLastState = false;
	float FreezeFrameAltitudeLast = -1.0f;
	FString FreezeFrameEventLabelLast;

	UFUNCTION()
	void OnTelemetryUpdated(const FProcessedTelemetryData& Data);

	UFUNCTION()
	void OnFlightEventDetected(const FFlightEventData& EventData);

	/** Cached from last telemetry update for event disk and banner placement */
	FVector LastVehicleUEPosition = FVector::ZeroVector;
	FVector LastVehicleUEVelocity = FVector::ZeroVector;
	FVector PrevUEPosition = FVector::ZeroVector;
	FQuat LastVehicleUERotation = FQuat::Identity;
	double LastAltitudeASL = 0.0;
	double LastVerticalVelocity = 0.0;

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
	ACineCameraActor* DevCameraActor = nullptr;

	UPROPERTY()
	ARocketARHUD* HUDOverlay = nullptr;
};
