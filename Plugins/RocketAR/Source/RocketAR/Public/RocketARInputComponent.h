#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RocketARInputComponent.generated.h"

class ARocketARSetupActor;
class ACSVTelemetryProvider;

/**
 * Keyboard input bindings for RocketAR operator controls.
 * Space=pause, arrows=scrub, []=timescale, R=reset, D=dev toggle, F1=HUD, F2=stats, F4=DeckLink, Tab=banner cycle.
 */
UCLASS(BlueprintType, meta=(BlueprintSpawnableComponent))
class ROCKETAR_API URocketARInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URocketARInputComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Bind inputs. Call after setup actor is ready. */
	void SetupBindings(ARocketARSetupActor* InSetupActor);

private:
	UFUNCTION()
	void OnTogglePlayPause();

	UFUNCTION()
	void OnScrubForward();

	UFUNCTION()
	void OnScrubBackward();

	UFUNCTION()
	void OnTimeScaleUp();

	UFUNCTION()
	void OnTimeScaleDown();

	UFUNCTION()
	void OnReset();

	UFUNCTION()
	void OnToggleDevVis();

	UFUNCTION()
	void OnToggleHUD();

	UFUNCTION()
	void OnToggleStats();

	UFUNCTION()
	void OnCycleBanner();

	UFUNCTION()
	void OnToggleDeckLink();

	UPROPERTY()
	ARocketARSetupActor* SetupActor = nullptr;

	bool bBindingsReady = false;
	int32 CurrentBannerIndex = 0;
};
