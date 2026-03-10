#include "FlightEventDetector.h"
#include "RocketARModule.h"

DECLARE_STATS_GROUP(TEXT("RocketAR"), STATGROUP_RocketAR, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Event Detection"), STAT_EventDetection, STATGROUP_RocketAR);

UFlightEventDetector::UFlightEventDetector()
{
}

void UFlightEventDetector::Reset()
{
	LatchedEvents.Empty();
	DetectedEvents.Empty();
	bPrevAnyEngineActive = false;
	bPrevSRBsActive = false;
	bPrevCoreActive = false;
	bPrevSecondStageActive = false;
	PrevAltitude = 0.0;
	PrevVerticalVelocity = 0.0;
	PrevMachNumber = 0.0;
	PrevDynamicPressure = 0.0;
	MaxQPeakValue = 0.0;
	MaxQRisingStartMET = -1.0;
	bMaxQRising = false;
	MaxQLastHigherTime = 0.0;
	bMaxQConfirmed = false;
	LastAltitudeMarker = 0.0;
	HighestAltitudeMarker = 0.0;
	bFirstFrame = true;

	UE_LOG(LogRocketAR, Log, TEXT("FlightEventDetector: Reset"));
}

void UFlightEventDetector::ProcessTelemetry(const FProcessedTelemetryData& Data)
{
	SCOPE_CYCLE_COUNTER(STAT_EventDetection);

	if (bFirstFrame)
	{
		// Initialize previous state
		bPrevAnyEngineActive = Data.bAnyEngineActive;
		bPrevSRBsActive = AreSRBsActive(Data.RawData.EngineThrustPercent);
		bPrevCoreActive = IsCoreActive(Data.RawData.EngineThrustPercent);
		bPrevSecondStageActive = IsSecondStageActive(Data.RawData.EngineThrustPercent);
		PrevAltitude = Data.AltitudeASL;
		PrevVerticalVelocity = Data.VerticalVelocity;
		PrevMachNumber = Data.MachNumber;
		PrevDynamicPressure = Data.DynamicPressurePa;
		bFirstFrame = false;
		return;
	}

	// Check all events
	CheckIgnition(Data);
	CheckLiftoff(Data);
	CheckMach1(Data);
	CheckMaxQ(Data);
	CheckSRBEvents(Data);
	CheckMECO(Data);
	CheckStageSeparation(Data);
	CheckSecondStageIgnition(Data);
	CheckFairingJettison(Data);
	CheckSecondStageCutoff(Data);
	CheckApogee(Data);
	CheckReentry(Data);
	CheckChuteDeployment(Data);
	CheckSplashdown(Data);
	CheckAltitudeMarkers(Data);

	// Update previous state
	bPrevAnyEngineActive = Data.bAnyEngineActive;
	bPrevSRBsActive = AreSRBsActive(Data.RawData.EngineThrustPercent);
	bPrevCoreActive = IsCoreActive(Data.RawData.EngineThrustPercent);
	bPrevSecondStageActive = IsSecondStageActive(Data.RawData.EngineThrustPercent);
	PrevAltitude = Data.AltitudeASL;
	PrevVerticalVelocity = Data.VerticalVelocity;
	PrevMachNumber = Data.MachNumber;
	PrevDynamicPressure = Data.DynamicPressurePa;
}

void UFlightEventDetector::FireEvent(EFlightEvent EventType, const FProcessedTelemetryData& Data, const FString& Label)
{
	if (IsEventLatched(EventType) && EventType != EFlightEvent::AltitudeMarker)
	{
		return;
	}

	FFlightEventData EventData;
	EventData.EventType = EventType;
	EventData.MET = Data.RawData.MissionElapsedTime;
	EventData.Altitude = Data.AltitudeASL;
	EventData.Velocity = Data.VelocityMagnitude;
	EventData.EventLabel = Label;
	EventData.ECEFPosition = Data.VehicleECEFPosition;

	DetectedEvents.Add(EventData);
	LatchEvent(EventType);

	UE_LOG(LogRocketAR, Log, TEXT("FLIGHT EVENT: %s at MET=%.1fs, Alt=%.0fm, Vel=%.0fm/s"),
		*Label, EventData.MET, EventData.Altitude, EventData.Velocity);

	OnFlightEvent.Broadcast(EventData);
}

bool UFlightEventDetector::IsEventLatched(EFlightEvent EventType) const
{
	return LatchedEvents.Contains(EventType);
}

void UFlightEventDetector::LatchEvent(EFlightEvent EventType)
{
	LatchedEvents.Add(EventType);
}

// Engine group helpers
bool UFlightEventDetector::AreSRBsActive(const TArray<float>& Thrust) const
{
	for (int32 i = 0; i < Config.SRBEngineCount && i < Thrust.Num(); ++i)
	{
		if (Thrust[i] > Config.ThrustOnThreshold)
		{
			return true;
		}
	}
	return false;
}

bool UFlightEventDetector::IsCoreActive(const TArray<float>& Thrust) const
{
	const int32 Start = Config.SRBEngineCount;
	const int32 End = Start + Config.CoreEngineCount;
	for (int32 i = Start; i < End && i < Thrust.Num(); ++i)
	{
		if (Thrust[i] > Config.ThrustOnThreshold)
		{
			return true;
		}
	}
	return false;
}

bool UFlightEventDetector::IsSecondStageActive(const TArray<float>& Thrust) const
{
	const int32 Start = Config.SRBEngineCount + Config.CoreEngineCount;
	for (int32 i = Start; i < Thrust.Num(); ++i)
	{
		if (Thrust[i] > Config.ThrustOnThreshold)
		{
			return true;
		}
	}
	return false;
}

// Individual event checks

void UFlightEventDetector::CheckIgnition(const FProcessedTelemetryData& Data)
{
	if (IsEventLatched(EFlightEvent::Ignition)) return;

	// Rising edge: any engine becomes active
	if (Data.bAnyEngineActive && !bPrevAnyEngineActive)
	{
		FireEvent(EFlightEvent::Ignition, Data, TEXT("IGNITION"));
	}
}

void UFlightEventDetector::CheckLiftoff(const FProcessedTelemetryData& Data)
{
	if (IsEventLatched(EFlightEvent::Liftoff)) return;

	// Altitude crosses threshold (rising edge)
	if (Data.AltitudeASL > Config.LiftoffAltitudeThreshold && PrevAltitude <= Config.LiftoffAltitudeThreshold)
	{
		FireEvent(EFlightEvent::Liftoff, Data, TEXT("LIFTOFF"));
	}
}

void UFlightEventDetector::CheckMach1(const FProcessedTelemetryData& Data)
{
	if (IsEventLatched(EFlightEvent::Mach1)) return;

	if (Data.MachNumber >= 1.0 && PrevMachNumber < 1.0)
	{
		FireEvent(EFlightEvent::Mach1, Data, FString::Printf(TEXT("MACH 1 | %.0f m/s"), Data.VelocityMagnitude));
	}
}

void UFlightEventDetector::CheckMaxQ(const FProcessedTelemetryData& Data)
{
	if (IsEventLatched(EFlightEvent::MaxQ)) return;

	const double Q = Data.DynamicPressurePa;
	const double MET = Data.RawData.MissionElapsedTime;

	// Track if Q is still rising
	if (Q > PrevDynamicPressure)
	{
		if (!bMaxQRising)
		{
			bMaxQRising = true;
			MaxQRisingStartMET = MET;
		}
		MaxQPeakValue = FMath::Max(MaxQPeakValue, Q);
		MaxQLastHigherTime = MET;
	}
	else if (bMaxQRising)
	{
		// Q is dropping — check if we've been rising long enough
		const double RisingDuration = MET - MaxQRisingStartMET;

		if (RisingDuration >= Config.MaxQRisingDuration)
		{
			// Check if current Q is below peak by threshold percentage
			const double DropFraction = (MaxQPeakValue - Q) / MaxQPeakValue;
			const double TimeSinceHigher = MET - MaxQLastHigherTime;

			if (DropFraction >= Config.MaxQDropPercent && TimeSinceHigher >= Config.MaxQConfirmationWindow)
			{
				FireEvent(EFlightEvent::MaxQ, Data,
					FString::Printf(TEXT("MAX Q | %.0f Pa"), MaxQPeakValue));
			}
		}
	}
}

void UFlightEventDetector::CheckSRBEvents(const FProcessedTelemetryData& Data)
{
	const bool bSRBsNow = AreSRBsActive(Data.RawData.EngineThrustPercent);

	// SRB Ignition
	if (!IsEventLatched(EFlightEvent::SRBIgnition) && bSRBsNow && !bPrevSRBsActive)
	{
		FireEvent(EFlightEvent::SRBIgnition, Data, TEXT("SRB IGNITION"));
	}

	// SRB Separation
	if (!IsEventLatched(EFlightEvent::SRBSeparation) && !bSRBsNow && bPrevSRBsActive)
	{
		// Only fire if SRBs were previously active (not at startup)
		if (IsEventLatched(EFlightEvent::SRBIgnition))
		{
			FireEvent(EFlightEvent::SRBSeparation, Data,
				FString::Printf(TEXT("SRB SEP | %.0f km"), Data.AltitudeASL / 1000.0));
		}
	}
}

void UFlightEventDetector::CheckMECO(const FProcessedTelemetryData& Data)
{
	if (IsEventLatched(EFlightEvent::MECO)) return;

	const bool bCoreNow = IsCoreActive(Data.RawData.EngineThrustPercent);

	// Core engines shut down (falling edge)
	if (!bCoreNow && bPrevCoreActive && IsEventLatched(EFlightEvent::Ignition))
	{
		FireEvent(EFlightEvent::MECO, Data, TEXT("MECO"));
	}
}

void UFlightEventDetector::CheckStageSeparation(const FProcessedTelemetryData& Data)
{
	if (IsEventLatched(EFlightEvent::StageSeparation)) return;

	// Stage separation detected shortly after MECO
	// We detect it as: MECO latched AND core is off AND some time has passed
	if (IsEventLatched(EFlightEvent::MECO) && !IsCoreActive(Data.RawData.EngineThrustPercent))
	{
		// Check if enough time since MECO (typically ~6 seconds)
		const FFlightEventData* MECOEvent = nullptr;
		for (const auto& Evt : DetectedEvents)
		{
			if (Evt.EventType == EFlightEvent::MECO)
			{
				MECOEvent = &Evt;
				break;
			}
		}

		if (MECOEvent && (Data.RawData.MissionElapsedTime - MECOEvent->MET) >= 3.0)
		{
			FireEvent(EFlightEvent::StageSeparation, Data, TEXT("STAGE SEP"));
		}
	}
}

void UFlightEventDetector::CheckSecondStageIgnition(const FProcessedTelemetryData& Data)
{
	if (IsEventLatched(EFlightEvent::SecondStageIgnition)) return;

	const bool bS2Now = IsSecondStageActive(Data.RawData.EngineThrustPercent);

	if (bS2Now && !bPrevSecondStageActive && IsEventLatched(EFlightEvent::MECO))
	{
		FireEvent(EFlightEvent::SecondStageIgnition, Data, TEXT("2ND STAGE IGN"));
	}
}

void UFlightEventDetector::CheckFairingJettison(const FProcessedTelemetryData& Data)
{
	if (IsEventLatched(EFlightEvent::FairingJettison)) return;

	// Fairing jettison when dynamic pressure drops below a threshold (typically ~1 Pa)
	// and altitude is above ~100km. This is approximate.
	if (Data.AltitudeASL > 100000.0 && Data.DynamicPressurePa < 1.0 &&
		IsEventLatched(EFlightEvent::Liftoff))
	{
		FireEvent(EFlightEvent::FairingJettison, Data,
			FString::Printf(TEXT("FAIRING SEP | %.0f km"), Data.AltitudeASL / 1000.0));
	}
}

void UFlightEventDetector::CheckSecondStageCutoff(const FProcessedTelemetryData& Data)
{
	if (IsEventLatched(EFlightEvent::SecondStageCutoff)) return;

	const bool bS2Now = IsSecondStageActive(Data.RawData.EngineThrustPercent);

	if (!bS2Now && bPrevSecondStageActive && IsEventLatched(EFlightEvent::SecondStageIgnition))
	{
		FireEvent(EFlightEvent::SecondStageCutoff, Data, TEXT("SECO"));
	}
}

void UFlightEventDetector::CheckApogee(const FProcessedTelemetryData& Data)
{
	if (IsEventLatched(EFlightEvent::Apogee)) return;

	// Vertical velocity goes from positive to negative (or zero)
	// Only after MECO to avoid false triggers during ascent oscillations
	if (IsEventLatched(EFlightEvent::MECO) &&
		Data.VerticalVelocity <= 0.0 && PrevVerticalVelocity > 0.0)
	{
		FireEvent(EFlightEvent::Apogee, Data,
			FString::Printf(TEXT("APOGEE | %.1f km"), Data.AltitudeASL / 1000.0));
	}
}

void UFlightEventDetector::CheckReentry(const FProcessedTelemetryData& Data)
{
	if (IsEventLatched(EFlightEvent::ReentryStart)) return;

	// Reentry: dynamic pressure increases above threshold while descending
	if (IsEventLatched(EFlightEvent::Apogee) &&
		Data.DynamicPressurePa > Config.ReentryQThreshold &&
		PrevDynamicPressure <= Config.ReentryQThreshold)
	{
		FireEvent(EFlightEvent::ReentryStart, Data, TEXT("REENTRY"));
	}
}

void UFlightEventDetector::CheckChuteDeployment(const FProcessedTelemetryData& Data)
{
	if (IsEventLatched(EFlightEvent::ChuteDeployment)) return;

	// Altitude drops below threshold while descending
	if (IsEventLatched(EFlightEvent::Apogee) &&
		Data.AltitudeASL <= Config.ChuteDeployAltitude &&
		PrevAltitude > Config.ChuteDeployAltitude &&
		Data.VerticalVelocity < 0.0)
	{
		FireEvent(EFlightEvent::ChuteDeployment, Data, TEXT("CHUTE DEPLOY"));
	}
}

void UFlightEventDetector::CheckSplashdown(const FProcessedTelemetryData& Data)
{
	if (IsEventLatched(EFlightEvent::Splashdown)) return;

	if (IsEventLatched(EFlightEvent::Apogee) &&
		Data.AltitudeASL <= Config.SplashdownAltitude &&
		PrevAltitude > Config.SplashdownAltitude)
	{
		FireEvent(EFlightEvent::Splashdown, Data, TEXT("SPLASHDOWN"));
	}
}

void UFlightEventDetector::CheckAltitudeMarkers(const FProcessedTelemetryData& Data)
{
	// Only fire markers during ascent
	if (Data.VerticalVelocity <= 0.0) return;
	if (Data.AltitudeASL <= 0.0) return;

	const float Interval = Config.AltitudeMarkerInterval;
	const float MinSpacing = Config.AltitudeMarkerMinSpacing;

	// Use predicted altitude (current + velocity * look-ahead) to fire markers early
	const double PredictedAlt = Data.AltitudeASL + Data.VerticalVelocity * Config.AltitudeMarkerAnticipation;
	const double MarkerAlt = FMath::FloorToDouble(PredictedAlt / Interval) * Interval;

	if (MarkerAlt > HighestAltitudeMarker && MarkerAlt > 0.0 &&
		(MarkerAlt - LastAltitudeMarker) >= MinSpacing)
	{
		HighestAltitudeMarker = MarkerAlt;
		LastAltitudeMarker = MarkerAlt;

		FFlightEventData EventData;
		EventData.EventType = EFlightEvent::AltitudeMarker;
		EventData.MET = Data.RawData.MissionElapsedTime;
		EventData.Altitude = Data.AltitudeASL;
		EventData.Velocity = Data.VelocityMagnitude;
		EventData.ECEFPosition = Data.VehicleECEFPosition;

		if (MarkerAlt >= 1000.0)
		{
			EventData.EventLabel = FString::Printf(TEXT("%.0f km"), MarkerAlt / 1000.0);
		}
		else
		{
			EventData.EventLabel = FString::Printf(TEXT("%.0f m"), MarkerAlt);
		}

		DetectedEvents.Add(EventData);

		UE_LOG(LogRocketAR, Log, TEXT("ALTITUDE MARKER: %s at MET=%.1fs"),
			*EventData.EventLabel, EventData.MET);

		OnFlightEvent.Broadcast(EventData);
	}
}
