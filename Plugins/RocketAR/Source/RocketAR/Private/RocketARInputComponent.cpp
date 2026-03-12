#include "RocketARInputComponent.h"
#include "RocketARSetupActor.h"
#include "CSVTelemetryProvider.h"
#include "BannerManager.h"
#include "FlightEventDetector.h"
#include "DevVisualizationActor.h"
#include "RocketARModule.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

URocketARInputComponent::URocketARInputComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URocketARInputComponent::BeginPlay()
{
	Super::BeginPlay();

	// Get the owning setup actor
	SetupActor = Cast<ARocketARSetupActor>(GetOwner());
	if (SetupActor)
	{
		SetupBindings(SetupActor);
	}
}

void URocketARInputComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Defer input binding until PlayerController and its InputComponent are ready
	if (!bBindingsReady && SetupActor)
	{
		SetupBindings(SetupActor);
	}
}

void URocketARInputComponent::SetupBindings(ARocketARSetupActor* InSetupActor)
{
	SetupActor = InSetupActor;
	if (!SetupActor) return;

	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	// Ensure game input is active — without a GameMode/Pawn, UE may default to UI-only
	PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));
	PC->bShowMouseCursor = true;

	// Enable input on the setup actor — puts its InputComponent on the PC's input stack
	SetupActor->EnableInput(PC);

	UInputComponent* InputComp = SetupActor->InputComponent;
	if (!InputComp) return;

	// Bind directly to keys
	InputComp->BindKey(EKeys::SpaceBar, IE_Pressed, this, &URocketARInputComponent::OnTogglePlayPause);
	InputComp->BindKey(EKeys::Right, IE_Pressed, this, &URocketARInputComponent::OnScrubForward);
	InputComp->BindKey(EKeys::Left, IE_Pressed, this, &URocketARInputComponent::OnScrubBackward);
	InputComp->BindKey(EKeys::RightBracket, IE_Pressed, this, &URocketARInputComponent::OnTimeScaleUp);
	InputComp->BindKey(EKeys::LeftBracket, IE_Pressed, this, &URocketARInputComponent::OnTimeScaleDown);
	InputComp->BindKey(EKeys::R, IE_Pressed, this, &URocketARInputComponent::OnReset);
	InputComp->BindKey(EKeys::D, IE_Pressed, this, &URocketARInputComponent::OnToggleDevVis);
	InputComp->BindKey(EKeys::F1, IE_Pressed, this, &URocketARInputComponent::OnToggleHUD);
	InputComp->BindKey(EKeys::F2, IE_Pressed, this, &URocketARInputComponent::OnToggleStats);
	InputComp->BindKey(EKeys::Tab, IE_Pressed, this, &URocketARInputComponent::OnCycleBanner);
	InputComp->BindKey(EKeys::F3, IE_Pressed, this, &URocketARInputComponent::OnToggleAlphaPreview);

	bBindingsReady = true;
	PrimaryComponentTick.SetTickFunctionEnable(false);
	UE_LOG(LogRocketAR, Log, TEXT("Input bindings set up (Space, Arrows, [], R, D, F1, F2, F3, Tab)"));
}

void URocketARInputComponent::OnTogglePlayPause()
{
	if (!SetupActor) return;
	ACSVTelemetryProvider* CSV = SetupActor->GetCSVProvider();
	if (!CSV) return;

	if (CSV->IsPlaying())
	{
		CSV->Pause();
		UE_LOG(LogRocketAR, Log, TEXT("Input: Pause"));
	}
	else
	{
		CSV->Play();
		UE_LOG(LogRocketAR, Log, TEXT("Input: Play"));
	}
}

void URocketARInputComponent::OnScrubForward()
{
	if (!SetupActor) return;
	ACSVTelemetryProvider* CSV = SetupActor->GetCSVProvider();
	if (!CSV) return;

	CSV->ScrubToTime(CSV->GetCurrentMET() + 1.0);
	UE_LOG(LogRocketAR, Log, TEXT("Input: Scrub +1s (MET=%.1f)"), CSV->GetCurrentMET());
}

void URocketARInputComponent::OnScrubBackward()
{
	if (!SetupActor) return;
	ACSVTelemetryProvider* CSV = SetupActor->GetCSVProvider();
	if (!CSV) return;

	CSV->ScrubToTime(CSV->GetCurrentMET() - 1.0);
	UE_LOG(LogRocketAR, Log, TEXT("Input: Scrub -1s (MET=%.1f)"), CSV->GetCurrentMET());
}

void URocketARInputComponent::OnTimeScaleUp()
{
	if (!SetupActor) return;
	ACSVTelemetryProvider* CSV = SetupActor->GetCSVProvider();
	if (!CSV) return;

	CSV->SetTimeScale(CSV->GetTimeScale() * 2.0f);
	UE_LOG(LogRocketAR, Log, TEXT("Input: TimeScale = %.1fx"), CSV->GetTimeScale());
}

void URocketARInputComponent::OnTimeScaleDown()
{
	if (!SetupActor) return;
	ACSVTelemetryProvider* CSV = SetupActor->GetCSVProvider();
	if (!CSV) return;

	CSV->SetTimeScale(CSV->GetTimeScale() * 0.5f);
	UE_LOG(LogRocketAR, Log, TEXT("Input: TimeScale = %.1fx"), CSV->GetTimeScale());
}

void URocketARInputComponent::OnReset()
{
	if (!SetupActor) return;

	// Reset CSV
	ACSVTelemetryProvider* CSV = SetupActor->GetCSVProvider();
	if (CSV) CSV->ResetPlayback();

	// Reset event detector
	UFlightEventDetector* Detector = SetupActor->GetEventDetector();
	if (Detector) Detector->Reset();

	// Destroy all banners
	UBannerManager* Banners = SetupActor->GetBannerManager();
	if (Banners) Banners->DestroyAllBanners();

	UE_LOG(LogRocketAR, Log, TEXT("Input: Full Reset"));
}

void URocketARInputComponent::OnToggleDevVis()
{
	if (!SetupActor) return;
	SetupActor->bDevVisualization = !SetupActor->bDevVisualization;
	UE_LOG(LogRocketAR, Log, TEXT("Input: Dev Visualization = %s"),
		SetupActor->bDevVisualization ? TEXT("ON") : TEXT("OFF"));
}

void URocketARInputComponent::OnToggleHUD()
{
	if (!SetupActor) return;

	// Toggle all three HUD elements together
	const bool bAllOn = SetupActor->bShowHUDTelemetry && SetupActor->bShowHUDEvents && SetupActor->bShowDebugMessages;
	const bool bNewState = !bAllOn;
	SetupActor->bShowHUDTelemetry = bNewState;
	SetupActor->bShowHUDEvents = bNewState;
	SetupActor->bShowDebugMessages = bNewState;
	UE_LOG(LogRocketAR, Log, TEXT("Input: HUD (all) = %s"),
		bNewState ? TEXT("ON") : TEXT("OFF"));
}

void URocketARInputComponent::OnToggleStats()
{
	// Toggle UE stat display
	if (GEngine)
	{
		GEngine->Exec(GetWorld(), TEXT("stat RocketAR"));
	}
}

void URocketARInputComponent::OnToggleAlphaPreview()
{
	if (!SetupActor) return;
	SetupActor->bShowAlphaPreview = !SetupActor->bShowAlphaPreview;
	UE_LOG(LogRocketAR, Log, TEXT("Input: Alpha Preview = %s"),
		SetupActor->bShowAlphaPreview ? TEXT("ON") : TEXT("OFF"));
}

void URocketARInputComponent::OnCycleBanner()
{
	if (!SetupActor) return;
	UBannerManager* Banners = SetupActor->GetBannerManager();
	if (!Banners || Banners->GetActiveBannerCount() == 0) return;

	CurrentBannerIndex = (CurrentBannerIndex + 1) % Banners->GetActiveBannerCount();
	UE_LOG(LogRocketAR, Log, TEXT("Input: Cycle to banner %d/%d"),
		CurrentBannerIndex + 1, Banners->GetActiveBannerCount());
}
