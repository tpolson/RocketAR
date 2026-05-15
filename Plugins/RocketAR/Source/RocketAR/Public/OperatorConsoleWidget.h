#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OperatorConsoleWidget.generated.h"

class ARocketARSetupActor;
class UButton;
class UTextBlock;
class UDeckLinkPanelWidget;
class UCameraPanelWidget;
class UBannerPanelWidget;

/**
 * Root operator console widget. Designers subclass this in Blueprint
 * (WBP_OperatorConsole), lay out sub-panels by name, and the C++ base
 * wires button clicks to setup-actor Apply/Revert/Save operations.
 *
 * Expected child widget names (meta=(BindWidget)):
 *   ApplyButton, RevertButton, SaveButton            : UButton
 *   StatusText                                       : UTextBlock (optional)
 *   DeckLinkPanel, CameraPanel, BannerPanel          : sub-panel widgets (optional)
 *
 * Toggle visibility via ARocketARSetupActor::ToggleOperatorConsole (F12).
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class ROCKETAR_API UOperatorConsoleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	/** Wire the console to the setup actor it edits. */
	UFUNCTION(BlueprintCallable, Category = "Operator Console")
	void SetSetupActor(ARocketARSetupActor* InSetupActor);

	UFUNCTION(BlueprintCallable, Category = "Operator Console")
	ARocketARSetupActor* GetSetupActor() const { return SetupActor; }

	/** Pull current setup-actor values into all sub-panels. */
	UFUNCTION(BlueprintCallable, Category = "Operator Console")
	void RefreshFromSetupActor();

	/** Push all sub-panel values back to the setup actor (live, without persisting). */
	UFUNCTION(BlueprintCallable, Category = "Operator Console")
	void ApplyToSetupActor();

	/** Apply, then persist to the default save slot. */
	UFUNCTION(BlueprintCallable, Category = "Operator Console")
	bool SaveSettings();

protected:
	UFUNCTION()
	void OnApplyClicked();

	UFUNCTION()
	void OnRevertClicked();

	UFUNCTION()
	void OnSaveClicked();

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* ApplyButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* RevertButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* SaveButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StatusText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UDeckLinkPanelWidget* DeckLinkPanel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UCameraPanelWidget* CameraPanel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UBannerPanelWidget* BannerPanel = nullptr;

	UPROPERTY()
	ARocketARSetupActor* SetupActor = nullptr;

	void SetStatus(const FString& Message);
};
