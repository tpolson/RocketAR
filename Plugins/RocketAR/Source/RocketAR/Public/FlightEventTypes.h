#pragma once

#include "CoreMinimal.h"
#include "FlightEventTypes.generated.h"

UENUM(BlueprintType)
enum class EFlightEvent : uint8
{
	Ignition            UMETA(DisplayName = "Ignition"),
	Liftoff             UMETA(DisplayName = "Liftoff"),
	Mach1               UMETA(DisplayName = "Mach 1"),
	MaxQ                UMETA(DisplayName = "Max Q"),
	SRBIgnition         UMETA(DisplayName = "SRB Ignition"),
	SRBSeparation       UMETA(DisplayName = "SRB Separation"),
	MECO                UMETA(DisplayName = "MECO"),
	StageSeparation     UMETA(DisplayName = "Stage Separation"),
	SecondStageIgnition UMETA(DisplayName = "Second Stage Ignition"),
	FairingJettison     UMETA(DisplayName = "Fairing Jettison"),
	SecondStageCutoff   UMETA(DisplayName = "SECO"),
	Apogee              UMETA(DisplayName = "Apogee"),
	ReentryStart        UMETA(DisplayName = "Reentry Start"),
	ChuteDeployment     UMETA(DisplayName = "Chute Deployment"),
	Splashdown          UMETA(DisplayName = "Splashdown"),
	AltitudeMarker      UMETA(DisplayName = "Altitude Marker"),
	Custom              UMETA(DisplayName = "Custom Event"),
	MAX                 UMETA(Hidden)
};

/** Metric used by custom threshold events */
UENUM(BlueprintType)
enum class ECustomEventMetric : uint8
{
	Altitude        UMETA(DisplayName = "Altitude (m)"),
	Velocity        UMETA(DisplayName = "Velocity (m/s)"),
	MachNumber      UMETA(DisplayName = "Mach Number"),
	DynamicPressure UMETA(DisplayName = "Dynamic Pressure (Pa)"),
	GForce          UMETA(DisplayName = "G-Force"),
	MET             UMETA(DisplayName = "Mission Elapsed Time (s)")
};

/** Edge direction for custom threshold events */
UENUM(BlueprintType)
enum class ECustomEventDirection : uint8
{
	RisingEdge  UMETA(DisplayName = "Rising Edge (crosses above)"),
	FallingEdge UMETA(DisplayName = "Falling Edge (crosses below)")
};

/**
 * Per-event enable/disable and label override.
 * Empty LabelOverride = use C++ default label (zero regression risk).
 * Label supports tokens: {alt_km}, {alt_m}, {vel}, {mach}, {q_pa}, {met}, {gforce}, {extra}
 */
USTRUCT(BlueprintType)
struct ROCKETAR_API FBuiltinEventOverride
{
	GENERATED_BODY()

	/** Which built-in event to override */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	EFlightEvent EventType = EFlightEvent::Ignition;

	/** Whether this event is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	bool bEnabled = true;

	/** Custom label (empty = use C++ default). Supports tokens: {alt_km}, {vel}, {mach}, {q_pa}, {met}, {gforce}, {extra} */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	FString LabelOverride;
};

/**
 * Definition for a user-defined custom threshold event.
 */
USTRUCT(BlueprintType)
struct ROCKETAR_API FCustomEventDefinition
{
	GENERATED_BODY()

	/** Unique identifier for this event (used as latch key) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	FName EventId;

	/** Whether this custom event is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	bool bEnabled = true;

	/** Which telemetry metric to watch */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	ECustomEventMetric Metric = ECustomEventMetric::Altitude;

	/** Threshold value for the metric */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	double Threshold = 0.0;

	/** Fire when metric crosses above or below threshold */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	ECustomEventDirection Direction = ECustomEventDirection::RisingEdge;

	/** Banner label. Supports tokens: {alt_km}, {alt_m}, {vel}, {mach}, {q_pa}, {met}, {gforce}, {extra} */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	FString Label = TEXT("CUSTOM EVENT");

	/** Built-in event that must have fired before this custom event can fire (MAX = no prerequisite) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	EFlightEvent Prerequisite = EFlightEvent::MAX;
};

/**
 * Configuration for a single flight event's detection thresholds.
 * Data-driven so other vehicles can be supported by changing config.
 */
USTRUCT(BlueprintType)
struct ROCKETAR_API FFlightEventConfig
{
	GENERATED_BODY()

	// --- Detection Thresholds ---

	/** Liftoff altitude threshold (meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds")
	float LiftoffAltitudeThreshold = 1.0f;

	/** Mach threshold for the Mach 1 event (default 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds")
	float Mach1Threshold = 1.0f;

	/** Max-Q sliding window: minimum rising duration (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds")
	float MaxQRisingDuration = 20.0f;

	/** Max-Q: percentage drop below peak to confirm (0.05 = 5%) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds")
	float MaxQDropPercent = 0.05f;

	/** Max-Q: confirmation window — no higher Q in this many seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds")
	float MaxQConfirmationWindow = 1.0f;

	/** Thrust threshold for engine on/off detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds")
	float ThrustOnThreshold = 0.01f;

	/** Delay after MECO before stage separation fires (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds")
	float StageSeparationDelay = 3.0f;

	/** Altitude above which fairing jettison can fire (meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds")
	float FairingAltitudeThreshold = 100000.0f;

	/** Dynamic pressure below which fairing jettison can fire (Pa) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds")
	float FairingQThreshold = 1.0f;

	/** Reentry dynamic pressure threshold (Pa) — reentry starts when Q rises above this */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds")
	float ReentryQThreshold = 1000.0f;

	/** Chute deployment altitude (meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds")
	float ChuteDeployAltitude = 8000.0f;

	/** Splashdown altitude threshold (meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds")
	float SplashdownAltitude = 10.0f;

	// --- Engine Layout ---

	/** SRB engine indices (first N engines are SRBs) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Engine Layout")
	int32 SRBEngineCount = 2;

	/** Core engine indices (engines after SRBs) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Engine Layout")
	int32 CoreEngineCount = 4;

	// --- Altitude Markers ---

	/** Altitude marker interval (meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Markers")
	float AltitudeMarkerInterval = 10000.0f;

	/** Minimum spacing between altitude markers (meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Markers")
	float AltitudeMarkerMinSpacing = 5000.0f;

	/** Seconds of look-ahead for predictive altitude marker firing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Markers")
	float AltitudeMarkerAnticipation = 2.0f;

	// --- Per-Event Overrides ---

	/** Per-event enable/disable and label overrides. Add entries only for events you want to customize. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event Overrides", meta=(DisplayName="Built-in Event Overrides"))
	TArray<FBuiltinEventOverride> EventOverrides;

	// --- Custom Events ---

	/** User-defined threshold events (e.g., Mach 2, Karman Line) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Events")
	TArray<FCustomEventDefinition> CustomEvents;

};

/**
 * Data about a detected flight event.
 */
USTRUCT(BlueprintType)
struct ROCKETAR_API FFlightEventData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Events")
	EFlightEvent EventType = EFlightEvent::MAX;

	/** Mission elapsed time when event occurred */
	UPROPERTY(BlueprintReadOnly, Category = "Events")
	double MET = 0.0;

	/** Altitude at event (meters ASL) */
	UPROPERTY(BlueprintReadOnly, Category = "Events")
	double Altitude = 0.0;

	/** Velocity at event (m/s) */
	UPROPERTY(BlueprintReadOnly, Category = "Events")
	double Velocity = 0.0;

	/** Display label for the banner */
	UPROPERTY(BlueprintReadOnly, Category = "Events")
	FString EventLabel;

	/** Custom event identifier (set for Custom events, empty for built-in) */
	UPROPERTY(BlueprintReadOnly, Category = "Events")
	FName CustomEventId;

	/** ECEF position at event (for banner placement) */
	UPROPERTY(BlueprintReadOnly, Category = "Events")
	FVector ECEFPosition = FVector::ZeroVector;
};
