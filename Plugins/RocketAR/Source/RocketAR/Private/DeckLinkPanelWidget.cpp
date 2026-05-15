#include "DeckLinkPanelWidget.h"
#include "RocketARSetupActor.h"
#include "RocketARMediaOutput.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"

void UDeckLinkPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ResolutionCombo)
	{
		ResolutionCombo->ClearOptions();
		ResolutionCombo->AddOption(ResolutionToString(ERocketAROutputResolution::Res_1080p60));
		ResolutionCombo->AddOption(ResolutionToString(ERocketAROutputResolution::Res_1080p5994));
		ResolutionCombo->AddOption(ResolutionToString(ERocketAROutputResolution::Res_2160p60));
		ResolutionCombo->AddOption(ResolutionToString(ERocketAROutputResolution::Res_2160p5994));
	}
}

void UDeckLinkPanelWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	if (StatusText && SetupActor)
	{
		if (URocketARMediaOutput* MO = SetupActor->GetMediaOutput())
		{
			StatusText->SetText(FText::FromString(MO->GetStatusString()));
		}
	}
}

void UDeckLinkPanelWidget::Refresh(ARocketARSetupActor* InSetupActor)
{
	SetupActor = InSetupActor;
	if (!SetupActor) return;

	if (EnableCheck)     EnableCheck->SetIsChecked(SetupActor->bEnableDeckLink);
	if (FillAndKeyCheck) FillAndKeyCheck->SetIsChecked(SetupActor->bDeckLinkFillAndKey);
	if (AutoStartCheck)  AutoStartCheck->SetIsChecked(SetupActor->bDeckLinkAutoStart);
	if (GenlockCheck)    GenlockCheck->SetIsChecked(SetupActor->bDeckLinkGenlock);
	if (TimecodeCheck)   TimecodeCheck->SetIsChecked(SetupActor->bDeckLinkTimecode);
	if (ResolutionCombo) ResolutionCombo->SetSelectedOption(ResolutionToString(SetupActor->DeckLinkOutputResolution));
}

void UDeckLinkPanelWidget::Apply(ARocketARSetupActor* InSetupActor)
{
	SetupActor = InSetupActor;
	if (!SetupActor) return;

	if (EnableCheck)     SetupActor->bEnableDeckLink      = EnableCheck->IsChecked();
	if (FillAndKeyCheck) SetupActor->bDeckLinkFillAndKey  = FillAndKeyCheck->IsChecked();
	if (AutoStartCheck)  SetupActor->bDeckLinkAutoStart   = AutoStartCheck->IsChecked();
	if (GenlockCheck)    SetupActor->bDeckLinkGenlock     = GenlockCheck->IsChecked();
	if (TimecodeCheck)   SetupActor->bDeckLinkTimecode    = TimecodeCheck->IsChecked();
	if (ResolutionCombo) SetupActor->DeckLinkOutputResolution = ResolutionFromString(ResolutionCombo->GetSelectedOption());

	// Propagate resolution change to the live MediaOutput component
	if (URocketARMediaOutput* MO = SetupActor->GetMediaOutput())
	{
		MO->OutputResolution = SetupActor->DeckLinkOutputResolution;
		MO->bFillAndKey      = SetupActor->bDeckLinkFillAndKey;
		MO->bEnableGenlock   = SetupActor->bDeckLinkGenlock;
		MO->bEnableTimecode  = SetupActor->bDeckLinkTimecode;
	}
}

FString UDeckLinkPanelWidget::ResolutionToString(ERocketAROutputResolution Res)
{
	switch (Res)
	{
	case ERocketAROutputResolution::Res_1080p60:   return TEXT("1080p60");
	case ERocketAROutputResolution::Res_1080p5994: return TEXT("1080p59.94");
	case ERocketAROutputResolution::Res_2160p60:   return TEXT("2160p60");
	case ERocketAROutputResolution::Res_2160p5994: return TEXT("2160p59.94");
	default: return TEXT("1080p60");
	}
}

ERocketAROutputResolution UDeckLinkPanelWidget::ResolutionFromString(const FString& Str)
{
	if (Str == TEXT("1080p59.94")) return ERocketAROutputResolution::Res_1080p5994;
	if (Str == TEXT("2160p60"))    return ERocketAROutputResolution::Res_2160p60;
	if (Str == TEXT("2160p59.94")) return ERocketAROutputResolution::Res_2160p5994;
	return ERocketAROutputResolution::Res_1080p60;
}
