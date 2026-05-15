#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RocketARMediaOutput.h"
#include "DeckLinkPanelWidget.generated.h"

class ARocketARSetupActor;
class UCheckBox;
class UComboBoxString;
class UTextBlock;

/**
 * DeckLink configuration sub-panel. Designer-bound widget names:
 *   EnableCheck, FillAndKeyCheck, AutoStartCheck, GenlockCheck, TimecodeCheck : UCheckBox
 *   ResolutionCombo : UComboBoxString (options populated by C++ to match ERocketAROutputResolution)
 *   StatusText      : UTextBlock (live capture state)
 *
 * Reads from / writes to ARocketARSetupActor when the parent console calls
 * RefreshFromSetupActor() / ApplyToSetupActor().
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class ROCKETAR_API UDeckLinkPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

	/** Pull values from setup actor into this panel's UI controls. */
	UFUNCTION(BlueprintCallable, Category = "DeckLink Panel")
	void Refresh(ARocketARSetupActor* InSetupActor);

	/** Write this panel's UI control values back to the setup actor. */
	UFUNCTION(BlueprintCallable, Category = "DeckLink Panel")
	void Apply(ARocketARSetupActor* InSetupActor);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* EnableCheck = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* FillAndKeyCheck = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* AutoStartCheck = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* GenlockCheck = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* TimecodeCheck = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UComboBoxString* ResolutionCombo = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StatusText = nullptr;

	UPROPERTY()
	ARocketARSetupActor* SetupActor = nullptr;

	static ERocketAROutputResolution ResolutionFromString(const FString& Str);
	static FString ResolutionToString(ERocketAROutputResolution Res);
};
