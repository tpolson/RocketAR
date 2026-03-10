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

	/** Set the Earth center and pole direction in UE space (from ECEF via Cesium) */
	void SetEarthTransform(const FVector& CenterUE, const FVector& NorthPoleDirectionUE);

	UFUNCTION(BlueprintCallable, Category = "Dev")
	bool IsVisible() const { return bIsVisible; }

	/** Get the unscaled mount point at rocket base (for camera attachment) */
	USceneComponent* GetRocketMountPoint() const { return RocketMountPoint; }

	/** Rocket body height in meters. SLS Block 1 = 98m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dev Visualization")
	float RocketHeight = 98.0f;

	/** Rocket body radius in meters. SLS Block 1 core stage = 4.2m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dev Visualization")
	float RocketRadius = 4.2f;

	/** Apply current height/radius to mesh scale and offset */
	void UpdateRocketDimensions();

private:
	UPROPERTY()
	UStaticMeshComponent* EarthMesh = nullptr;

	UPROPERTY()
	UStaticMeshComponent* RocketMesh = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* EarthMaterial = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* RocketMaterial = nullptr;

	/** Unscaled component at rocket base position+rotation — camera attaches here */
	UPROPERTY()
	USceneComponent* RocketMountPoint = nullptr;

	/** Previous UE position for velocity-based rocket orientation */
	FVector PrevUEPosition = FVector::ZeroVector;
	bool bHasPrevPosition = false;

	/** Current rocket orientation (aligned to velocity) */
	FQuat RocketOrientation = FQuat::Identity;

	bool bIsVisible = true;
};
