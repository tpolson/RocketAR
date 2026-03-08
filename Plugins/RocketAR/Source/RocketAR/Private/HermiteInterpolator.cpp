#include "HermiteInterpolator.h"

UHermiteInterpolator::UHermiteInterpolator()
{
	Samples.SetNum(RingBufferSize);
}

void UHermiteInterpolator::AddSample(const FTelemetrySample& Sample)
{
	Samples[WriteIndex] = Sample;
	WriteIndex = (WriteIndex + 1) % RingBufferSize;
	SampleCount = FMath::Min(SampleCount + 1, RingBufferSize);
}

void UHermiteInterpolator::Reset()
{
	for (auto& S : Samples)
	{
		S.bValid = false;
	}
	WriteIndex = 0;
	SampleCount = 0;
}

int32 UHermiteInterpolator::GetSampleCount() const
{
	return SampleCount;
}

const FTelemetrySample& UHermiteInterpolator::GetSample(int32 IndexFromOldest) const
{
	const int32 ActualIndex = (WriteIndex - SampleCount + IndexFromOldest + RingBufferSize * 2) % RingBufferSize;
	return Samples[ActualIndex];
}

FVector UHermiteInterpolator::HermitePosition(
	const FVector& P0, const FVector& V0,
	const FVector& P1, const FVector& V1,
	double DeltaTime, double T)
{
	// Cubic Hermite: h(t) = (2t^3 - 3t^2 + 1)*P0 + (t^3 - 2t^2 + t)*M0 + (-2t^3 + 3t^2)*P1 + (t^3 - t^2)*M1
	// where M0 = V0*dt, M1 = V1*dt (tangents scaled by time interval)
	const double T2 = T * T;
	const double T3 = T2 * T;

	const double H00 = 2.0 * T3 - 3.0 * T2 + 1.0;
	const double H10 = T3 - 2.0 * T2 + T;
	const double H01 = -2.0 * T3 + 3.0 * T2;
	const double H11 = T3 - T2;

	const FVector M0 = V0 * DeltaTime;
	const FVector M1 = V1 * DeltaTime;

	return H00 * P0 + H10 * M0 + H01 * P1 + H11 * M1;
}

TArray<float> UHermiteInterpolator::LerpThrust(const TArray<float>& A, const TArray<float>& B, float Alpha)
{
	const int32 Count = FMath::Max(A.Num(), B.Num());
	TArray<float> Result;
	Result.SetNum(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		const float ValA = (i < A.Num()) ? A[i] : 0.0f;
		const float ValB = (i < B.Num()) ? B[i] : 0.0f;
		Result[i] = FMath::Lerp(ValA, ValB, Alpha);
	}

	return Result;
}

bool UHermiteInterpolator::GetInterpolated(
	double Time,
	FVector& OutPosition,
	FVector& OutVelocity,
	FQuat& OutRotation,
	FVector& OutAcceleration,
	TArray<float>& OutThrust,
	double& OutMET,
	bool& bOutIsExtrapolating,
	float& OutStaleDuration) const
{
	if (SampleCount == 0)
	{
		return false;
	}

	const FTelemetrySample& Latest = GetSample(SampleCount - 1);
	const double TimeSinceLatest = Time - Latest.Timestamp;

	// If we only have one sample or time is at/after the latest sample
	if (SampleCount == 1 || TimeSinceLatest >= 0.0)
	{
		if (TimeSinceLatest > ExtrapolationTimeout)
		{
			// Frozen — stale data
			OutPosition = Latest.Position;
			OutVelocity = FVector::ZeroVector;
			OutRotation = Latest.Rotation;
			OutAcceleration = Latest.Acceleration;
			OutThrust = Latest.EngineThrustPercent;
			OutMET = Latest.MET + ExtrapolationTimeout;
			bOutIsExtrapolating = true;
			OutStaleDuration = static_cast<float>(TimeSinceLatest);
			return true;
		}
		else if (TimeSinceLatest > 0.0)
		{
			// Extrapolate using last velocity
			OutPosition = Latest.Position + Latest.Velocity * TimeSinceLatest;
			OutVelocity = Latest.Velocity;
			OutRotation = Latest.Rotation;
			OutAcceleration = Latest.Acceleration;
			OutThrust = Latest.EngineThrustPercent;
			OutMET = Latest.MET + TimeSinceLatest;
			bOutIsExtrapolating = true;
			OutStaleDuration = static_cast<float>(TimeSinceLatest);
			return true;
		}
		else if (SampleCount == 1)
		{
			// Only one sample, return it
			OutPosition = Latest.Position;
			OutVelocity = Latest.Velocity;
			OutRotation = Latest.Rotation;
			OutAcceleration = Latest.Acceleration;
			OutThrust = Latest.EngineThrustPercent;
			OutMET = Latest.MET;
			bOutIsExtrapolating = false;
			OutStaleDuration = 0.0f;
			return true;
		}
	}

	// Find the two samples bracketing the current time
	int32 UpperIdx = -1;
	for (int32 i = 1; i < SampleCount; ++i)
	{
		if (GetSample(i).Timestamp >= Time)
		{
			UpperIdx = i;
			break;
		}
	}

	// If time is before all samples, clamp to oldest
	if (UpperIdx <= 0)
	{
		if (Time < GetSample(0).Timestamp)
		{
			const auto& Oldest = GetSample(0);
			OutPosition = Oldest.Position;
			OutVelocity = Oldest.Velocity;
			OutRotation = Oldest.Rotation;
			OutAcceleration = Oldest.Acceleration;
			OutThrust = Oldest.EngineThrustPercent;
			OutMET = Oldest.MET;
			bOutIsExtrapolating = false;
			OutStaleDuration = 0.0f;
			return true;
		}
		// Time is after latest — already handled above
		UpperIdx = SampleCount - 1;
	}

	const int32 LowerIdx = UpperIdx - 1;
	const FTelemetrySample& S0 = GetSample(LowerIdx);
	const FTelemetrySample& S1 = GetSample(UpperIdx);

	const double DeltaTime = S1.Timestamp - S0.Timestamp;
	if (DeltaTime <= 0.0)
	{
		OutPosition = S1.Position;
		OutVelocity = S1.Velocity;
		OutRotation = S1.Rotation;
		OutAcceleration = S1.Acceleration;
		OutThrust = S1.EngineThrustPercent;
		OutMET = S1.MET;
		bOutIsExtrapolating = false;
		OutStaleDuration = 0.0f;
		return true;
	}

	const double Alpha = FMath::Clamp((Time - S0.Timestamp) / DeltaTime, 0.0, 1.0);

	// Cubic Hermite for position
	OutPosition = HermitePosition(S0.Position, S0.Velocity, S1.Position, S1.Velocity, DeltaTime, Alpha);

	// Linear interpolation for velocity
	OutVelocity = FMath::Lerp(S0.Velocity, S1.Velocity, Alpha);

	// SLERP for rotation
	OutRotation = FQuat::Slerp(S0.Rotation, S1.Rotation, Alpha);

	// Linear for acceleration
	OutAcceleration = FMath::Lerp(S0.Acceleration, S1.Acceleration, static_cast<double>(Alpha));

	// Linear for thrust
	OutThrust = LerpThrust(S0.EngineThrustPercent, S1.EngineThrustPercent, static_cast<float>(Alpha));

	// Linear for MET
	OutMET = FMath::Lerp(S0.MET, S1.MET, Alpha);

	bOutIsExtrapolating = false;
	OutStaleDuration = 0.0f;
	return true;
}
