#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BannerPanelWidget.generated.h"

class ARocketARSetupActor;
class USpinBox;
class UCheckBox;

/**
 * Banner geometry / motion sub-panel. Designer-bound widget names:
 *   Width, Height, RotationYaw, TextSize, SDFSharpness : USpinBox
 *   SlideSpeed, SlideDuration, FadeIn, FadeOut         : USpinBox
 *   SpawnZOffset, Anticipation, MaxActive              : USpinBox
 *   ShowMarkersCheck                                   : UCheckBox
 *   MarkerInterval                                     : USpinBox
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class ROCKETAR_API UBannerPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Banner Panel")
	void Refresh(ARocketARSetupActor* InSetupActor);

	UFUNCTION(BlueprintCallable, Category = "Banner Panel")
	void Apply(ARocketARSetupActor* InSetupActor);

protected:
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* Width = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* Height = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* RotationYaw = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* TextSize = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* SDFSharpness = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* SlideSpeed = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* SlideDuration = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* FadeIn = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* FadeOut = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* SpawnZOffset = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* Anticipation = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* MaxActive = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UCheckBox* ShowMarkersCheck = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* MarkerInterval = nullptr;
};
