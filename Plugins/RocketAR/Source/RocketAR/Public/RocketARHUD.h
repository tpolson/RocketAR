#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TelemetryTypes.h"
#include "FlightEventTypes.h"
#include "RocketARHUD.generated.h"

/**
 * Simple canvas-based HUD overlay for RocketAR.
 * Draws event name (with fade), altitude, and velocity directly to screen.
 * No Blueprint/UMG assets required.
 */
UCLASS()
class ROCKETAR_API ARocketARHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Call when a flight event is detected */
	void ShowEvent(const FFlightEventData& EventData);

	/** Call each frame with current telemetry */
	void UpdateTelemetry(const FProcessedTelemetryData& Data);

private:
	// Current telemetry values
	double CurrentAltitude = 0.0;
	double CurrentVelocity = 0.0;
	double CurrentMET = 0.0;
	bool bHasTelemetry = false;

	// Event display
	FString CurrentEventName;
	float EventDisplayTimer = 0.0f;
	float EventDisplayDuration = 5.0f;
	float EventFadeDuration = 1.0f;

	FString FormatAltitude(double AltMeters) const;
	FString FormatMET(double MET) const;
};
