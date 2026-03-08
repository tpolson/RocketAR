#pragma once

#include "CoreMinimal.h"
#include "TelemetryTypes.h"
#include "HermiteInterpolator.generated.h"

/**
 * A timestamped telemetry sample for interpolation.
 */
USTRUCT()
struct ROCKETAR_API FTelemetrySample
{
	GENERATED_BODY()

	FVector Position = FVector::ZeroVector;       // ECEF meters
	FVector Velocity = FVector::ZeroVector;       // ECEF m/s
	FQuat Rotation = FQuat::Identity;             // ECEF frame
	FVector Acceleration = FVector::ZeroVector;   // body frame m/s^2
	TArray<float> EngineThrustPercent;
	double MET = 0.0;                             // mission elapsed time
	double Timestamp = 0.0;                       // wall-clock time when received
	bool bValid = false;
};

/**
 * Cubic Hermite interpolation for telemetry data.
 * Ring buffer of samples, interpolates position using velocity as tangents,
 * SLERP for rotation, linear for scalars.
 * Extrapolates up to configurable timeout, then freezes.
 */
UCLASS(BlueprintType)
class ROCKETAR_API UHermiteInterpolator : public UObject
{
	GENERATED_BODY()

public:
	UHermiteInterpolator();

	/** Add a new telemetry sample */
	void AddSample(const FTelemetrySample& Sample);

	/**
	 * Get interpolated telemetry at the given time.
	 * @param Time Current wall-clock time
	 * @param OutPosition Interpolated ECEF position
	 * @param OutVelocity Interpolated velocity
	 * @param OutRotation Interpolated rotation
	 * @param OutAcceleration Interpolated acceleration
	 * @param OutThrust Interpolated thrust array
	 * @param OutMET Interpolated mission elapsed time
	 * @param bOutIsExtrapolating True if we're beyond the last sample
	 * @param OutStaleDuration Seconds since last sample (0 if interpolating)
	 * @return True if valid data was produced
	 */
	bool GetInterpolated(
		double Time,
		FVector& OutPosition,
		FVector& OutVelocity,
		FQuat& OutRotation,
		FVector& OutAcceleration,
		TArray<float>& OutThrust,
		double& OutMET,
		bool& bOutIsExtrapolating,
		float& OutStaleDuration) const;

	/** Reset all samples */
	void Reset();

	/** Number of valid samples in the buffer */
	int32 GetSampleCount() const;

	/** Maximum extrapolation time before freezing (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation")
	float ExtrapolationTimeout = 1.0f;

private:
	static constexpr int32 RingBufferSize = 16;
	TArray<FTelemetrySample> Samples;
	int32 WriteIndex = 0;
	int32 SampleCount = 0;

	const FTelemetrySample& GetSample(int32 IndexFromOldest) const;

	/** Cubic Hermite interpolation for position using velocity as tangents */
	static FVector HermitePosition(
		const FVector& P0, const FVector& V0,
		const FVector& P1, const FVector& V1,
		double DeltaTime, double T);

	/** Linear interpolation for thrust arrays */
	static TArray<float> LerpThrust(const TArray<float>& A, const TArray<float>& B, float Alpha);
};
