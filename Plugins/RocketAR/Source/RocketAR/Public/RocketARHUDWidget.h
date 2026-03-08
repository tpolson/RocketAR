#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TelemetryTypes.h"
#include "RocketARHUDWidget.generated.h"

class UTelemetrySubsystem;

/**
 * C++ base class for the RocketAR HUD widget.
 * Displays MET, altitude, velocity, Mach, G-force, flight phase, telemetry status.
 * The actual UMG layout is created in Blueprint inheriting from this class.
 * Toggle via bShowHUD on the setup actor.
 */
UCLASS(BlueprintType, Blueprintable)
class ROCKETAR_API URocketARHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// --- Bound values for Blueprint widget bindings ---

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FString METDisplay;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FString AltitudeDisplay;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FString VelocityDisplay;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FString MachDisplay;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FString GForceDisplay;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FString FlightPhaseDisplay;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FString TelemetryStatusDisplay;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FString DynamicPressureDisplay;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FLinearColor StatusColor = FLinearColor::Green;

protected:
	void UpdateFromTelemetry(const FProcessedTelemetryData& Data);
	FString FormatMET(double MET) const;
	FString DetermineFlightPhase(const FProcessedTelemetryData& Data) const;
	FString DetermineTelemetryStatus(const FProcessedTelemetryData& Data) const;

	UPROPERTY()
	UTelemetrySubsystem* TelemetrySubsystem = nullptr;
};
