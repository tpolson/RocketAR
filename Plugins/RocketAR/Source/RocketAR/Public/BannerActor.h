#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlightEventTypes.h"
#include "BannerActor.generated.h"

class UProceduralMeshComponent;
class UCanvasRenderTarget2D;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EBannerState : uint8
{
	SpawnAnimation,
	Active,
	FadeOut,
	Destroyed
};

/**
 * A single banner actor: cylindrical arc mesh with curved text.
 * Positioned at an Earth-fixed ECEF location (converted to UE each frame via Cesium).
 * Camera-facing orientation constrained to local horizontal plane.
 */
UCLASS(BlueprintType)
class ROCKETAR_API ABannerActor : public AActor
{
	GENERATED_BODY()

public:
	ABannerActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Initialize the banner with event data and geometry parameters */
	void InitBanner(
		const FFlightEventData& InEventData,
		float ArcAngleDeg,
		float ArcRadius,
		float ArcHeight,
		int32 ArcSegments,
		UMaterialInterface* BannerMaterial,
		UFont* TextFont);

	/** Start the fade-out sequence */
	UFUNCTION(BlueprintCallable, Category = "Banner")
	void StartFadeOut();

	/** Force immediate destruction */
	UFUNCTION(BlueprintCallable, Category = "Banner")
	void ForceDestroy();

	/** Get the event data this banner was spawned for */
	UFUNCTION(BlueprintCallable, Category = "Banner")
	const FFlightEventData& GetEventData() const { return EventData; }

	UFUNCTION(BlueprintCallable, Category = "Banner")
	EBannerState GetBannerState() const { return State; }

	/** Spawn time (world seconds) for age-based culling */
	double SpawnTime = 0.0;

	/** ECEF position of the banner (for Earth-fixed positioning) */
	UPROPERTY(BlueprintReadOnly, Category = "Banner")
	FVector ECEFPosition = FVector::ZeroVector;

	/** Banner lifetime in seconds (0 = infinite) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	float LifetimeSeconds = 30.0f;

	/** Fade-out duration in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	float FadeOutDuration = 1.0f;

	/** Additional rotation offset applied after camera-facing (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	FRotator BannerRotationOffset = FRotator::ZeroRotator;

protected:
	void UpdateSpawnAnimation(float DeltaTime);
	void UpdateFadeOut(float DeltaTime);
	void UpdateCameraFacing();
	void RenderTextToTarget();

	UPROPERTY()
	UProceduralMeshComponent* MeshComponent = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMaterial = nullptr;

	UPROPERTY()
	UCanvasRenderTarget2D* RenderTarget = nullptr;

	UPROPERTY()
	UFont* BannerFont = nullptr;

	FFlightEventData EventData;
	EBannerState State = EBannerState::SpawnAnimation;

	// Spawn animation state
	float SpawnAnimTime = 0.0f;
	static constexpr float SpawnOvershootDuration = 0.2f;
	static constexpr float SpawnSettleDuration = 0.1f;
	static constexpr float SpawnOvershootScale = 1.1f;

	// Fade state
	float FadeAlpha = 1.0f;
	float CurrentLifetime = 0.0f;
};
