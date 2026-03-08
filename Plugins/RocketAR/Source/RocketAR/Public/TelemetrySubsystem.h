#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TelemetryTypes.h"
#include "TelemetrySubsystem.generated.h"

class UHermiteInterpolator;
class ACesiumGeoreference;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTelemetryUpdated, const FProcessedTelemetryData&, Data);

/**
 * World subsystem managing the telemetry data pipeline.
 * Auto-discovers ITelemetryProvider actors, runs ECEF conversion,
 * Hermite interpolation, and derived value computation each frame.
 */
UCLASS()
class ROCKETAR_API UTelemetrySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UTelemetrySubsystem();

	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Get the latest processed telemetry data */
	UFUNCTION(BlueprintCallable, Category = "Telemetry")
	const FProcessedTelemetryData& GetProcessedData() const { return ProcessedData; }

	/** Get the latest raw telemetry data */
	UFUNCTION(BlueprintCallable, Category = "Telemetry")
	const FTelemetryInputData& GetRawData() const { return ProcessedData.RawData; }

	/** Is telemetry currently being received? */
	UFUNCTION(BlueprintCallable, Category = "Telemetry")
	bool IsTelemetryActive() const { return bTelemetryActive; }

	/** Force re-discovery of telemetry providers */
	UFUNCTION(BlueprintCallable, Category = "Telemetry")
	void DiscoverProviders();

	/** Set the Cesium georeference for ECEF conversion */
	void SetGeoreference(ACesiumGeoreference* InGeoreference);

	/** Fired each frame after telemetry is processed */
	UPROPERTY(BlueprintAssignable, Category = "Telemetry")
	FOnTelemetryUpdated OnTelemetryUpdated;

	/** Extrapolation timeout in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry")
	float ExtrapolationTimeout = 1.0f;

private:
	void PollProvider();
	void ComputeDerivedValues(const FTelemetryInputData& RawData);
	FVector ECEFToUE(const FVector& ECEFPosition) const;
	FQuat ECEFRotationToUE(const FQuat& ECEFRotation) const;
	double ComputeAltitudeFromECEF(const FVector& ECEFPosition) const;
	FVector ComputeLocalUpVector(const FVector& ECEFPosition) const;

	UPROPERTY()
	TScriptInterface<class ITelemetryProvider> ActiveProvider;

	UPROPERTY()
	UHermiteInterpolator* Interpolator = nullptr;

	UPROPERTY()
	ACesiumGeoreference* Georeference = nullptr;

	FProcessedTelemetryData ProcessedData;
	bool bTelemetryActive = false;
	bool bProvidersDiscovered = false;
	double LastProviderPollTime = 0.0;
	double WorldTimeAccumulator = 0.0;
};
