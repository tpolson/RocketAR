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
	MAX                 UMETA(Hidden)
};

/**
 * Configuration for a single flight event's detection thresholds.
 * Data-driven so other vehicles can be supported by changing config.
 */
USTRUCT(BlueprintType)
struct ROCKETAR_API FFlightEventConfig
{
	GENERATED_BODY()

	/** Liftoff altitude threshold (meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	float LiftoffAltitudeThreshold = 1.0f;

	/** Max-Q sliding window: minimum rising duration (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	float MaxQRisingDuration = 20.0f;

	/** Max-Q: percentage drop below peak to confirm (0.05 = 5%) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	float MaxQDropPercent = 0.05f;

	/** Max-Q: confirmation window — no higher Q in this many seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	float MaxQConfirmationWindow = 1.0f;

	/** Thrust threshold for engine on/off detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	float ThrustOnThreshold = 0.01f;

	/** SRB engine indices (first N engines are SRBs) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	int32 SRBEngineCount = 2;

	/** Core engine indices (engines after SRBs) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	int32 CoreEngineCount = 4;

	/** Altitude marker interval (meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	float AltitudeMarkerInterval = 10000.0f;

	/** Minimum spacing between altitude markers (meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	float AltitudeMarkerMinSpacing = 5000.0f;

	/** Seconds of look-ahead for predictive altitude marker firing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	float AltitudeMarkerAnticipation = 2.0f;

	/** Reentry dynamic pressure threshold (Pa) — reentry starts when Q rises above this */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	float ReentryQThreshold = 1000.0f;

	/** Chute deployment altitude (meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	float ChuteDeployAltitude = 8000.0f;

	/** Splashdown altitude threshold (meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events")
	float SplashdownAltitude = 10.0f;
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

	/** ECEF position at event (for banner placement) */
	UPROPERTY(BlueprintReadOnly, Category = "Events")
	FVector ECEFPosition = FVector::ZeroVector;
};
