#include "TelemetrySubsystem.h"
#include "TelemetryProvider.h"
#include "HermiteInterpolator.h"
#include "AtmosphereModel.h"
#include "RocketARModule.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

#if WITH_CESIUM
#include "CesiumGeoreference.h"
#endif

DECLARE_STATS_GROUP(TEXT("RocketAR"), STATGROUP_RocketAR, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Telemetry Subsystem Tick"), STAT_TelemetryTick, STATGROUP_RocketAR);
DECLARE_CYCLE_STAT(TEXT("Interpolation"), STAT_Interpolation, STATGROUP_RocketAR);
DECLARE_CYCLE_STAT(TEXT("Derived Values"), STAT_DerivedValues, STATGROUP_RocketAR);

// WGS84 ellipsoid constants
static constexpr double WGS84_A = 6378137.0;          // semi-major axis (m)
static constexpr double WGS84_B = 6356752.314245;     // semi-minor axis (m)
static constexpr double WGS84_E2 = 0.00669437999014;  // first eccentricity squared

UTelemetrySubsystem::UTelemetrySubsystem()
{
}

void UTelemetrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Interpolator = NewObject<UHermiteInterpolator>(this);
	Interpolator->ExtrapolationTimeout = ExtrapolationTimeout;

	UE_LOG(LogRocketAR, Log, TEXT("TelemetrySubsystem initialized"));
}

void UTelemetrySubsystem::Deinitialize()
{
	ActiveProvider = nullptr;
	Interpolator = nullptr;
	Georeference = nullptr;

	Super::Deinitialize();
}

bool UTelemetrySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

TStatId UTelemetrySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTelemetrySubsystem, STATGROUP_RocketAR);
}

void UTelemetrySubsystem::SetGeoreference(ACesiumGeoreference* InGeoreference)
{
	Georeference = InGeoreference;
	UE_LOG(LogRocketAR, Log, TEXT("Georeference set: %s"),
		Georeference ? *Georeference->GetName() : TEXT("null"));
}

void UTelemetrySubsystem::DiscoverProviders()
{
	UWorld* World = GetWorld();
	if (!World) return;

	TScriptInterface<ITelemetryProvider> BestProvider;
	int32 BestPriority = -1;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetClass()->ImplementsInterface(UTelemetryProvider::StaticClass()))
		{
			const int32 Priority = ITelemetryProvider::Execute_GetProviderPriority(Actor);
			if (Priority > BestPriority)
			{
				BestPriority = Priority;
				BestProvider.SetObject(Actor);
				BestProvider.SetInterface(Cast<ITelemetryProvider>(Actor));
			}
		}
	}

	if (BestProvider.GetObject())
	{
		ActiveProvider = BestProvider;
		UE_LOG(LogRocketAR, Log, TEXT("Telemetry provider discovered: %s (priority %d)"),
			*BestProvider.GetObject()->GetName(), BestPriority);
	}
	else
	{
		UE_LOG(LogRocketAR, Warning, TEXT("No telemetry provider found in world"));
	}

	bProvidersDiscovered = true;
}

void UTelemetrySubsystem::PollProvider()
{
	if (!ActiveProvider.GetObject())
	{
		return;
	}

	AActor* ProviderActor = Cast<AActor>(ActiveProvider.GetObject());
	if (!ProviderActor || !IsValid(ProviderActor))
	{
		ActiveProvider = nullptr;
		bProvidersDiscovered = false;
		return;
	}

	if (!ITelemetryProvider::Execute_IsTelemetryAvailable(ProviderActor))
	{
		return;
	}

	const FTelemetryInputData RawData = ITelemetryProvider::Execute_GetTelemetryData(ProviderActor);

	if (!RawData.bTelemetryValid)
	{
		return;
	}

	// --- Enhanced Validation (Sprint 7) ---

	// NaN/Inf checks
	if (!FMath::IsFinite(RawData.VehiclePosition.X) || !FMath::IsFinite(RawData.VehiclePosition.Y) ||
		!FMath::IsFinite(RawData.VehiclePosition.Z))
	{
		UE_LOG(LogRocketAR, Warning, TEXT("Telemetry rejected: NaN/Inf in position"));
		return;
	}
	if (!FMath::IsFinite(RawData.VehicleVelocity.X) || !FMath::IsFinite(RawData.VehicleVelocity.Y) ||
		!FMath::IsFinite(RawData.VehicleVelocity.Z))
	{
		UE_LOG(LogRocketAR, Warning, TEXT("Telemetry rejected: NaN/Inf in velocity"));
		return;
	}

	// Quaternion normalization check (reject if magnitude outside 0.9–1.1)
	{
		const double QuatMag = RawData.VehicleRotation.Size();
		if (QuatMag < 0.9 || QuatMag > 1.1)
		{
			UE_LOG(LogRocketAR, Warning, TEXT("Telemetry rejected: quaternion magnitude %.3f"), QuatMag);
			return;
		}
	}

	// Position sanity: must be within Earth radius + 1000km
	{
		const double DistFromCenter = RawData.VehiclePosition.Size();
		const double MaxDist = 6378137.0 + 1000000.0; // Earth radius + 1000km
		if (DistFromCenter > MaxDist || DistFromCenter < 1000.0) // too far or too close to center
		{
			UE_LOG(LogRocketAR, Warning, TEXT("Telemetry rejected: position %.0fm from Earth center"), DistFromCenter);
			return;
		}
	}

	// Jump detection: >500m between consecutive samples at 5Hz = >100km/s (physically impossible)
	if (Interpolator->GetSampleCount() > 0)
	{
		// Check against the last sample position
		FVector LastPos, LastVel, LastAcc;
		FQuat LastRot;
		TArray<float> LastThrust;
		double LastMET;
		bool bWasExtrapolating;
		float LastStale;

		if (Interpolator->GetInterpolated(WorldTimeAccumulator, LastPos, LastVel, LastRot,
			LastAcc, LastThrust, LastMET, bWasExtrapolating, LastStale))
		{
			const double JumpDist = FVector::Dist(RawData.VehiclePosition, LastPos);
			if (JumpDist > 500.0 && !bWasExtrapolating)
			{
				UE_LOG(LogRocketAR, Warning, TEXT("Telemetry rejected: position jump of %.0fm"), JumpDist);
				return;
			}
		}
	}

	// --- End Validation ---

	// Create a sample for the interpolator
	FTelemetrySample Sample;
	Sample.Position = RawData.VehiclePosition;
	Sample.Velocity = RawData.VehicleVelocity;
	Sample.Rotation = RawData.VehicleRotation;
	Sample.Acceleration = RawData.VehicleAcceleration;
	Sample.EngineThrustPercent = RawData.EngineThrustPercent;
	Sample.MET = RawData.MissionElapsedTime;
	Sample.Timestamp = WorldTimeAccumulator;
	Sample.bValid = true;

	Interpolator->AddSample(Sample);
	bTelemetryActive = true;
}

double UTelemetrySubsystem::ComputeAltitudeFromECEF(const FVector& ECEFPosition) const
{
	// Bowring's iterative method for ECEF to geodetic altitude
	const double X = ECEFPosition.X;
	const double Y = ECEFPosition.Y;
	const double Z = ECEFPosition.Z;

	const double P = FMath::Sqrt(X * X + Y * Y);
	const double E2 = WGS84_E2;
	const double A = WGS84_A;
	const double B = WGS84_B;
	const double EP2 = (A * A - B * B) / (B * B);

	// Initial estimate
	double Theta = FMath::Atan2(Z * A, P * B);
	double Phi = 0.0;

	// Iterate (3 iterations is sufficient for convergence)
	for (int32 i = 0; i < 3; ++i)
	{
		const double SinTheta = FMath::Sin(Theta);
		const double CosTheta = FMath::Cos(Theta);
		Phi = FMath::Atan2(
			Z + EP2 * B * SinTheta * SinTheta * SinTheta,
			P - E2 * A * CosTheta * CosTheta * CosTheta);
		Theta = Phi;
	}

	const double SinPhi = FMath::Sin(Phi);
	const double N = A / FMath::Sqrt(1.0 - E2 * SinPhi * SinPhi);
	const double CosPhi = FMath::Cos(Phi);

	if (FMath::Abs(CosPhi) > 1e-10)
	{
		return P / CosPhi - N;
	}
	else
	{
		return FMath::Abs(Z) - B;
	}
}

FVector UTelemetrySubsystem::ComputeLocalUpVector(const FVector& ECEFPosition) const
{
	// Local up in ECEF is approximately the normalized position vector
	// (exact for a sphere, close enough for WGS84 at the precision we need)
	return ECEFPosition.GetSafeNormal();
}

FVector UTelemetrySubsystem::ECEFToUE(const FVector& ECEFPosition) const
{
#if WITH_CESIUM
	if (Georeference)
	{
		// Use Cesium's double-precision ECEF→UE conversion
		const FVector UEPos = Georeference->TransformEarthCenteredEarthFixedPositionToUnreal(
			FVector(ECEFPosition.X, ECEFPosition.Y, ECEFPosition.Z));
		return UEPos;
	}
#endif

	// Fallback: manual ECEF→ENU→UE (less precise without Cesium origin management)
	// This requires a reference origin. If no georeference, use a basic conversion.
	UE_LOG(LogRocketAR, Warning, TEXT("No Cesium georeference — ECEF conversion is approximate"));

	// Convert directly (placeholder — not production quality without Cesium)
	return ECEFPosition * 100.0; // meters to cm
}

FQuat UTelemetrySubsystem::ECEFRotationToUE(const FQuat& ECEFRotation) const
{
#if WITH_CESIUM
	if (Georeference)
	{
		// Convert ECEF rotation to UE rotation via Cesium's transform
		// Q_ue = Q_ecef_to_ue * Q_vehicle_ecef
		// The matrix includes scale (meters→cm), so we must extract rotation only
		FMatrix RotOnly = Georeference->ComputeEarthCenteredEarthFixedToUnrealTransformation();
		RotOnly.RemoveScaling();
		const FQuat ECEFToUERotation = RotOnly.ToQuat();
		return ECEFToUERotation * ECEFRotation;
	}
#endif

	return ECEFRotation;
}

void UTelemetrySubsystem::ComputeDerivedValues(const FTelemetryInputData& RawData)
{
	SCOPE_CYCLE_COUNTER(STAT_DerivedValues);

	ProcessedData.RawData = RawData;
	ProcessedData.VehicleECEFPosition = RawData.VehiclePosition;

	// Altitude from ECEF
	ProcessedData.AltitudeASL = ComputeAltitudeFromECEF(RawData.VehiclePosition);

	// Velocity magnitude
	ProcessedData.VelocityMagnitude = RawData.VehicleVelocity.Size();

	// Mach number
	ProcessedData.MachNumber = UAtmosphereModel::GetMachNumber(
		ProcessedData.AltitudeASL, ProcessedData.VelocityMagnitude);

	// G-force
	ProcessedData.GForce = RawData.VehicleAcceleration.Size() / 9.80665;

	// Dynamic pressure
	ProcessedData.DynamicPressurePa = UAtmosphereModel::GetDynamicPressure(
		ProcessedData.AltitudeASL, ProcessedData.VelocityMagnitude);

	// Vertical velocity — project velocity onto local up
	const FVector LocalUp = ComputeLocalUpVector(RawData.VehiclePosition);
	ProcessedData.VerticalVelocity = FVector::DotProduct(RawData.VehicleVelocity, LocalUp);

	// Engine active state
	ProcessedData.bAnyEngineActive = false;
	for (float Thrust : RawData.EngineThrustPercent)
	{
		if (Thrust > 0.01f)
		{
			ProcessedData.bAnyEngineActive = true;
			break;
		}
	}

	// UE space transforms
	ProcessedData.UEPosition = ECEFToUE(RawData.VehiclePosition);
	ProcessedData.UERotation = ECEFRotationToUE(RawData.VehicleRotation);
}

void UTelemetrySubsystem::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_TelemetryTick);

	WorldTimeAccumulator += DeltaTime;

	// Discover providers on first tick or if we lost our provider
	if (!bProvidersDiscovered || !ActiveProvider.GetObject())
	{
		DiscoverProviders();
	}

	// Poll the active provider for new data
	PollProvider();

	if (!Interpolator || Interpolator->GetSampleCount() == 0)
	{
		return;
	}

	// Interpolate
	{
		SCOPE_CYCLE_COUNTER(STAT_Interpolation);

		FVector InterpPos, InterpVel, InterpAccel;
		FQuat InterpRot;
		TArray<float> InterpThrust;
		double InterpMET;
		bool bExtrapolating;
		float StaleDuration;

		if (Interpolator->GetInterpolated(
			WorldTimeAccumulator, InterpPos, InterpVel, InterpRot, InterpAccel,
			InterpThrust, InterpMET, bExtrapolating, StaleDuration))
		{
			// Build raw data from interpolated values
			FTelemetryInputData InterpolatedRaw;
			InterpolatedRaw.VehiclePosition = InterpPos;
			InterpolatedRaw.VehicleVelocity = InterpVel;
			InterpolatedRaw.VehicleRotation = InterpRot;
			InterpolatedRaw.VehicleAcceleration = InterpAccel;
			InterpolatedRaw.EngineThrustPercent = InterpThrust;
			InterpolatedRaw.MissionElapsedTime = InterpMET;
			InterpolatedRaw.bTelemetryValid = true;

			// Compute derived values
			ComputeDerivedValues(InterpolatedRaw);
			ProcessedData.bTelemetryStale = (StaleDuration > ExtrapolationTimeout);
			ProcessedData.StaleDurationSeconds = StaleDuration;

			// Broadcast
			OnTelemetryUpdated.Broadcast(ProcessedData);
		}
	}
}
