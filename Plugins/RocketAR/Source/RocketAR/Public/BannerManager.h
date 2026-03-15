#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FlightEventTypes.h"
#include "BannerManager.generated.h"

class ABannerActor;
class UFlightEventDetector;
class UTexture2D;

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
	float BannerWidth = 10000.0f; // cm (100m)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	float BannerHeight = 10000.0f; // cm (100m)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	int32 MaxActiveBanners = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	float BannerFadeOutDuration = 1.0f;

	/** Banner actor class to spawn (defaults to ABannerActor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	TSubclassOf<ABannerActor> BannerActorClass;

	// Text configuration for event banners
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Text")
	float BannerTextSize = 200.0f; // cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Text")
	FVector BannerTextOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Text")
	float TextSDFSharpness = 50.0f;

	// Text configuration for altitude markers
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Text")
	float MarkerTextSize = 150.0f; // cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Text")
	FVector MarkerTextOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Text")
	float MarkerSDFSharpness = 50.0f;

	// Altitude marker geometry (distinct from event banners)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Marker")
	float MarkerWidth = 4000.0f; // cm (40m)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Marker")
	float MarkerHeight = 4000.0f; // cm (40m)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Marker")
	FLinearColor MarkerColor = FLinearColor(0.2f, 0.8f, 1.0f, 1.0f); // cyan

	/** Z-axis rotation (yaw) for event banners in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config")
	float BannerRotationYaw = 0.0f;

	/** Z-axis rotation (yaw) for altitude markers in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Marker")
	float MarkerRotationYaw = 0.0f;

	// Slide configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Slide")
	float TriggerTimeOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Slide")
	float SlideSpeed = 5000.0f; // cm/s (50 m/s)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Slide")
	float SlideDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Slide")
	float FadeInDuration = 0.3f;

	/** Local Z offset above vehicle center for event banners (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Slide")
	float BannerSpawnZOffset = 8000.0f;

	/** Local Z offset above vehicle center for altitude markers (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Slide")
	float MarkerSpawnZOffset = 6000.0f;

	/** Seconds before trigger time to begin spawn (anticipation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Slide")
	float AnticipationSeconds = 1.5f;

	/** Background image for event banners (PNG with alpha). nullptr = solid color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Image")
	UTexture2D* BannerImage = nullptr;

	/** Background image for altitude markers (PNG with alpha). nullptr = solid color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Image")
	UTexture2D* MarkerImage = nullptr;

	/** Use opaque banner material for dev wireframe visibility (no alpha/fade) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner Config|Debug")
	bool bDevOpaqueBanners = false;

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
