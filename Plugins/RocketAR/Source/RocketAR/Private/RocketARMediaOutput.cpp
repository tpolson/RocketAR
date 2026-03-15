#include "RocketARMediaOutput.h"
#include "RocketARModule.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "MediaCapture.h"
#include "MediaOutput.h"

#if WITH_BLACKMAGIC
#include "BlackmagicMediaOutput.h"
#include "BlackmagicCustomTimeStep.h"
#include "BlackmagicTimecodeProvider.h"
#include "BlackmagicDeviceProvider.h"
#include "MediaIOCoreDefinitions.h"
#endif

URocketARMediaOutput::URocketARMediaOutput()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URocketARMediaOutput::BeginPlay()
{
	Super::BeginPlay();

	if (!bManualInit)
	{
		Initialize();
	}
	else
	{
		UE_LOG(LogRocketAR, Log, TEXT("DeckLink: Deferred init — waiting for Initialize() call"));
	}
}

void URocketARMediaOutput::Initialize()
{
	if (bInitialized)
	{
		UE_LOG(LogRocketAR, Warning, TEXT("DeckLink: Already initialized"));
		return;
	}
	bInitialized = true;

#if WITH_BLACKMAGIC
	CreateMediaOutput();

	if (bDeckLinkAvailable)
	{
		if (bEnableGenlock) SetupGenlock();
		if (bEnableTimecode) SetupTimecode();
		if (bAutoStart) StartCapture();
	}
#else
	bDeckLinkAvailable = false;
	UE_LOG(LogRocketAR, Log, TEXT("DeckLink: Disabled (compiled without WITH_BLACKMAGIC)"));
#endif
}

void URocketARMediaOutput::CreateMediaOutput()
{
#if WITH_BLACKMAGIC
	// Auto-detect DeckLink card and find a matching output configuration
	FBlackmagicDeviceProvider DeviceProvider;

	TArray<FMediaIODevice> Devices = DeviceProvider.GetDevices();
	if (Devices.Num() == 0)
	{
		UE_LOG(LogRocketAR, Warning, TEXT("DeckLink: No Blackmagic devices found"));
		return;
	}

	UE_LOG(LogRocketAR, Log, TEXT("DeckLink: Found %d device(s)"), Devices.Num());
	for (const FMediaIODevice& Dev : Devices)
	{
		UE_LOG(LogRocketAR, Log, TEXT("  Device %d: %s"), Dev.DeviceIdentifier, *Dev.DeviceName.ToString());
	}

	// Target resolution and frame rate from enum
	FIntPoint TargetRes;
	FFrameRate TargetRate;
	switch (OutputResolution)
	{
	case ERocketAROutputResolution::Res_1080p60:
		TargetRes = FIntPoint(1920, 1080);
		TargetRate = FFrameRate(60000, 1000);
		break;
	case ERocketAROutputResolution::Res_1080p5994:
		TargetRes = FIntPoint(1920, 1080);
		TargetRate = FFrameRate(60000, 1001);
		break;
	case ERocketAROutputResolution::Res_2160p60:
		TargetRes = FIntPoint(3840, 2160);
		TargetRate = FFrameRate(60000, 1000);
		break;
	case ERocketAROutputResolution::Res_2160p5994:
		TargetRes = FIntPoint(3840, 2160);
		TargetRate = FFrameRate(60000, 1001);
		break;
	}

	// Find a matching output configuration from the hardware
	TArray<FMediaIOOutputConfiguration> OutputConfigs = DeviceProvider.GetOutputConfigurations();

	UE_LOG(LogRocketAR, Log, TEXT("DeckLink: %d output configurations available"), OutputConfigs.Num());

	FMediaIOOutputConfiguration MatchedConfig;
	bool bFound = false;

	for (const FMediaIOOutputConfiguration& Config : OutputConfigs)
	{
		const FMediaIOMode& Mode = Config.MediaConfiguration.MediaMode;
		if (Mode.Resolution == TargetRes &&
			Mode.FrameRate == TargetRate &&
			Mode.Standard == EMediaIOStandardType::Progressive)
		{
			// Match output type preference
			if (bFillAndKey && Config.OutputType == EMediaIOOutputType::FillAndKey)
			{
				MatchedConfig = Config;
				bFound = true;
				break;
			}
			else if (!bFillAndKey && Config.OutputType == EMediaIOOutputType::Fill)
			{
				MatchedConfig = Config;
				bFound = true;
				break;
			}
		}
	}

	// If no exact match on output type, take any matching resolution/rate
	if (!bFound)
	{
		for (const FMediaIOOutputConfiguration& Config : OutputConfigs)
		{
			const FMediaIOMode& Mode = Config.MediaConfiguration.MediaMode;
			if (Mode.Resolution == TargetRes &&
				Mode.FrameRate == TargetRate &&
				Mode.Standard == EMediaIOStandardType::Progressive)
			{
				MatchedConfig = Config;
				bFound = true;
				UE_LOG(LogRocketAR, Warning, TEXT("DeckLink: Exact output type not found, using %s"),
					Config.OutputType == EMediaIOOutputType::FillAndKey ? TEXT("Fill+Key") : TEXT("Fill"));
				break;
			}
		}
	}

	if (!bFound)
	{
		// Log available modes for debugging
		UE_LOG(LogRocketAR, Error, TEXT("DeckLink: No matching output config for %s"), *GetResolutionDisplayName(OutputResolution));
		for (const FMediaIOOutputConfiguration& Config : OutputConfigs)
		{
			const FMediaIOMode& Mode = Config.MediaConfiguration.MediaMode;
			UE_LOG(LogRocketAR, Log, TEXT("  Available: %dx%d @ %d/%d %s %s"),
				Mode.Resolution.X, Mode.Resolution.Y,
				Mode.FrameRate.Numerator, Mode.FrameRate.Denominator,
				Mode.Standard == EMediaIOStandardType::Progressive ? TEXT("p") : TEXT("i"),
				Config.OutputType == EMediaIOOutputType::FillAndKey ? TEXT("Fill+Key") : TEXT("Fill"));
		}
		return;
	}

	// Create and configure the output
	UBlackmagicMediaOutput* BMOutput = NewObject<UBlackmagicMediaOutput>(this, TEXT("BlackmagicOutput"));
	if (!BMOutput)
	{
		UE_LOG(LogRocketAR, Error, TEXT("DeckLink: Failed to create BlackmagicMediaOutput"));
		return;
	}

	BMOutput->OutputConfiguration = MatchedConfig;
	BMOutput->PixelFormat = EBlackmagicMediaOutputPixelFormat::PF_8BIT_YUV;
	BMOutput->bWaitForSyncEvent = true; // Lock UE to DeckLink's free-run clock
	BMOutput->bLogDropFrame = true;

	CreatedMediaOutput = BMOutput;
	bDeckLinkAvailable = true;

	UE_LOG(LogRocketAR, Log, TEXT("DeckLink: Configured [%s, %s, Device: %s, Port: %d]"),
		*GetResolutionDisplayName(OutputResolution),
		MatchedConfig.OutputType == EMediaIOOutputType::FillAndKey ? TEXT("Fill+Key") : TEXT("Fill"),
		*MatchedConfig.MediaConfiguration.MediaConnection.Device.DeviceName.ToString(),
		MatchedConfig.MediaConfiguration.MediaConnection.PortIdentifier);
#endif
}

bool URocketARMediaOutput::StartCapture()
{
#if WITH_BLACKMAGIC
	if (!bDeckLinkAvailable || !CreatedMediaOutput)
	{
		UE_LOG(LogRocketAR, Warning, TEXT("DeckLink: Cannot start — no media output"));
		return false;
	}

	if (bCaptureActive && MediaCapture)
	{
		UE_LOG(LogRocketAR, Warning, TEXT("DeckLink: Already capturing"));
		return true;
	}

	// Create capture from the output
	MediaCapture = CreatedMediaOutput->CreateMediaCapture();
	if (!MediaCapture)
	{
		UE_LOG(LogRocketAR, Error, TEXT("DeckLink: Failed to create media capture"));
		return false;
	}

	// Listen for state changes (error, stopped, etc.)
	MediaCapture->OnStateChanged.AddDynamic(this, &URocketARMediaOutput::OnCaptureStateChanged);

	// Capture the active scene viewport (whatever camera is active — production camera)
	FMediaCaptureOptions Options;
	Options.CapturePhase = EMediaCapturePhase::EndFrame; // After alpha propagation
	Options.bSkipFrameWhenRunningExpensiveTasks = false;
	Options.bForceAlphaToOneOnConversion = false; // Preserve alpha for key signal
	Options.ResizeMethod = EMediaCaptureResizeMethod::ResizeSource; // Match viewport to output resolution
	Options.bAutoRestartOnSourceSizeChange = true; // Handle viewport resizes gracefully

	if (!MediaCapture->CaptureActiveSceneViewport(Options))
	{
		UE_LOG(LogRocketAR, Error, TEXT("DeckLink: CaptureActiveSceneViewport failed — is a DeckLink card installed?"));
		MediaCapture->OnStateChanged.RemoveDynamic(this, &URocketARMediaOutput::OnCaptureStateChanged);
		MediaCapture = nullptr;
		return false;
	}

	bCaptureActive = true;
	bNeedsRestart = false;
	RestartAttemptCount = 0;
	UE_LOG(LogRocketAR, Log, TEXT("DeckLink: Capture active [%s]"),
		*GetResolutionDisplayName(OutputResolution));
	return true;

#else
	UE_LOG(LogRocketAR, Warning, TEXT("DeckLink: Compiled without WITH_BLACKMAGIC"));
	return false;
#endif
}

void URocketARMediaOutput::StopCapture()
{
#if WITH_BLACKMAGIC
	if (MediaCapture)
	{
		MediaCapture->OnStateChanged.RemoveDynamic(this, &URocketARMediaOutput::OnCaptureStateChanged);
		MediaCapture->StopCapture(true);
		MediaCapture = nullptr;
		UE_LOG(LogRocketAR, Log, TEXT("DeckLink: Capture stopped"));
	}
#endif

	bCaptureActive = false;
	bNeedsRestart = false;
	RestartTimer = 0.0f;
}

void URocketARMediaOutput::OnCaptureStateChanged()
{
#if WITH_BLACKMAGIC
	if (!MediaCapture) return;

	EMediaCaptureState State = MediaCapture->GetState();

	switch (State)
	{
	case EMediaCaptureState::Capturing:
		bCaptureActive = true;
		UE_LOG(LogRocketAR, Log, TEXT("DeckLink: Capture active [%s]"),
			*GetResolutionDisplayName(OutputResolution));
		break;

	case EMediaCaptureState::Error:
		UE_LOG(LogRocketAR, Error, TEXT("DeckLink: Capture error"));
		bCaptureActive = false;
		MediaCapture->OnStateChanged.RemoveDynamic(this, &URocketARMediaOutput::OnCaptureStateChanged);
		MediaCapture = nullptr;

		if (bAutoRestart && RestartAttemptCount < MaxRestartAttempts)
		{
			bNeedsRestart = true;
			RestartTimer = RestartDelay;
			RestartAttemptCount++;
			UE_LOG(LogRocketAR, Warning, TEXT("DeckLink: Restart %d/%d in %.1fs"),
				RestartAttemptCount, MaxRestartAttempts, RestartDelay);
		}
		else if (RestartAttemptCount >= MaxRestartAttempts)
		{
			UE_LOG(LogRocketAR, Error, TEXT("DeckLink: Max restarts (%d) reached"), MaxRestartAttempts);
		}
		break;

	case EMediaCaptureState::Stopped:
		bCaptureActive = false;
		UE_LOG(LogRocketAR, Log, TEXT("DeckLink: Capture stopped"));
		break;

	default:
		break;
	}
#endif
}

void URocketARMediaOutput::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bNeedsRestart)
	{
		RestartTimer -= DeltaTime;
		if (RestartTimer <= 0.0f)
		{
			bNeedsRestart = false;
			UE_LOG(LogRocketAR, Log, TEXT("DeckLink: Restart attempt %d/%d..."),
				RestartAttemptCount, MaxRestartAttempts);
			StartCapture();
		}
	}
}

void URocketARMediaOutput::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCapture();
	TeardownGenlock();
	TeardownTimecode();
	Super::EndPlay(EndPlayReason);
}

void URocketARMediaOutput::SetupGenlock()
{
#if WITH_BLACKMAGIC
	// Create genlock time step programmatically
	GenlockTimeStep = NewObject<UBlackmagicCustomTimeStep>(this, TEXT("GenlockTimeStep"));
	if (GenlockTimeStep && GEngine)
	{
		GEngine->SetCustomTimeStep(GenlockTimeStep);
		UE_LOG(LogRocketAR, Log, TEXT("DeckLink: Genlock enabled"));
	}
#endif
}

void URocketARMediaOutput::TeardownGenlock()
{
#if WITH_BLACKMAGIC
	if (GenlockTimeStep && GEngine)
	{
		GEngine->SetCustomTimeStep(nullptr);
		GenlockTimeStep = nullptr;
		UE_LOG(LogRocketAR, Log, TEXT("DeckLink: Genlock disabled"));
	}
#endif
}

void URocketARMediaOutput::SetupTimecode()
{
#if WITH_BLACKMAGIC
	TimecodeProvider = NewObject<UBlackmagicTimecodeProvider>(this, TEXT("TimecodeProvider"));
	if (TimecodeProvider && GEngine)
	{
		GEngine->SetTimecodeProvider(TimecodeProvider);
		UE_LOG(LogRocketAR, Log, TEXT("DeckLink: Timecode provider set"));
	}
#endif
}

void URocketARMediaOutput::TeardownTimecode()
{
#if WITH_BLACKMAGIC
	if (TimecodeProvider && GEngine)
	{
		GEngine->SetTimecodeProvider(nullptr);
		TimecodeProvider = nullptr;
		UE_LOG(LogRocketAR, Log, TEXT("DeckLink: Timecode provider cleared"));
	}
#endif
}

FString URocketARMediaOutput::GetStatusString() const
{
	if (!bDeckLinkAvailable) return TEXT("OFF");
	if (bCaptureActive) return FString::Printf(TEXT("CAPTURING [%s]"), *GetResolutionDisplayName(OutputResolution));
	if (bNeedsRestart) return FString::Printf(TEXT("RESTARTING (%d/%d)"), RestartAttemptCount, MaxRestartAttempts);
	if (bInitialized) return TEXT("READY");
	return TEXT("OFF");
}

FString URocketARMediaOutput::GetResolutionDisplayName(ERocketAROutputResolution Res)
{
	switch (Res)
	{
	case ERocketAROutputResolution::Res_1080p60:   return TEXT("1080p60");
	case ERocketAROutputResolution::Res_1080p5994: return TEXT("1080p59.94");
	case ERocketAROutputResolution::Res_2160p60:   return TEXT("2160p60");
	case ERocketAROutputResolution::Res_2160p5994: return TEXT("2160p59.94");
	default: return TEXT("Unknown");
	}
}
