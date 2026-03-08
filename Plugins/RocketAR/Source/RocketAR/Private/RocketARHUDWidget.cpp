#include "RocketARHUDWidget.h"
#include "TelemetrySubsystem.h"
#include "Engine/World.h"

void URocketARHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UWorld* World = GetWorld())
	{
		TelemetrySubsystem = World->GetSubsystem<UTelemetrySubsystem>();
	}
}

void URocketARHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (TelemetrySubsystem && TelemetrySubsystem->IsTelemetryActive())
	{
		UpdateFromTelemetry(TelemetrySubsystem->GetProcessedData());
	}
	else
	{
		METDisplay = TEXT("T+00:00:00");
		AltitudeDisplay = TEXT("--- m");
		VelocityDisplay = TEXT("--- m/s");
		MachDisplay = TEXT("---");
		GForceDisplay = TEXT("--- G");
		FlightPhaseDisplay = TEXT("STANDBY");
		TelemetryStatusDisplay = TEXT("NO DATA");
		DynamicPressureDisplay = TEXT("--- Pa");
		StatusColor = FLinearColor(0.5f, 0.5f, 0.5f);
	}
}

void URocketARHUDWidget::UpdateFromTelemetry(const FProcessedTelemetryData& Data)
{
	METDisplay = FormatMET(Data.RawData.MissionElapsedTime);

	if (Data.AltitudeASL >= 1000.0)
	{
		AltitudeDisplay = FString::Printf(TEXT("%.1f km"), Data.AltitudeASL / 1000.0);
	}
	else
	{
		AltitudeDisplay = FString::Printf(TEXT("%.0f m"), Data.AltitudeASL);
	}

	VelocityDisplay = FString::Printf(TEXT("%.0f m/s"), Data.VelocityMagnitude);
	MachDisplay = FString::Printf(TEXT("M %.2f"), Data.MachNumber);
	GForceDisplay = FString::Printf(TEXT("%.1f G"), Data.GForce);
	DynamicPressureDisplay = FString::Printf(TEXT("%.0f Pa"), Data.DynamicPressurePa);

	FlightPhaseDisplay = DetermineFlightPhase(Data);
	TelemetryStatusDisplay = DetermineTelemetryStatus(Data);

	// Status color
	if (Data.bTelemetryStale)
	{
		StatusColor = (Data.StaleDurationSeconds > 3.0f) ? FLinearColor::Red : FLinearColor::Yellow;
	}
	else
	{
		StatusColor = FLinearColor::Green;
	}
}

FString URocketARHUDWidget::FormatMET(double MET) const
{
	const bool bNegative = MET < 0.0;
	const double AbsMET = FMath::Abs(MET);

	const int32 Hours = static_cast<int32>(AbsMET / 3600.0);
	const int32 Minutes = static_cast<int32>(FMath::Fmod(AbsMET, 3600.0) / 60.0);
	const int32 Seconds = static_cast<int32>(FMath::Fmod(AbsMET, 60.0));

	return FString::Printf(TEXT("T%s%02d:%02d:%02d"),
		bNegative ? TEXT("-") : TEXT("+"), Hours, Minutes, Seconds);
}

FString URocketARHUDWidget::DetermineFlightPhase(const FProcessedTelemetryData& Data) const
{
	const double MET = Data.RawData.MissionElapsedTime;

	if (MET < 0.0) return TEXT("COUNTDOWN");
	if (Data.AltitudeASL < 1.0 && Data.bAnyEngineActive) return TEXT("IGNITION");
	if (Data.AltitudeASL < 100.0 && Data.bAnyEngineActive) return TEXT("LIFTOFF");
	if (Data.VelocityMagnitude < 343.0 && Data.bAnyEngineActive) return TEXT("ASCENT");
	if (Data.DynamicPressurePa > 10000.0) return TEXT("MAX-Q REGION");
	if (Data.bAnyEngineActive && Data.VerticalVelocity > 0) return TEXT("POWERED ASCENT");
	if (!Data.bAnyEngineActive && Data.VerticalVelocity > 0) return TEXT("COAST");
	if (Data.VerticalVelocity <= 0 && Data.AltitudeASL > 80000) return TEXT("APOGEE/DESCENT");
	if (Data.VerticalVelocity < 0) return TEXT("DESCENT");

	return TEXT("FLIGHT");
}

FString URocketARHUDWidget::DetermineTelemetryStatus(const FProcessedTelemetryData& Data) const
{
	if (Data.bTelemetryStale)
	{
		if (Data.StaleDurationSeconds > 5.0f) return TEXT("STALE");
		if (Data.StaleDurationSeconds > 3.0f) return TEXT("SIGNAL LOSS");
		if (Data.StaleDurationSeconds > 1.0f) return TEXT("PREDICTED");
	}

	// Determine source
	// TODO: expose source type from subsystem
	return TEXT("LIVE");
}
