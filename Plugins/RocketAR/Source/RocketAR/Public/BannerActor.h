#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlightEventTypes.h"
#include "BannerActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;
class UTexture2D;

UENUM(BlueprintType)
enum class EBannerState : uint8
{
	SpawnAnimation,
	Active,
	FadeOut,
	Destroyed
};

/**
 * Banner actor: flat plane with translucent text material.
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

	/** Pre-compile banner materials so first spawn doesn't show a checkerboard flash */
	static void WarmUpMaterials();

	/** Initialize the banner with event data and dimensions */
	void InitBanner(
		const FFlightEventData& InEventData,
		float InWidth,
		float InHeight,
		bool bUseOpaqueMaterial = false,
		FColor InWireframeColor = FColor(255, 255, 0),
		float InRotationYaw = 0.0f);

	/** Initialize slide motion (call after InitBanner) */
	void InitSlide(float InSlideSpeed);

	/** Override the banner color tint (call after InitBanner) */
	void SetBannerColor(const FLinearColor& Color);

	/** Apply a background texture (PNG with alpha). Call after InitBanner. nullptr = solid color. */
	void SetBannerTexture(UTexture2D* InTexture);

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

	// Text configuration (set by BannerManager before InitBanner or at runtime)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Text")
	FVector TextOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Text")
	float TextWorldSize = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Text")
	FColor TextColor = FColor::White;

protected:
	void UpdateSpawnAnimation(float DeltaTime);
	void UpdateFadeOut(float DeltaTime);

	UPROPERTY()
	USceneComponent* RootScene = nullptr;

	UPROPERTY()
	UStaticMeshComponent* BannerMesh = nullptr;

	UPROPERTY()
	UTextRenderComponent* TextComponent = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMaterial = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* TextDynamicMaterial = nullptr;

	UPROPERTY()
	UTexture2D* BannerTexture = nullptr;

	FFlightEventData EventData;
	EBannerState State = EBannerState::SpawnAnimation;

	float SlideSpeed = 0.0f;
	float SpawnAnimTime = 0.0f;
	float FadeAlpha = 1.0f;
	float CurrentLifetime = 0.0f;
};
