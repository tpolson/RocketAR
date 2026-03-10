#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FlightEventTypes.h"
#include "BannerManager.generated.h"

class ABannerActor;
class UFlightEventDetector;

/** Pending banner waiting for trigger time offset to elapse */
USTRUCT()
struct FPendingBanner
{
	GENERATED_BODY()

	FFlightEventData EventData;
	FVector TrajectoryAtTrigger = FVector::ZeroVector;
	FVector SpawnPosition = FVector::ZeroVector;
	double TriggerWorldTime = 0.0;
};

/**
 * Manages banner lifecycle: spawning, slide motion, culling.
 * Banners spawn at the rocket's position and slide away along the trajectory vector.
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

	/** Manually spawn a banner for an event (uses cached position/velocity) */
	UFUNCTION(BlueprintCallable, Category = "Banner Manager")
	ABannerActor* SpawnBanner(const FFlightEventData& EventData);

	/** Queue a banner for deferred spawn (respects TriggerTimeOffset) */
	void QueueBanner(const FFlightEventData& EventData, const FVector& TrajectoryAtTrigger);

	/** Update the cached vehicle UE position (called each telemetry frame) */
	void UpdateVehiclePosition(const FVector& UEPosition) { LastVehicleUEPosition = UEPosition; }

	/** Update the cached vehicle UE velocity (called each telemetry frame) */
	void UpdateVehicleVelocity(const FVector& UEVelocity) { LastVehicleUEVelocity = UEVelocity; }

	/** Set the component banners will be attached to (rocket mount point) */
	void SetAttachTarget(USceneComponent* InTarget) { AttachTarget = InTarget; }

	/** Destroy all active banners */
	UFUNCTION(BlueprintCallable, Category = "Banner Manager")
	void DestroyAllBanners();

	/** Number of active banners */
	UFUNCTION(BlueprintCallable, Category = "Banner Manager")
	int32 GetActiveBannerCount() const { return ActiveBanners.Num(); }


	// Geometry configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	float BannerDiskRadius = 5000.0f; // cm (50m)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	float BannerDiskThickness = 100.0f; // cm (1m)

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

	// Altitude marker geometry (distinct from event banners)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Marker")
	float MarkerDiskRadius = 2000.0f; // cm (20m)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Marker")
	float MarkerDiskThickness = 50.0f; // cm (0.5m)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Marker")
	FLinearColor MarkerColor = FLinearColor(0.2f, 0.8f, 1.0f, 1.0f); // cyan

	// Slide configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Slide")
	float TriggerTimeOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Slide")
	float SlideSpeed = 5000.0f; // cm/s (50 m/s)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Slide")
	float SlideDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Slide")
	float FadeInDuration = 0.3f;

	/** Whether to show on-screen debug messages when banners spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Debug")
	bool bShowDebugMessages = true;

private:
	UFUNCTION()
	void OnFlightEventDetected(const FFlightEventData& EventData);

	ABannerActor* SpawnBannerFromQueue(const FPendingBanner& Pending);
	void CullOldestBanner();
	void CleanupDestroyedBanners();

	UPROPERTY()
	TArray<ABannerActor*> ActiveBanners;

	UPROPERTY()
	UFlightEventDetector* EventDetector = nullptr;

	TArray<FPendingBanner> PendingBanners;
	FVector LastVehicleUEPosition = FVector::ZeroVector;

	UPROPERTY()
	USceneComponent* AttachTarget = nullptr;
	FVector LastVehicleUEVelocity = FVector::ZeroVector;
};
