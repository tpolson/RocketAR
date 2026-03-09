#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AltitudeMarkerActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UCanvasRenderTarget2D;

/**
 * Altitude marker: cylindrical arc mesh with altitude text.
 * Pre-placed along the rocket's predicted trajectory.
 * Separate from flight event banners — independent size, rotation, and lifecycle.
 */
UCLASS(BlueprintType)
class ROCKETAR_API AAltitudeMarkerActor : public AActor
{
	GENERATED_BODY()

public:
	AAltitudeMarkerActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Initialize the marker with altitude label and geometry */
	void InitMarker(
		const FString& Label,
		double AltitudeMeters,
		float ArcAngleDeg,
		float ArcRadius,
		float ArcHeight,
		int32 ArcSegments,
		UMaterialInterface* MarkerMaterial,
		UFont* TextFont);

	/** Start fade-out */
	UFUNCTION(BlueprintCallable, Category = "Marker")
	void StartFadeOut();

	/** Force immediate destruction */
	UFUNCTION(BlueprintCallable, Category = "Marker")
	void ForceDestroy();

	UFUNCTION(BlueprintCallable, Category = "Marker")
	bool IsFadingOut() const { return bFading; }

	/** The altitude this marker represents (meters) */
	UPROPERTY(BlueprintReadOnly, Category = "Marker")
	double MarkerAltitude = 0.0;

	/** Lifetime in seconds (0 = infinite) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker")
	float LifetimeSeconds = 0.0f;

	/** Fade-out duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker")
	float FadeOutDuration = 1.0f;

	/** Additional rotation applied to the marker (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker")
	FRotator MarkerRotationOffset = FRotator::ZeroRotator;

	double SpawnTime = 0.0;

protected:
	void RenderTextToTarget();
	void UpdateCameraFacing();

	UPROPERTY()
	UProceduralMeshComponent* MeshComponent = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMaterial = nullptr;

	UPROPERTY()
	UCanvasRenderTarget2D* RenderTarget = nullptr;

	UPROPERTY()
	UFont* MarkerFont = nullptr;

	FString DisplayLabel;
	float CurrentLifetime = 0.0f;
	bool bFading = false;
	float FadeAlpha = 1.0f;
	bool bInitialized = false;
};
