#include "Misc/AutomationTest.h"
#include "HermiteInterpolator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHermiteInterpolatorBasicTest,
	"RocketAR.Interpolation.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHermiteInterpolatorBasicTest::RunTest(const FString& Parameters)
{
	UHermiteInterpolator* Interp = NewObject<UHermiteInterpolator>();

	// Add two samples: constant velocity motion
	FTelemetrySample S0;
	S0.Position = FVector(0.0, 0.0, 0.0);
	S0.Velocity = FVector(100.0, 0.0, 0.0);
	S0.Rotation = FQuat::Identity;
	S0.MET = 0.0;
	S0.Timestamp = 0.0;
	S0.bValid = true;

	FTelemetrySample S1;
	S1.Position = FVector(100.0, 0.0, 0.0); // 100m at 100m/s after 1s
	S1.Velocity = FVector(100.0, 0.0, 0.0);
	S1.Rotation = FQuat::Identity;
	S1.MET = 1.0;
	S1.Timestamp = 1.0;
	S1.bValid = true;

	Interp->AddSample(S0);
	Interp->AddSample(S1);

	FVector Pos, Vel, Acc;
	FQuat Rot;
	TArray<float> Thrust;
	double MET;
	bool bExtrapolating;
	float StaleDuration;

	// Interpolate at midpoint
	bool bOk = Interp->GetInterpolated(0.5, Pos, Vel, Rot, Acc, Thrust, MET, bExtrapolating, StaleDuration);
	TestTrue(TEXT("Interpolation succeeded"), bOk);
	TestTrue(TEXT("Midpoint position ~50m"), FMath::IsNearlyEqual(Pos.X, 50.0, 1.0));
	TestFalse(TEXT("Not extrapolating"), bExtrapolating);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHermiteInterpolatorExtrapolationTest,
	"RocketAR.Interpolation.Extrapolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHermiteInterpolatorExtrapolationTest::RunTest(const FString& Parameters)
{
	UHermiteInterpolator* Interp = NewObject<UHermiteInterpolator>();
	Interp->ExtrapolationTimeout = 1.0f;

	FTelemetrySample S0;
	S0.Position = FVector(0.0, 0.0, 0.0);
	S0.Velocity = FVector(100.0, 0.0, 0.0);
	S0.Rotation = FQuat::Identity;
	S0.MET = 0.0;
	S0.Timestamp = 0.0;
	S0.bValid = true;

	Interp->AddSample(S0);

	FVector Pos, Vel, Acc;
	FQuat Rot;
	TArray<float> Thrust;
	double MET;
	bool bExtrapolating;
	float StaleDuration;

	// 0.5s later — should extrapolate
	Interp->GetInterpolated(0.5, Pos, Vel, Rot, Acc, Thrust, MET, bExtrapolating, StaleDuration);
	TestTrue(TEXT("Extrapolating"), bExtrapolating);
	TestTrue(TEXT("Position extrapolated ~50m"), FMath::IsNearlyEqual(Pos.X, 50.0, 1.0));

	// 2.0s later — beyond timeout, should freeze
	Interp->GetInterpolated(2.0, Pos, Vel, Rot, Acc, Thrust, MET, bExtrapolating, StaleDuration);
	TestTrue(TEXT("Still extrapolating (but frozen)"), bExtrapolating);
	TestTrue(TEXT("Position frozen at origin"), FMath::IsNearlyEqual(Pos.X, 0.0, 1.0));
	TestTrue(TEXT("Stale duration > 1s"), StaleDuration > 1.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHermiteInterpolatorSLERPTest,
	"RocketAR.Interpolation.RotationSLERP",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHermiteInterpolatorSLERPTest::RunTest(const FString& Parameters)
{
	UHermiteInterpolator* Interp = NewObject<UHermiteInterpolator>();

	FTelemetrySample S0;
	S0.Position = FVector::ZeroVector;
	S0.Velocity = FVector::ZeroVector;
	S0.Rotation = FQuat::Identity;
	S0.MET = 0.0;
	S0.Timestamp = 0.0;
	S0.bValid = true;

	FTelemetrySample S1;
	S1.Position = FVector::ZeroVector;
	S1.Velocity = FVector::ZeroVector;
	// 90 degrees around Z axis
	S1.Rotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(90.0));
	S1.MET = 1.0;
	S1.Timestamp = 1.0;
	S1.bValid = true;

	Interp->AddSample(S0);
	Interp->AddSample(S1);

	FVector Pos, Vel, Acc;
	FQuat Rot;
	TArray<float> Thrust;
	double MET;
	bool bExtrapolating;
	float StaleDuration;

	// At midpoint, rotation should be ~45 degrees
	Interp->GetInterpolated(0.5, Pos, Vel, Rot, Acc, Thrust, MET, bExtrapolating, StaleDuration);

	const FRotator Rotator = Rot.Rotator();
	TestTrue(TEXT("Mid-rotation Yaw ~45 degrees"), FMath::IsNearlyEqual(Rotator.Yaw, 45.0, 2.0));

	return true;
}
