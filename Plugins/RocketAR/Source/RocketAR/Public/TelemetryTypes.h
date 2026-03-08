#pragma once

#include "CoreMinimal.h"
#include "TelemetryTypes.generated.h"

/**
 * Raw telemetry input from the client's provider.
 * All positions/velocities in ECEF frame. Rotation as ECEF-frame quaternion (XYZW, UE native FQuat).
 */
USTRUCT(BlueprintType)
struct ROCKETAR_API FTelemetryInputData
{
	GENERATED_BODY()

	/** Vehicle position in ECEF meters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry")
	FVector VehiclePosition = FVector::ZeroVector;

	/** Vehicle orientation as ECEF-frame quaternion (XYZW, UE native FQuat order) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry")
	FQuat VehicleRotation = FQuat::Identity;

	/** Velocity vector in ECEF frame (m/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry")
	FVector VehicleVelocity = FVector::ZeroVector;

	/** Acceleration in body frame (m/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry")
	FVector VehicleAcceleration = FVector::ZeroVector;

	/** Per-engine thrust percentage (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry")
	TArray<float> EngineThrustPercent;

	/** Mission elapsed time in seconds (negative = countdown) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry")
	double MissionElapsedTime = 0.0;

	/** True when telemetry data is valid */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry")
	bool bTelemetryValid = false;
};

/**
 * Processed telemetry with derived values, ready for event detection and rendering.
 */
USTRUCT(BlueprintType)
struct ROCKETAR_API FProcessedTelemetryData
{
	GENERATED_BODY()

	/** Raw input data */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FTelemetryInputData RawData;

	/** Altitude above sea level (meters) — derived from ECEF via WGS84 */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry|Derived")
	double AltitudeASL = 0.0;

	/** Velocity magnitude (m/s) */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry|Derived")
	double VelocityMagnitude = 0.0;

	/** Mach number — velocity / speed_of_sound(altitude) */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry|Derived")
	double MachNumber = 0.0;

	/** G-force — |acceleration| / 9.80665 */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry|Derived")
	double GForce = 0.0;

	/** Dynamic pressure in Pascals — 0.5 * rho(alt) * v^2 */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry|Derived")
	double DynamicPressurePa = 0.0;

	/** Vertical velocity (m/s) — velocity projected onto local up vector */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry|Derived")
	double VerticalVelocity = 0.0;

	/** True if any engine has thrust > 1% */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry|Derived")
	bool bAnyEngineActive = false;

	/** Vehicle position in UE world space (after ECEF conversion) */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry|UE")
	FVector UEPosition = FVector::ZeroVector;

	/** Vehicle rotation in UE world space (after ECEF conversion) */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry|UE")
	FQuat UERotation = FQuat::Identity;

	/** True when telemetry data has gone stale (no updates within timeout) */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry|Status")
	bool bTelemetryStale = false;

	/** How long since last valid telemetry update (seconds) */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry|Status")
	float StaleDurationSeconds = 0.0f;

	/** Vehicle ECEF position (for banner spawning) */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry|ECEF")
	FVector VehicleECEFPosition = FVector::ZeroVector;
};
