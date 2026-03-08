#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TelemetryTypes.h"
#include "TelemetryProvider.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UTelemetryProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for telemetry data providers.
 * The client implements this on their plugin's actor. The CSV provider also implements it.
 * The telemetry subsystem auto-discovers actors implementing this interface at runtime.
 */
class ROCKETAR_API ITelemetryProvider
{
	GENERATED_BODY()

public:
	/**
	 * Get the current telemetry data.
	 * Client implements this in Blueprint by handling the GetTelemetryData event
	 * and filling the FTelemetryInputData struct from their internal data.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Telemetry")
	FTelemetryInputData GetTelemetryData() const;

	/** Returns true when the provider has valid telemetry available */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Telemetry")
	bool IsTelemetryAvailable() const;

	/** Returns the provider's priority (higher = preferred). CSV=100, Setup actor=50, External=75 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Telemetry")
	int32 GetProviderPriority() const;
};
