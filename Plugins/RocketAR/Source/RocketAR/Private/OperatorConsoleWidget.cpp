#include "OperatorConsoleWidget.h"
#include "RocketARSetupActor.h"
#include "RocketAROperatorSettings.h"
#include "DeckLinkPanelWidget.h"
#include "CameraPanelWidget.h"
#include "BannerPanelWidget.h"
#include "RocketARModule.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UOperatorConsoleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ApplyButton)  ApplyButton->OnClicked.AddDynamic(this, &UOperatorConsoleWidget::OnApplyClicked);
	if (RevertButton) RevertButton->OnClicked.AddDynamic(this, &UOperatorConsoleWidget::OnRevertClicked);
	if (SaveButton)   SaveButton->OnClicked.AddDynamic(this, &UOperatorConsoleWidget::OnSaveClicked);

	SetStatus(TEXT("Ready"));
}

void UOperatorConsoleWidget::SetSetupActor(ARocketARSetupActor* InSetupActor)
{
	SetupActor = InSetupActor;
	RefreshFromSetupActor();
}

void UOperatorConsoleWidget::RefreshFromSetupActor()
{
	if (!SetupActor) return;

	if (DeckLinkPanel) DeckLinkPanel->Refresh(SetupActor);
	if (CameraPanel)   CameraPanel->Refresh(SetupActor);
	if (BannerPanel)   BannerPanel->Refresh(SetupActor);

	SetStatus(TEXT("Loaded from setup actor"));
}

void UOperatorConsoleWidget::ApplyToSetupActor()
{
	if (!SetupActor) return;

	if (DeckLinkPanel) DeckLinkPanel->Apply(SetupActor);
	if (CameraPanel)   CameraPanel->Apply(SetupActor);
	if (BannerPanel)   BannerPanel->Apply(SetupActor);

	SetStatus(TEXT("Applied (not saved)"));
}

bool UOperatorConsoleWidget::SaveSettings()
{
	if (!SetupActor)
	{
		SetStatus(TEXT("Save failed: no setup actor"));
		return false;
	}

	ApplyToSetupActor();

	const bool bOk = SetupActor->SaveOperatorSettings();
	SetStatus(bOk ? TEXT("Saved") : TEXT("Save FAILED"));
	return bOk;
}

void UOperatorConsoleWidget::OnApplyClicked()  { ApplyToSetupActor(); }
void UOperatorConsoleWidget::OnRevertClicked() { RefreshFromSetupActor(); }
void UOperatorConsoleWidget::OnSaveClicked()   { SaveSettings(); }

void UOperatorConsoleWidget::SetStatus(const FString& Message)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
	}
	UE_LOG(LogRocketAR, Log, TEXT("OperatorConsole: %s"), *Message);
}
