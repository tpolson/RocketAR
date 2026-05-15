#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RocketARMediaOutput.generated.h"

class UBlackmagicMediaOutput;
class UBlackmagicCustomTimeStep;
class UBlackmagicTimecodeProvider;
class UMediaCapture;
class UMediaOutput;
class UTextureRenderTarget2D;

/**
 * Output resolution presets for DeckLink SDI output.
 */
UENUM(BlueprintType)
enum class ERocketAROutputResolution : uint8
{
	Res_1080p60    UMETA(DisplayName = "1080p 60Hz"),
	Res_1080p5994  UMETA(DisplayName = "1080p 59.94Hz"),
	Res_2160p60    UMETA(DisplayName = "2160p 60Hz (4K)"),
	Res_2160p5994  UMETA(DisplayName = "2160p 59.94Hz (4K)")
};

/**
 * DeckLink fill/key SDI output component.
 * Creates a BlackmagicMediaOutput programmatically, configures it for fill/key,
 * and captures a UTextureRenderTarget2D supplied by the camera manager
 * (each rig owns its own production-camera SceneCapture). The game viewport
 * is decoupled from broadcast output — operator UI on the viewport never
 * appears in SDI.
 *
 * No manual asset creation required — just enable DeckLink on the setup actor,
 * pick a resolution, and hit Play.
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

	/** Deferred initialization — creates the media output, configures it, and optionally starts capture. */
	UFUNCTION(BlueprintCallable, Category = "DeckLink")
	void Initialize();

	/** Start capturing the active viewport to DeckLink fill/key */
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

	/** Human-readable status for HUD display */
	UFUNCTION(BlueprintCallable, Category = "DeckLink")
	FString GetStatusString() const;

	// --- Configuration ---

	/** Output resolution preset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	ERocketAROutputResolution OutputResolution = ERocketAROutputResolution::Res_1080p60;

	/** Use Fill+Key (two SDI for alpha keying) vs Fill only (single HDMI/SDI) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	bool bFillAndKey = false;

	/** Auto-start capture on Initialize() */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	bool bAutoStart = true;

	/** Auto-restart capture on failure */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	bool bAutoRestart = true;

	/** Seconds to wait before auto-restart after failure */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	float RestartDelay = 2.0f;

	/** Maximum restart attempts before giving up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	int32 MaxRestartAttempts = 10;

	/** Defer initialization until Initialize() is called (for setup actor ordering) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	bool bManualInit = false;

	// --- Sync ---

	/** Enable genlock (lock UE render to house sync) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink|Sync")
	bool bEnableGenlock = false;

	/** Enable timecode embedding */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink|Sync")
	bool bEnableTimecode = false;

	// --- Source ---

	/** Render target captured into the SDI fill/key feed. Supplied by the camera manager. */
	UPROPERTY(BlueprintReadWrite, Category = "DeckLink")
	TObjectPtr<UTextureRenderTarget2D> SourceRenderTarget;

	/** Swap the source render target. If currently capturing, the capture is restarted on the new RT. */
	UFUNCTION(BlueprintCallable, Category = "DeckLink")
	void SetSourceRenderTarget(UTextureRenderTarget2D* RT);

	/** Delegate target for URocketARCameraManager::OnActiveRigChanged. */
	UFUNCTION()
	void OnActiveRigChanged(UTextureRenderTarget2D* NewRT);

private:
	/** Create and configure a UBlackmagicMediaOutput programmatically */
	void CreateMediaOutput();

	/** Set up genlock via BlackmagicCustomTimeStep */
	void SetupGenlock();
	void TeardownGenlock();

	/** Set up timecode provider */
	void SetupTimecode();
	void TeardownTimecode();

	/** Callback for media capture state changes */
	UFUNCTION()
	void OnCaptureStateChanged();

	static FString GetResolutionDisplayName(ERocketAROutputResolution Res);

	bool bDeckLinkAvailable = false;
	bool bCaptureActive = false;
	bool bNeedsRestart = false;
	bool bInitialized = false;
	float RestartTimer = 0.0f;
	int32 RestartAttemptCount = 0;

	UPROPERTY()
	UMediaOutput* CreatedMediaOutput = nullptr;

	UPROPERTY()
	UMediaCapture* MediaCapture = nullptr;

	UPROPERTY()
	UBlackmagicCustomTimeStep* GenlockTimeStep = nullptr;

	UPROPERTY()
	UBlackmagicTimecodeProvider* TimecodeProvider = nullptr;
};
