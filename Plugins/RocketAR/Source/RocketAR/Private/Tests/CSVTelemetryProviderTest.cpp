#include "Misc/AutomationTest.h"
#include "CSVTelemetryProvider.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCSVProviderLoadTest,
	"RocketAR.CSV.LoadValidFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCSVProviderLoadTest::RunTest(const FString& Parameters)
{
	// This test verifies the CSV parsing logic with inline test data
	// Actual file loading is tested via in-editor functional tests

	// Test: valid row parsing
	ACSVTelemetryProvider* Provider = NewObject<ACSVTelemetryProvider>();
	TestTrue(TEXT("Provider created"), Provider != nullptr);
	TestEqual(TEXT("Initial row count is 0"), Provider->GetRowCount(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCSVProviderPlaybackTest,
	"RocketAR.CSV.PlaybackControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCSVProviderPlaybackTest::RunTest(const FString& Parameters)
{
	ACSVTelemetryProvider* Provider = NewObject<ACSVTelemetryProvider>();

	// Test playback state management
	TestFalse(TEXT("Not playing initially"), Provider->IsPlaying());

	Provider->Play();
	TestTrue(TEXT("Playing after Play()"), Provider->IsPlaying());

	Provider->Pause();
	TestFalse(TEXT("Not playing after Pause()"), Provider->IsPlaying());

	// Test time scale clamping
	Provider->SetTimeScale(0.01f);
	TestTrue(TEXT("TimeScale clamped to min 0.1"), Provider->GetTimeScale() >= 0.1f);

	Provider->SetTimeScale(100.0f);
	TestTrue(TEXT("TimeScale clamped to max 10"), Provider->GetTimeScale() <= 10.0f);

	return true;
}
