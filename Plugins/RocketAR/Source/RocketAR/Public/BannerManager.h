#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FlightEventTypes.h"
#include "BannerManager.generated.h"

class ABannerActor;
class UFlightEventDetector;

/**
 * Manages banner lifecycle: spawning, positioning, culling.
 * Listens to FlightEventDetector for new events and spawns banners at ECEF positions.
 */
UCLASS(BlueprintType, meta=(BlueprintSpawnableComponent))
class ROCKETAR_API UBannerManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UBannerManager();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Set the event detector to listen to */
	void SetEventDetector(UFlightEventDetector* Detector);

	/** Manually spawn a banner for an event */
	UFUNCTION(BlueprintCallable, Category = "Banner Manager")
	ABannerActor* SpawnBanner(const FFlightEventData& EventData);

	/** Spawn a banner at a specific UE world position */
	UFUNCTION(BlueprintCallable, Category = "Banner Manager")
	ABannerActor* SpawnBannerAtPosition(const FFlightEventData& EventData, const FVector& UEPosition);

	/** Update the cached vehicle UE position (called each telemetry frame) */
	void UpdateVehiclePosition(const FVector& UEPosition) { LastVehicleUEPosition = UEPosition; }

	/** Destroy all active banners */
	UFUNCTION(BlueprintCallable, Category = "Banner Manager")
	void DestroyAllBanners();

	/** Number of active banners */
	UFUNCTION(BlueprintCallable, Category = "Banner Manager")
	int32 GetActiveBannerCount() const { return ActiveBanners.Num(); }

	// Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	float BannerArcAngle = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	float BannerArcRadius = 10000.0f; // cm (100m)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	float BannerArcHeight = 5000.0f; // cm (50m)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	int32 BannerArcSegments = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	float BannerLifetimeSeconds = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	int32 MaxActiveBanners = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	float BannerFadeOutDuration = 1.0f;

	/** Material to use for banners (must have BannerTexture and Opacity parameters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	UMaterialInterface* BannerMaterial = nullptr;

	/** Font for banner text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	UFont* BannerFont = nullptr;

	/** Banner actor class to spawn (defaults to ABannerActor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	TSubclassOf<ABannerActor> BannerActorClass;

private:
	UFUNCTION()
	void OnFlightEventDetected(const FFlightEventData& EventData);

	void CullOldestBanner();
	void CleanupDestroyedBanners();

	UPROPERTY()
	TArray<ABannerActor*> ActiveBanners;

	UPROPERTY()
	UFlightEventDetector* EventDetector = nullptr;

	FVector LastVehicleUEPosition = FVector::ZeroVector;
};
