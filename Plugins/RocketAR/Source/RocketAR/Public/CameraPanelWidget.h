#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CameraPanelWidget.generated.h"

class ARocketARSetupActor;
class USpinBox;

/**
 * Camera mount sub-panel. Designer-bound widget names:
 *   OffsetX, OffsetY, OffsetZ        : USpinBox (cm)
 *   Pitch, Yaw, Roll                 : USpinBox (degrees)
 *   OpticalRoll                      : USpinBox (degrees)
 *   HFOV                             : USpinBox (degrees, 1..180)
 *   RigIndex                         : USpinBox (int)
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class ROCKETAR_API UCameraPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Camera Panel")
	void Refresh(ARocketARSetupActor* InSetupActor);

	UFUNCTION(BlueprintCallable, Category = "Camera Panel")
	void Apply(ARocketARSetupActor* InSetupActor);

protected:
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* OffsetX = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* OffsetY = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* OffsetZ = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* Pitch = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* Yaw = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* Roll = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* OpticalRoll = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* HFOV = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) USpinBox* RigIndex = nullptr;
};
