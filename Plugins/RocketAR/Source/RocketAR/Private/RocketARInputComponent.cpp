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
	PrimaryComponentTick.bCanEverTick = false;
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

void URocketARInputComponent::SetupBindings(ARocketARSetupActor* InSetupActor)
{
	SetupActor = InSetupActor;
	if (!SetupActor) return;

	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	UInputComponent* InputComp = PC->InputComponent;
	if (!InputComp) return;

	// Bind directly to keys (no need for action mappings in DefaultInput.ini)
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

	UE_LOG(LogRocketAR, Log, TEXT("Input bindings set up (Space, Arrows, [], R, D, F1, F2, Tab)"));
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

	// Clear event disks
	ADevVisualizationActor* DevVis = SetupActor->GetDevVisActor();
	if (DevVis) DevVis->ClearEventDisks();

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
	SetupActor->bShowHUD = !SetupActor->bShowHUD;
	UE_LOG(LogRocketAR, Log, TEXT("Input: HUD = %s"),
		SetupActor->bShowHUD ? TEXT("ON") : TEXT("OFF"));
}

void URocketARInputComponent::OnToggleStats()
{
	// Toggle UE stat display
	if (GEngine)
	{
		GEngine->Exec(GetWorld(), TEXT("stat RocketAR"));
	}
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
