#pragma once

#include "CoreMinimal.h"
#include "FlightEventTypes.h"
#include "TelemetryTypes.h"
#include "FlightEventDetector.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFlightEvent, const FFlightEventData&, EventData);

/**
 * Detects flight milestones from processed telemetry data.
 * Edge detection for threshold events, sliding window for Max-Q,
 * configurable altitude markers. Events are latched (fire once per flight).
 */
UCLASS(BlueprintType, Blueprintable)
class ROCKETAR_API UFlightEventDetector : public UObject
{
	GENERATED_BODY()

public:
	UFlightEventDetector();

	/** Process a frame of telemetry data. Call each tick with the latest processed data. */
	UFUNCTION(BlueprintCallable, Category = "Flight Events")
	void ProcessTelemetry(const FProcessedTelemetryData& Data);

	/** Reset all latches — allows events to fire again (e.g., for playback restart) */
	UFUNCTION(BlueprintCallable, Category = "Flight Events")
	void Reset();

	/** Get all detected events so far */
	UFUNCTION(BlueprintCallable, Category = "Flight Events")
	const TArray<FFlightEventData>& GetDetectedEvents() const { return DetectedEvents; }

	/** Fired when a new flight event is detected */
	UPROPERTY(BlueprintAssignable, Category = "Flight Events")
	FOnFlightEvent OnFlightEvent;

	/** Event detection configuration (data-driven thresholds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight Events")
	FFlightEventConfig Config;

private:
	void CheckIgnition(const FProcessedTelemetryData& Data);
	void CheckLiftoff(const FProcessedTelemetryData& Data);
	void CheckMach1(const FProcessedTelemetryData& Data);
	void CheckMaxQ(const FProcessedTelemetryData& Data);
	void CheckSRBEvents(const FProcessedTelemetryData& Data);
	void CheckMECO(const FProcessedTelemetryData& Data);
	void CheckStageSeparation(const FProcessedTelemetryData& Data);
	void CheckSecondStageIgnition(const FProcessedTelemetryData& Data);
	void CheckFairingJettison(const FProcessedTelemetryData& Data);
	void CheckSecondStageCutoff(const FProcessedTelemetryData& Data);
	void CheckApogee(const FProcessedTelemetryData& Data);
	void CheckReentry(const FProcessedTelemetryData& Data);
	void CheckChuteDeployment(const FProcessedTelemetryData& Data);
	void CheckSplashdown(const FProcessedTelemetryData& Data);
	void CheckAltitudeMarkers(const FProcessedTelemetryData& Data);

	void CheckCustomEvents(const FProcessedTelemetryData& Data);

	void FireEvent(EFlightEvent EventType, const FProcessedTelemetryData& Data, const FString& Label);
	void FireCustomEvent(const FCustomEventDefinition& Def, const FProcessedTelemetryData& Data, const FString& Label);
	void ApplyTextOffsetOverride(EFlightEvent EventType, FFlightEventData& OutData) const;
	bool IsEventLatched(EFlightEvent EventType) const;
	void LatchEvent(EFlightEvent EventType);

	// Data-driven event enable/disable and label helpers
	bool IsEventEnabled(EFlightEvent EventType) const;
	FString GetEventLabel(EFlightEvent EventType, const FString& Default, const FProcessedTelemetryData& Data, double ExtraValue = 0.0) const;
	static FString SubstituteTokens(const FString& Template, const FProcessedTelemetryData& Data, double ExtraValue = 0.0);

	// Helper: get metric value for custom events
	static double GetMetricValue(ECustomEventMetric Metric, const FProcessedTelemetryData& Data);

	// Helper: check if specific engine group is active
	bool AreSRBsActive(const TArray<float>& Thrust) const;
	bool IsCoreActive(const TArray<float>& Thrust) const;
	bool IsSecondStageActive(const TArray<float>& Thrust) const;

	// Event latches
	TSet<EFlightEvent> LatchedEvents;

	// Detected events log
	TArray<FFlightEventData> DetectedEvents;

	// Custom event latches (by EventId since all share EFlightEvent::Custom)
	TSet<FName> CustomLatchedEvents;

	// Previous metric values for custom event edge detection
	TMap<FName, double> CustomPrevValues;

	// Previous frame state for edge detection
	bool bPrevAnyEngineActive = false;
	bool bPrevSRBsActive = false;
	bool bPrevCoreActive = false;
	bool bPrevSecondStageActive = false;
	double PrevAltitude = 0.0;
	double PrevVerticalVelocity = 0.0;
	double PrevMachNumber = 0.0;
	double PrevDynamicPressure = 0.0;

	// Max-Q detection state
	double MaxQPeakValue = 0.0;
	double MaxQRisingStartMET = -1.0;
	bool bMaxQRising = false;
	double MaxQLastHigherTime = 0.0;
	// Thrust array size warning (fire once per flight)
	bool bThrustArrayWarned = false;

	// Altitude marker tracking
	double LastAltitudeMarker = 0.0;
	double HighestAltitudeMarker = 0.0;

	// Pad altitude (recorded on first frame for relative liftoff detection)
	double PadAltitude = 0.0;

	// Frame tracking
	bool bFirstFrame = true;
};
