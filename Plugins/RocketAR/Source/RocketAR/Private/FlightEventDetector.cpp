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
	CustomLatchedEvents.Empty();
	CustomPrevValues.Empty();
	bPrevAnyEngineActive = false;
	bPrevSRBsActive = false;
	bPrevCoreActive = false;
	bPrevSecondStageActive = false;
	PadAltitude = 0.0;
	PrevAltitude = 0.0;
	PrevVerticalVelocity = 0.0;
	PrevMachNumber = 0.0;
	PrevDynamicPressure = 0.0;
	MaxQPeakValue = 0.0;
	MaxQRisingStartMET = -1.0;
	bMaxQRising = false;
	MaxQLastHigherTime = 0.0;
	bThrustArrayWarned = false;
	LastAltitudeMarker = 0.0;
	HighestAltitudeMarker = 0.0;
	bFirstFrame = true;

	UE_LOG(LogRocketAR, Log, TEXT("FlightEventDetector: Reset"));
}

void UFlightEventDetector::ProcessTelemetry(const FProcessedTelemetryData& Data)
{
	SCOPE_CYCLE_COUNTER(STAT_EventDetection);

	// One-time thrust array size validation
	const int32 ExpectedEngines = Config.SRBEngineCount + Config.CoreEngineCount + 1;
	if (Data.RawData.EngineThrustPercent.Num() < ExpectedEngines && !bThrustArrayWarned)
	{
		UE_LOG(LogRocketAR, Warning, TEXT("Thrust array has %d engines, expected %d (SRB:%d + Core:%d + 1)"),
			Data.RawData.EngineThrustPercent.Num(), ExpectedEngines, Config.SRBEngineCount, Config.CoreEngineCount);
		bThrustArrayWarned = true;
	}

	if (bFirstFrame)
	{
		// Record pad altitude for relative liftoff detection
		PadAltitude = Data.AltitudeASL;

		// Initialize previous state
		bPrevAnyEngineActive = Data.bAnyEngineActive;
		bPrevSRBsActive = AreSRBsActive(Data.RawData.EngineThrustPercent);
		bPrevCoreActive = IsCoreActive(Data.RawData.EngineThrustPercent);
		bPrevSecondStageActive = IsSecondStageActive(Data.RawData.EngineThrustPercent);
		PrevAltitude = Data.AltitudeASL;
		PrevVerticalVelocity = Data.VerticalVelocity;
		PrevMachNumber = Data.MachNumber;
		PrevDynamicPressure = Data.DynamicPressurePa;

		// Initialize custom event previous values
		for (const FCustomEventDefinition& Def : Config.CustomEvents)
		{
			CustomPrevValues.Add(Def.EventId, GetMetricValue(Def.Metric, Data));
		}

		bFirstFrame = false;
		return;
	}

	// Check all built-in events
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

	// Check custom events
	CheckCustomEvents(Data);

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

// --- Data-driven enable/disable and label helpers ---

bool UFlightEventDetector::IsEventEnabled(EFlightEvent EventType) const
{
	for (const auto& Override : Config.EventOverrides)
	{
		if (Override.EventType == EventType)
		{
			return Override.bEnabled;
		}
	}
	return true; // no override = enabled
}

FString UFlightEventDetector::GetEventLabel(EFlightEvent EventType, const FString& Default, const FProcessedTelemetryData& Data, double ExtraValue) const
{
	const FBuiltinEventOverride* Found = nullptr;
	for (const auto& Override : Config.EventOverrides)
	{
		if (Override.EventType == EventType) { Found = &Override; break; }
	}
	if (Found && !Found->LabelOverride.IsEmpty())
	{
		const FString Result = SubstituteTokens(Found->LabelOverride, Data, ExtraValue);
		UE_LOG(LogRocketAR, Log, TEXT("GetEventLabel: Event %d using override '%s' → '%s'"),
			static_cast<uint8>(EventType), *Found->LabelOverride, *Result);
		return Result;
	}
	UE_LOG(LogRocketAR, Verbose, TEXT("GetEventLabel: Event %d using default '%s' (override %s)"),
		static_cast<uint8>(EventType), *Default,
		Found ? TEXT("found but empty") : TEXT("not found"));
	return Default;
}

FString UFlightEventDetector::SubstituteTokens(const FString& Template, const FProcessedTelemetryData& Data, double ExtraValue)
{
	FString Result = Template;
	Result.ReplaceInline(TEXT("{alt_km}"), *FString::Printf(TEXT("%.1f"), Data.AltitudeASL / 1000.0));
	Result.ReplaceInline(TEXT("{alt_m}"), *FString::Printf(TEXT("%.0f"), Data.AltitudeASL));
	Result.ReplaceInline(TEXT("{vel}"), *FString::Printf(TEXT("%.0f"), Data.VelocityMagnitude));
	Result.ReplaceInline(TEXT("{mach}"), *FString::Printf(TEXT("%.1f"), Data.MachNumber));
	Result.ReplaceInline(TEXT("{q_pa}"), *FString::Printf(TEXT("%.0f"), Data.DynamicPressurePa));
	Result.ReplaceInline(TEXT("{met}"), *FString::Printf(TEXT("%.1f"), Data.RawData.MissionElapsedTime));
	Result.ReplaceInline(TEXT("{gforce}"), *FString::Printf(TEXT("%.1f"), Data.GForce));
	Result.ReplaceInline(TEXT("{extra}"), *FString::Printf(TEXT("%.0f"), ExtraValue));
	return Result;
}

double UFlightEventDetector::GetMetricValue(ECustomEventMetric Metric, const FProcessedTelemetryData& Data)
{
	switch (Metric)
	{
	case ECustomEventMetric::Altitude:        return Data.AltitudeASL;
	case ECustomEventMetric::Velocity:        return Data.VelocityMagnitude;
	case ECustomEventMetric::MachNumber:      return Data.MachNumber;
	case ECustomEventMetric::DynamicPressure: return Data.DynamicPressurePa;
	case ECustomEventMetric::GForce:          return Data.GForce;
	case ECustomEventMetric::MET:             return Data.RawData.MissionElapsedTime;
	default:                                  return 0.0;
	}
}

// --- Event firing ---

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

	ApplyTextOffsetOverride(EventType, EventData);

	DetectedEvents.Add(EventData);
	LatchEvent(EventType);

	UE_LOG(LogRocketAR, Log, TEXT("FLIGHT EVENT: %s at MET=%.1fs, Alt=%.0fm, Vel=%.0fm/s"),
		*Label, EventData.MET, EventData.Altitude, EventData.Velocity);

	OnFlightEvent.Broadcast(EventData);
}

void UFlightEventDetector::FireCustomEvent(const FCustomEventDefinition& Def, const FProcessedTelemetryData& Data, const FString& Label)
{
	FFlightEventData EventData;
	EventData.EventType = EFlightEvent::Custom;
	EventData.MET = Data.RawData.MissionElapsedTime;
	EventData.Altitude = Data.AltitudeASL;
	EventData.Velocity = Data.VelocityMagnitude;
	EventData.EventLabel = Label;
	EventData.ECEFPosition = Data.VehicleECEFPosition;
	EventData.CustomEventId = Def.EventId;

	if (Def.bOverrideTextOffset)
	{
		EventData.bHasTextOffsetOverride = true;
		EventData.TextOffsetOverride = Def.TextOffsetOverride;
	}

	DetectedEvents.Add(EventData);
	CustomLatchedEvents.Add(Def.EventId);

	UE_LOG(LogRocketAR, Log, TEXT("CUSTOM EVENT [%s]: %s at MET=%.1fs, Alt=%.0fm, Vel=%.0fm/s"),
		*Def.EventId.ToString(), *Label, EventData.MET, EventData.Altitude, EventData.Velocity);

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

void UFlightEventDetector::ApplyTextOffsetOverride(EFlightEvent EventType, FFlightEventData& OutData) const
{
	for (const auto& Override : Config.EventOverrides)
	{
		if (Override.EventType == EventType && Override.bOverrideTextOffset)
		{
			OutData.bHasTextOffsetOverride = true;
			OutData.TextOffsetOverride = Override.TextOffsetOverride;
			return;
		}
	}
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

// --- Individual event checks ---

void UFlightEventDetector::CheckIgnition(const FProcessedTelemetryData& Data)
{
	if (!IsEventEnabled(EFlightEvent::Ignition)) return;
	if (IsEventLatched(EFlightEvent::Ignition)) return;

	// Rising edge: any engine becomes active
	if (Data.bAnyEngineActive && !bPrevAnyEngineActive)
	{
		const FString Label = GetEventLabel(EFlightEvent::Ignition, TEXT("IGNITION"), Data);
		FireEvent(EFlightEvent::Ignition, Data, Label);
	}
}

void UFlightEventDetector::CheckLiftoff(const FProcessedTelemetryData& Data)
{
	if (!IsEventEnabled(EFlightEvent::Liftoff)) return;
	if (IsEventLatched(EFlightEvent::Liftoff)) return;

	// Altitude above pad crosses threshold (rising edge)
	const double AltAbovePad = Data.AltitudeASL - PadAltitude;
	const double PrevAltAbovePad = PrevAltitude - PadAltitude;
	if (AltAbovePad > Config.LiftoffAltitudeThreshold && PrevAltAbovePad <= Config.LiftoffAltitudeThreshold)
	{
		const FString Label = GetEventLabel(EFlightEvent::Liftoff, TEXT("LIFTOFF"), Data);
		FireEvent(EFlightEvent::Liftoff, Data, Label);
	}
}

void UFlightEventDetector::CheckMach1(const FProcessedTelemetryData& Data)
{
	if (!IsEventEnabled(EFlightEvent::Mach1)) return;
	if (IsEventLatched(EFlightEvent::Mach1)) return;

	if (Data.MachNumber >= Config.Mach1Threshold && PrevMachNumber < Config.Mach1Threshold)
	{
		const FString DefaultLabel = FString::Printf(TEXT("MACH 1 | %.0f m/s"), Data.VelocityMagnitude);
		const FString Label = GetEventLabel(EFlightEvent::Mach1, DefaultLabel, Data);
		FireEvent(EFlightEvent::Mach1, Data, Label);
	}
}

void UFlightEventDetector::CheckMaxQ(const FProcessedTelemetryData& Data)
{
	if (!IsEventEnabled(EFlightEvent::MaxQ)) return;
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
				const FString DefaultLabel = FString::Printf(TEXT("MAX Q | %.0f Pa"), MaxQPeakValue);
				const FString Label = GetEventLabel(EFlightEvent::MaxQ, DefaultLabel, Data, MaxQPeakValue);
				FireEvent(EFlightEvent::MaxQ, Data, Label);
			}
		}
	}
}

void UFlightEventDetector::CheckSRBEvents(const FProcessedTelemetryData& Data)
{
	const bool bSRBsNow = AreSRBsActive(Data.RawData.EngineThrustPercent);

	// SRB Ignition
	if (IsEventEnabled(EFlightEvent::SRBIgnition) &&
		!IsEventLatched(EFlightEvent::SRBIgnition) && bSRBsNow && !bPrevSRBsActive)
	{
		const FString Label = GetEventLabel(EFlightEvent::SRBIgnition, TEXT("SRB IGNITION"), Data);
		FireEvent(EFlightEvent::SRBIgnition, Data, Label);
	}

	// SRB Separation
	if (IsEventEnabled(EFlightEvent::SRBSeparation) &&
		!IsEventLatched(EFlightEvent::SRBSeparation) && !bSRBsNow && bPrevSRBsActive)
	{
		// Only fire if SRBs were previously active (not at startup)
		if (IsEventLatched(EFlightEvent::SRBIgnition))
		{
			const FString DefaultLabel = FString::Printf(TEXT("SRB SEP | %.0f km"), Data.AltitudeASL / 1000.0);
			const FString Label = GetEventLabel(EFlightEvent::SRBSeparation, DefaultLabel, Data);
			FireEvent(EFlightEvent::SRBSeparation, Data, Label);
		}
	}
}

void UFlightEventDetector::CheckMECO(const FProcessedTelemetryData& Data)
{
	if (!IsEventEnabled(EFlightEvent::MECO)) return;
	if (IsEventLatched(EFlightEvent::MECO)) return;

	const bool bCoreNow = IsCoreActive(Data.RawData.EngineThrustPercent);

	// Core engines shut down (falling edge)
	if (!bCoreNow && bPrevCoreActive && IsEventLatched(EFlightEvent::Ignition))
	{
		const FString Label = GetEventLabel(EFlightEvent::MECO, TEXT("MECO"), Data);
		FireEvent(EFlightEvent::MECO, Data, Label);
	}
}

void UFlightEventDetector::CheckStageSeparation(const FProcessedTelemetryData& Data)
{
	if (!IsEventEnabled(EFlightEvent::StageSeparation)) return;
	if (IsEventLatched(EFlightEvent::StageSeparation)) return;

	// Stage separation detected shortly after MECO
	if (IsEventLatched(EFlightEvent::MECO) && !IsCoreActive(Data.RawData.EngineThrustPercent))
	{
		const FFlightEventData* MECOEvent = nullptr;
		for (const auto& Evt : DetectedEvents)
		{
			if (Evt.EventType == EFlightEvent::MECO)
			{
				MECOEvent = &Evt;
				break;
			}
		}

		if (MECOEvent && (Data.RawData.MissionElapsedTime - MECOEvent->MET) >= Config.StageSeparationDelay)
		{
			const FString Label = GetEventLabel(EFlightEvent::StageSeparation, TEXT("STAGE SEP"), Data);
			FireEvent(EFlightEvent::StageSeparation, Data, Label);
		}
	}
}

void UFlightEventDetector::CheckSecondStageIgnition(const FProcessedTelemetryData& Data)
{
	if (!IsEventEnabled(EFlightEvent::SecondStageIgnition)) return;
	if (IsEventLatched(EFlightEvent::SecondStageIgnition)) return;

	const bool bS2Now = IsSecondStageActive(Data.RawData.EngineThrustPercent);

	if (bS2Now && !bPrevSecondStageActive && IsEventLatched(EFlightEvent::MECO))
	{
		const FString Label = GetEventLabel(EFlightEvent::SecondStageIgnition, TEXT("2ND STAGE IGN"), Data);
		FireEvent(EFlightEvent::SecondStageIgnition, Data, Label);
	}
}

void UFlightEventDetector::CheckFairingJettison(const FProcessedTelemetryData& Data)
{
	if (!IsEventEnabled(EFlightEvent::FairingJettison)) return;
	if (IsEventLatched(EFlightEvent::FairingJettison)) return;

	if (Data.AltitudeASL > Config.FairingAltitudeThreshold &&
		Data.DynamicPressurePa < Config.FairingQThreshold &&
		IsEventLatched(EFlightEvent::Liftoff))
	{
		const FString DefaultLabel = FString::Printf(TEXT("FAIRING SEP | %.0f km"), Data.AltitudeASL / 1000.0);
		const FString Label = GetEventLabel(EFlightEvent::FairingJettison, DefaultLabel, Data);
		FireEvent(EFlightEvent::FairingJettison, Data, Label);
	}
}

void UFlightEventDetector::CheckSecondStageCutoff(const FProcessedTelemetryData& Data)
{
	if (!IsEventEnabled(EFlightEvent::SecondStageCutoff)) return;
	if (IsEventLatched(EFlightEvent::SecondStageCutoff)) return;

	const bool bS2Now = IsSecondStageActive(Data.RawData.EngineThrustPercent);

	if (!bS2Now && bPrevSecondStageActive && IsEventLatched(EFlightEvent::SecondStageIgnition))
	{
		const FString Label = GetEventLabel(EFlightEvent::SecondStageCutoff, TEXT("SECO"), Data);
		FireEvent(EFlightEvent::SecondStageCutoff, Data, Label);
	}
}

void UFlightEventDetector::CheckApogee(const FProcessedTelemetryData& Data)
{
	if (!IsEventEnabled(EFlightEvent::Apogee)) return;
	if (IsEventLatched(EFlightEvent::Apogee)) return;

	// Vertical velocity goes from positive to negative (or zero)
	// Only after MECO to avoid false triggers during ascent oscillations
	if (IsEventLatched(EFlightEvent::MECO) &&
		Data.VerticalVelocity <= 0.0 && PrevVerticalVelocity > 0.0)
	{
		const FString DefaultLabel = FString::Printf(TEXT("APOGEE | %.1f km"), Data.AltitudeASL / 1000.0);
		const FString Label = GetEventLabel(EFlightEvent::Apogee, DefaultLabel, Data);
		FireEvent(EFlightEvent::Apogee, Data, Label);
	}
}

void UFlightEventDetector::CheckReentry(const FProcessedTelemetryData& Data)
{
	if (!IsEventEnabled(EFlightEvent::ReentryStart)) return;
	if (IsEventLatched(EFlightEvent::ReentryStart)) return;

	// Reentry: dynamic pressure increases above threshold while descending
	if (IsEventLatched(EFlightEvent::Apogee) &&
		Data.DynamicPressurePa > Config.ReentryQThreshold &&
		PrevDynamicPressure <= Config.ReentryQThreshold)
	{
		const FString Label = GetEventLabel(EFlightEvent::ReentryStart, TEXT("REENTRY"), Data);
		FireEvent(EFlightEvent::ReentryStart, Data, Label);
	}
}

void UFlightEventDetector::CheckChuteDeployment(const FProcessedTelemetryData& Data)
{
	if (!IsEventEnabled(EFlightEvent::ChuteDeployment)) return;
	if (IsEventLatched(EFlightEvent::ChuteDeployment)) return;

	// Altitude drops below threshold while descending
	if (IsEventLatched(EFlightEvent::Apogee) &&
		Data.AltitudeASL <= Config.ChuteDeployAltitude &&
		PrevAltitude > Config.ChuteDeployAltitude &&
		Data.VerticalVelocity < 0.0)
	{
		const FString Label = GetEventLabel(EFlightEvent::ChuteDeployment, TEXT("CHUTE DEPLOY"), Data);
		FireEvent(EFlightEvent::ChuteDeployment, Data, Label);
	}
}

void UFlightEventDetector::CheckSplashdown(const FProcessedTelemetryData& Data)
{
	if (!IsEventEnabled(EFlightEvent::Splashdown)) return;
	if (IsEventLatched(EFlightEvent::Splashdown)) return;

	if (IsEventLatched(EFlightEvent::Apogee) &&
		Data.AltitudeASL <= Config.SplashdownAltitude &&
		PrevAltitude > Config.SplashdownAltitude)
	{
		const FString Label = GetEventLabel(EFlightEvent::Splashdown, TEXT("SPLASHDOWN"), Data);
		FireEvent(EFlightEvent::Splashdown, Data, Label);
	}
}

void UFlightEventDetector::CheckAltitudeMarkers(const FProcessedTelemetryData& Data)
{
	if (!IsEventEnabled(EFlightEvent::AltitudeMarker)) return;

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

		// Check for label override
		const FBuiltinEventOverride* Override = nullptr;
		for (const auto& Ovr : Config.EventOverrides)
		{
			if (Ovr.EventType == EFlightEvent::AltitudeMarker) { Override = &Ovr; break; }
		}
		if (Override && !Override->LabelOverride.IsEmpty())
		{
			EventData.EventLabel = SubstituteTokens(Override->LabelOverride, Data, MarkerAlt);
		}
		else if (MarkerAlt >= 1000.0)
		{
			EventData.EventLabel = FString::Printf(TEXT("%.0f km"), MarkerAlt / 1000.0);
		}
		else
		{
			EventData.EventLabel = FString::Printf(TEXT("%.0f m"), MarkerAlt);
		}

		ApplyTextOffsetOverride(EFlightEvent::AltitudeMarker, EventData);

		DetectedEvents.Add(EventData);

		UE_LOG(LogRocketAR, Log, TEXT("ALTITUDE MARKER: %s at MET=%.1fs"),
			*EventData.EventLabel, EventData.MET);

		OnFlightEvent.Broadcast(EventData);
	}
}

void UFlightEventDetector::CheckCustomEvents(const FProcessedTelemetryData& Data)
{
	for (const FCustomEventDefinition& Def : Config.CustomEvents)
	{
		// Skip disabled
		if (!Def.bEnabled) continue;

		// Skip if no EventId
		if (Def.EventId.IsNone()) continue;

		// Skip if already latched
		if (CustomLatchedEvents.Contains(Def.EventId)) continue;

		// Check prerequisite
		if (Def.Prerequisite != EFlightEvent::MAX && !IsEventLatched(Def.Prerequisite))
		{
			continue;
		}

		const double CurrentValue = GetMetricValue(Def.Metric, Data);
		const double* PrevValuePtr = CustomPrevValues.Find(Def.EventId);
		const double PrevValue = PrevValuePtr ? *PrevValuePtr : CurrentValue;

		// Edge detection
		bool bFired = false;
		if (Def.Direction == ECustomEventDirection::RisingEdge)
		{
			bFired = (CurrentValue >= Def.Threshold && PrevValue < Def.Threshold);
		}
		else // FallingEdge
		{
			bFired = (CurrentValue <= Def.Threshold && PrevValue > Def.Threshold);
		}

		if (bFired)
		{
			const FString Label = SubstituteTokens(Def.Label, Data, CurrentValue);
			FireCustomEvent(Def, Data, Label);
		}

		// Always update previous value
		CustomPrevValues.Add(Def.EventId, CurrentValue);
	}
}
