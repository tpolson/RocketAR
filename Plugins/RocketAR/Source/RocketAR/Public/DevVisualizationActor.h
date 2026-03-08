#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TelemetryTypes.h"
#include "DevVisualizationActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Development visualization: Earth sphere + rocket cylinder.
 * Earth is real scale, centered at ECEF origin (below the UE origin by Earth radius).
 * Rocket cylinder tracks vehicle position each frame.
 * Visible in viewport for dev; excluded from broadcast output via toggle.
 */
UCLASS(BlueprintType)
class ROCKETAR_API ADevVisualizationActor : public AActor
{
	GENERATED_BODY()

public:
	ADevVisualizationActor();

	virtual void BeginPlay() override;

	/** Update rocket position from telemetry */
	UFUNCTION(BlueprintCallable, Category = "Dev")
	void UpdateFromTelemetry(const FProcessedTelemetryData& Data);

	/** Show or hide all dev visualization */
	UFUNCTION(BlueprintCallable, Category = "Dev")
	void SetVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "Dev")
	bool IsVisible() const { return bIsVisible; }

private:
	UPROPERTY()
	UStaticMeshComponent* EarthMesh = nullptr;

	UPROPERTY()
	UStaticMeshComponent* RocketMesh = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* EarthMaterial = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* RocketMaterial = nullptr;

	bool bIsVisible = true;
};
