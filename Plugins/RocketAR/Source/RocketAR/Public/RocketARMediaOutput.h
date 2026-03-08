#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RocketARMediaOutput.generated.h"

/**
 * Helper component for DeckLink 8K Pro fill/key SDI output.
 * Currently a stub — full implementation requires BlackmagicMedia plugin
 * and MediaIOCore module dependency (uncomment in Build.cs when hardware available).
 */
UCLASS(BlueprintType, meta=(BlueprintSpawnableComponent))
class ROCKETAR_API URocketARMediaOutput : public UActorComponent
{
	GENERATED_BODY()

public:
	URocketARMediaOutput();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Start the DeckLink fill/key output */
	UFUNCTION(BlueprintCallable, Category = "DeckLink")
	bool StartCapture();

	/** Stop the DeckLink output */
	UFUNCTION(BlueprintCallable, Category = "DeckLink")
	void StopCapture();

	/** Is the DeckLink output currently active? */
	UFUNCTION(BlueprintCallable, Category = "DeckLink")
	bool IsCaptureActive() const { return bCaptureActive; }

	/** Is DeckLink hardware available? */
	UFUNCTION(BlueprintCallable, Category = "DeckLink")
	bool IsDeckLinkAvailable() const { return bDeckLinkAvailable; }

	// --- Configuration ---

	/** Path to the BlackmagicMediaOutput asset in the project */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	FSoftObjectPath MediaOutputAssetPath;

	/** Auto-start capture on BeginPlay if hardware is available */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	bool bAutoStart = false;

	/** Auto-restart capture on failure */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	bool bAutoRestart = true;

	/** Seconds to wait before auto-restart after failure */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	float RestartDelay = 2.0f;

	/** Enable genlock (BlackmagicCustomTimeStep) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	bool bEnableGenlock = false;

	/** Enable timecode embedding (BlackmagicTimecodeProvider) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	bool bEnableTimecode = false;

private:
	bool bDeckLinkAvailable = false;
	bool bCaptureActive = false;
	bool bNeedsRestart = false;
	float RestartTimer = 0.0f;
};
