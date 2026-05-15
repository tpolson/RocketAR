#include "BannerPanelWidget.h"
#include "RocketARSetupActor.h"
#include "Components/SpinBox.h"
#include "Components/CheckBox.h"

void UBannerPanelWidget::Refresh(ARocketARSetupActor* InSetupActor)
{
	if (!InSetupActor) return;
	if (Width)         Width->SetValue(InSetupActor->BannerWidth);
	if (Height)        Height->SetValue(InSetupActor->BannerHeight);
	if (RotationYaw)   RotationYaw->SetValue(InSetupActor->BannerRotationYaw);
	if (TextSize)      TextSize->SetValue(InSetupActor->BannerTextSize);
	if (SDFSharpness)  SDFSharpness->SetValue(InSetupActor->TextSDFSharpness);
	if (SlideSpeed)    SlideSpeed->SetValue(InSetupActor->SlideSpeed);
	if (SlideDuration) SlideDuration->SetValue(InSetupActor->SlideDuration);
	if (FadeIn)        FadeIn->SetValue(InSetupActor->BannerFadeInDuration);
	if (FadeOut)       FadeOut->SetValue(InSetupActor->BannerFadeOutDuration);
	if (SpawnZOffset)  SpawnZOffset->SetValue(InSetupActor->BannerSpawnZOffset);
	if (Anticipation)  Anticipation->SetValue(InSetupActor->AnticipationSeconds);
	if (MaxActive)     MaxActive->SetValue(static_cast<float>(InSetupActor->MaxActiveBanners));
	if (ShowMarkersCheck) ShowMarkersCheck->SetIsChecked(InSetupActor->bShowAltitudeMarkers);
	if (MarkerInterval)   MarkerInterval->SetValue(InSetupActor->AltitudeMarkerInterval);
}

void UBannerPanelWidget::Apply(ARocketARSetupActor* InSetupActor)
{
	if (!InSetupActor) return;
	if (Width)         InSetupActor->BannerWidth = Width->GetValue();
	if (Height)        InSetupActor->BannerHeight = Height->GetValue();
	if (RotationYaw)   InSetupActor->BannerRotationYaw = RotationYaw->GetValue();
	if (TextSize)      InSetupActor->BannerTextSize = TextSize->GetValue();
	if (SDFSharpness)  InSetupActor->TextSDFSharpness = FMath::Clamp(SDFSharpness->GetValue(), 1.0f, 200.0f);
	if (SlideSpeed)    InSetupActor->SlideSpeed = SlideSpeed->GetValue();
	if (SlideDuration) InSetupActor->SlideDuration = SlideDuration->GetValue();
	if (FadeIn)        InSetupActor->BannerFadeInDuration = FadeIn->GetValue();
	if (FadeOut)       InSetupActor->BannerFadeOutDuration = FadeOut->GetValue();
	if (SpawnZOffset)  InSetupActor->BannerSpawnZOffset = SpawnZOffset->GetValue();
	if (Anticipation)  InSetupActor->AnticipationSeconds = Anticipation->GetValue();
	if (MaxActive)     InSetupActor->MaxActiveBanners = FMath::Max(0, FMath::RoundToInt(MaxActive->GetValue()));
	if (ShowMarkersCheck) InSetupActor->bShowAltitudeMarkers = ShowMarkersCheck->IsChecked();
	if (MarkerInterval)   InSetupActor->AltitudeMarkerInterval = MarkerInterval->GetValue();
}
