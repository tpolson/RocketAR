#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlightEventTypes.h"
#include "BannerActor.generated.h"

class UStaticMeshComponent;
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
 * Banner actor: flat disk (squished cylinder) parented to the rocket.
 * Slides along the rocket's local -Z axis (toward exhaust).
 */
UCLASS(BlueprintType)
class ROCKETAR_API ABannerActor : public AActor
{
	GENERATED_BODY()

public:
	ABannerActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Initialize the banner with event data and disk scale */
	void InitBanner(
		const FFlightEventData& InEventData,
		float DiskRadius,
		float DiskThickness,
		UMaterialInterface* BannerMaterial,
		UFont* TextFont);

	/** Initialize slide motion (call after InitBanner) */
	void InitSlide(float InSlideSpeed);

	/** Override the disk color (call after InitBanner) */
	void SetDiskColor(const FLinearColor& Color);

	/** Start the fade-out sequence */
	UFUNCTION(BlueprintCallable, Category = "Banner")
	void StartFadeOut();

	/** Force immediate destruction */
	UFUNCTION(BlueprintCallable, Category = "Banner")
	void ForceDestroy();

	UFUNCTION(BlueprintCallable, Category = "Banner")
	const FFlightEventData& GetEventData() const { return EventData; }

	UFUNCTION(BlueprintCallable, Category = "Banner")
	EBannerState GetBannerState() const { return State; }

	double SpawnTime = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	float LifetimeSeconds = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	float FadeInDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	float FadeOutDuration = 1.0f;

protected:
	void UpdateSpawnAnimation(float DeltaTime);
	void UpdateFadeOut(float DeltaTime);
	void RenderTextToTarget();

	UPROPERTY()
	UStaticMeshComponent* DiskMesh = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMaterial = nullptr;

	UPROPERTY()
	UCanvasRenderTarget2D* RenderTarget = nullptr;

	UPROPERTY()
	UFont* BannerFont = nullptr;

	FFlightEventData EventData;
	EBannerState State = EBannerState::SpawnAnimation;

	float SlideSpeed = 0.0f;
	float SpawnAnimTime = 0.0f;
	float FadeAlpha = 1.0f;
	float CurrentLifetime = 0.0f;
};
