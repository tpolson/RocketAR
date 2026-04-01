#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TelemetryTypes.h"
#include "DevVisualizationActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class URocketDefinition;

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

	/**
	 * Apply a rocket definition: swaps to the definition's mesh (or restores cylinder fallback),
	 * updates Height/Radius from the definition, and applies production visibility.
	 * Call from SetupActor::BeginPlay after this actor is spawned.
	 */
	void ApplyRocketDefinition(URocketDefinition* Definition);

	/**
	 * Control whether the rocket mesh writes into the broadcast SDI output.
	 * true  = opaque unlit material → mesh visible in fill + key channels.
	 * false = additive material → visible in viewport, invisible in broadcast key.
	 */
	UFUNCTION(BlueprintCallable, Category = "Production")
	void SetProductionVisible(bool bVisible);

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

	/** Current rocket orientation (from telemetry quaternion) */
	FQuat RocketOrientation = FQuat::Identity;

	bool bIsVisible = true;
	bool bProductionVisible = false;

	UPROPERTY()
	URocketDefinition* ActiveDefinition = nullptr;

	/** Hard-loaded mesh ref from the definition's TSoftObjectPtr (set by ApplyRocketDefinition) */
	UPROPERTY()
	UStaticMesh* LoadedRocketMesh = nullptr;
};
