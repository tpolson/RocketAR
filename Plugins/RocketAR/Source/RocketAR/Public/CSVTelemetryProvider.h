#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TelemetryProvider.h"
#include "TelemetryTypes.h"
#include "CSVTelemetryProvider.generated.h"

/**
 * A single row of CSV telemetry data.
 */
USTRUCT()
struct FCSVTelemetryRow
{
	GENERATED_BODY()

	double MET = 0.0;
	FVector Position = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	FVector Acceleration = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	TArray<float> EngineThrustPercent;
};

/**
 * CSV-based telemetry provider for development and testing.
 * Implements ITelemetryProvider. Loads CSV on BeginPlay, plays back at MET timing.
 * Priority 100 (highest) — takes precedence over other providers when active.
 */
UCLASS(BlueprintType, Blueprintable)
class ROCKETAR_API ACSVTelemetryProvider : public AActor, public ITelemetryProvider
{
	GENERATED_BODY()

public:
	ACSVTelemetryProvider();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ITelemetryProvider interface
	virtual FTelemetryInputData GetTelemetryData_Implementation() const override;
	virtual bool IsTelemetryAvailable_Implementation() const override;
	virtual int32 GetProviderPriority_Implementation() const override;

	// Playback controls
	UFUNCTION(BlueprintCallable, Category = "CSV Playback")
	void Play();

	UFUNCTION(BlueprintCallable, Category = "CSV Playback")
	void Pause();

	UFUNCTION(BlueprintCallable, Category = "CSV Playback")
	void SetTimeScale(float Scale);

	UFUNCTION(BlueprintCallable, Category = "CSV Playback")
	void ScrubToTime(double MET);

	UFUNCTION(BlueprintCallable, Category = "CSV Playback")
	void StepForward();

	UFUNCTION(BlueprintCallable, Category = "CSV Playback")
	void StepBack();

	UFUNCTION(BlueprintCallable, Category = "CSV Playback")
	void SetLooping(bool bLoop);

	UFUNCTION(BlueprintCallable, Category = "CSV Playback")
	void ResetPlayback();

	UFUNCTION(BlueprintCallable, Category = "CSV Playback")
	bool IsPlaying() const { return bIsPlaying; }

	UFUNCTION(BlueprintCallable, Category = "CSV Playback")
	double GetCurrentMET() const { return CurrentMET; }

	UFUNCTION(BlueprintCallable, Category = "CSV Playback")
	float GetTimeScale() const { return TimeScale; }

	UFUNCTION(BlueprintCallable, Category = "CSV Playback")
	int32 GetRowCount() const { return Rows.Num(); }

	/** Path to the CSV file (relative to Content directory or absolute) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSV")
	FString CSVFilePath = TEXT("Data/SimulatedTelemetry.csv");

	/** Auto-play on BeginPlay */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSV Playback")
	bool bAutoPlay = true;

	/** Whether the CSV was loaded successfully */
	UPROPERTY(BlueprintReadOnly, Category = "CSV")
	bool bCSVLoaded = false;

private:
	bool LoadCSV(const FString& FilePath);
	bool ParseRow(const FString& Line, int32 LineNumber, FCSVTelemetryRow& OutRow);
	bool ValidateRows();
	int32 FindRowIndexForMET(double MET) const;

	TArray<FCSVTelemetryRow> Rows;
	int32 CurrentRowIndex = 0;
	double CurrentMET = 0.0;
	float TimeScale = 1.0f;
	bool bIsPlaying = false;
	bool bLooping = false;

	// Current interpolated data for GetTelemetryData
	mutable FTelemetryInputData CurrentData;
};
