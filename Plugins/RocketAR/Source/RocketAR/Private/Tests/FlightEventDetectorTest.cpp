#include "Misc/AutomationTest.h"
#include "FlightEventDetector.h"
#include "TelemetryTypes.h"

// Helper to create test telemetry data
static FProcessedTelemetryData MakeTestData(
	double MET, double Altitude, double Velocity, double DynPressure,
	double VertVel, double Mach, double GForce,
	const TArray<float>& Thrust, const FVector& ECEFPos = FVector::ZeroVector)
{
	FProcessedTelemetryData Data;
	Data.RawData.MissionElapsedTime = MET;
	Data.RawData.EngineThrustPercent = Thrust;
	Data.RawData.bTelemetryValid = true;
	Data.AltitudeASL = Altitude;
	Data.VelocityMagnitude = Velocity;
	Data.DynamicPressurePa = DynPressure;
	Data.VerticalVelocity = VertVel;
	Data.MachNumber = Mach;
	Data.GForce = GForce;
	Data.bAnyEngineActive = false;
	Data.VehicleECEFPosition = ECEFPos;

	for (float T : Thrust)
	{
		if (T > 0.01f)
		{
			Data.bAnyEngineActive = true;
			break;
		}
	}

	return Data;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightEventIgnitionTest,
	"RocketAR.Events.Ignition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightEventIgnitionTest::RunTest(const FString& Parameters)
{
	UFlightEventDetector* Detector = NewObject<UFlightEventDetector>();

	// No thrust
	TArray<float> NoThrust = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	Detector->ProcessTelemetry(MakeTestData(-5.0, 0, 0, 0, 0, 0, 0, NoThrust));

	// Thrust on
	TArray<float> WithThrust = { 0.0f, 0.0f, 0.85f, 0.85f, 0.85f, 0.85f, 0.0f };
	Detector->ProcessTelemetry(MakeTestData(-3.0, 0, 0, 0, 0, 0, 1.0, WithThrust));

	const auto& Events = Detector->GetDetectedEvents();
	TestTrue(TEXT("Ignition detected"), Events.Num() >= 1);
	if (Events.Num() >= 1)
	{
		TestEqual(TEXT("Event is Ignition"), Events[0].EventType, EFlightEvent::Ignition);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightEventLiftoffTest,
	"RocketAR.Events.Liftoff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightEventLiftoffTest::RunTest(const FString& Parameters)
{
	UFlightEventDetector* Detector = NewObject<UFlightEventDetector>();
	TArray<float> Thrust = { 0.95f, 0.95f, 0.85f, 0.85f, 0.85f, 0.85f, 0.0f };

	// On pad
	Detector->ProcessTelemetry(MakeTestData(0.0, 0.0, 0, 101325, 0, 0, 1.0, Thrust));

	// Just above ground
	Detector->ProcessTelemetry(MakeTestData(0.5, 2.0, 5, 101300, 5, 0, 1.5, Thrust));

	bool bFoundLiftoff = false;
	for (const auto& Evt : Detector->GetDetectedEvents())
	{
		if (Evt.EventType == EFlightEvent::Liftoff) bFoundLiftoff = true;
	}
	TestTrue(TEXT("Liftoff detected"), bFoundLiftoff);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightEventResetTest,
	"RocketAR.Events.Reset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightEventResetTest::RunTest(const FString& Parameters)
{
	UFlightEventDetector* Detector = NewObject<UFlightEventDetector>();

	TArray<float> NoThrust = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	TArray<float> WithThrust = { 0.95f, 0.95f, 0.85f, 0.85f, 0.85f, 0.85f, 0.0f };

	Detector->ProcessTelemetry(MakeTestData(-5.0, 0, 0, 0, 0, 0, 0, NoThrust));
	Detector->ProcessTelemetry(MakeTestData(0.0, 0, 0, 0, 0, 0, 0, WithThrust));
	TestTrue(TEXT("Has events before reset"), Detector->GetDetectedEvents().Num() > 0);

	Detector->Reset();
	TestEqual(TEXT("No events after reset"), Detector->GetDetectedEvents().Num(), 0);

	// Should be able to detect ignition again
	Detector->ProcessTelemetry(MakeTestData(-5.0, 0, 0, 0, 0, 0, 0, NoThrust));
	Detector->ProcessTelemetry(MakeTestData(0.0, 0, 0, 0, 0, 0, 0, WithThrust));
	TestTrue(TEXT("Events fire again after reset"), Detector->GetDetectedEvents().Num() > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightEventAltitudeMarkerTest,
	"RocketAR.Events.AltitudeMarkers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightEventAltitudeMarkerTest::RunTest(const FString& Parameters)
{
	UFlightEventDetector* Detector = NewObject<UFlightEventDetector>();
	Detector->Config.AltitudeMarkerInterval = 10000.0f;
	Detector->Config.AltitudeMarkerMinSpacing = 5000.0f;

	TArray<float> Thrust = { 0.95f, 0.95f, 0.85f, 0.85f, 0.85f, 0.85f, 0.0f };

	// Ascend through altitude markers
	Detector->ProcessTelemetry(MakeTestData(0.0, 0, 100, 50000, 100, 0.3, 2.0, Thrust));
	Detector->ProcessTelemetry(MakeTestData(30.0, 9000, 300, 40000, 300, 0.9, 2.5, Thrust));
	Detector->ProcessTelemetry(MakeTestData(60.0, 11000, 400, 30000, 300, 1.2, 3.0, Thrust));
	Detector->ProcessTelemetry(MakeTestData(90.0, 21000, 500, 15000, 400, 1.5, 2.0, Thrust));

	int32 MarkerCount = 0;
	for (const auto& Evt : Detector->GetDetectedEvents())
	{
		if (Evt.EventType == EFlightEvent::AltitudeMarker)
		{
			MarkerCount++;
		}
	}
	TestTrue(TEXT("At least one altitude marker detected"), MarkerCount >= 1);

	return true;
}
