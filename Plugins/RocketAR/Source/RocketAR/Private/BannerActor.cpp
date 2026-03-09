#include "BannerActor.h"
#include "ProceduralArcMesh.h"
#include "RocketARModule.h"
#include "ProceduralMeshComponent.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"

ABannerActor::ABannerActor()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BannerMesh"));
	MeshComponent->bCastDynamicShadow = false;
	MeshComponent->CastShadow = false;
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCullDistance(0); // Never distance-cull
	MeshComponent->bUseAsOccluder = false;
	RootComponent = MeshComponent;
}

void ABannerActor::BeginPlay()
{
	Super::BeginPlay();
	SpawnTime = GetWorld()->GetTimeSeconds();
}

void ABannerActor::InitBanner(
	const FFlightEventData& InEventData,
	float ArcAngleDeg,
	float ArcRadius,
	float ArcHeight,
	int32 ArcSegments,
	UMaterialInterface* BannerMaterial,
	UFont* TextFont)
{
	EventData = InEventData;
	ECEFPosition = InEventData.ECEFPosition;
	BannerFont = TextFont;

	// Generate arc mesh
	TArray<FVector> Vertices;
	TArray<FVector2D> UVs;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;

	UProceduralArcMesh::GenerateArcMesh(
		ArcAngleDeg, ArcRadius, ArcHeight, ArcSegments,
		Vertices, UVs, Triangles, Normals, Tangents);

	MeshComponent->CreateMeshSection(0, Vertices, Triangles, Normals,
		UVs, TArray<FColor>(), Tangents, false);

	// Create render target for text
	RenderTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(
		this, UCanvasRenderTarget2D::StaticClass(), 1024, 256);

	if (RenderTarget)
	{
		RenderTarget->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		RenderTextToTarget();
	}

	// Always use bright opaque material for now (debug visibility)
	{
		UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (BaseMat)
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (DynamicMaterial)
			{
				DynamicMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 1.0f, 0.0f, 1.0f));
				MeshComponent->SetMaterial(0, DynamicMaterial);
			}
		}
		UE_LOG(LogRocketAR, Log, TEXT("Banner: using bright yellow debug material"));
	}

	// Skip spawn animation for now — start fully visible for debugging
	State = EBannerState::Active;
	SetActorScale3D(FVector::OneVector);

	UE_LOG(LogRocketAR, Log, TEXT("Banner initialized: '%s' at (%.0f, %.0f, %.0f)"),
		*EventData.EventLabel, GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
}

void ABannerActor::RenderTextToTarget()
{
	if (!RenderTarget) return;

	RenderTarget->UpdateResource();

	UCanvas* Canvas = nullptr;
	FVector2D Size;
	FDrawToRenderTargetContext Context;

	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, RenderTarget, Canvas, Size, Context);

	if (Canvas && BannerFont)
	{
		const FString& Text = EventData.EventLabel;
		const float FontScale = 2.5f;

		// Approximate text dimensions for centering
		const float TextWidth = Text.Len() * 20.0f * FontScale;
		const float TextHeight = 32.0f * FontScale;
		const float X = (Size.X - TextWidth) * 0.5f;
		const float Y = (Size.Y - TextHeight) * 0.5f;

		// Draw text using FCanvasTextItem for reliable cross-version compatibility
		FCanvasTextItem TextItem(FVector2D(X, Y), FText::FromString(Text), BannerFont, FLinearColor::White);
		TextItem.Scale = FVector2D(FontScale, FontScale);
		TextItem.bCentreX = false;
		TextItem.bCentreY = false;
		TextItem.bOutlined = true;
		TextItem.OutlineColor = FLinearColor(0, 0, 0, 0.5f);
		Canvas->DrawItem(TextItem);
	}

	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
}

void ABannerActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	switch (State)
	{
	case EBannerState::SpawnAnimation:
		UpdateSpawnAnimation(DeltaTime);
		break;

	case EBannerState::Active:
		CurrentLifetime += DeltaTime;
		if (LifetimeSeconds > 0.0f && CurrentLifetime >= LifetimeSeconds)
		{
			StartFadeOut();
		}
		break;

	case EBannerState::FadeOut:
		UpdateFadeOut(DeltaTime);
		break;

	case EBannerState::Destroyed:
		Destroy();
		return;
	}

	// Rotation set once at spawn via SetTrajectoryRotation — no per-frame update
}

void ABannerActor::UpdateSpawnAnimation(float DeltaTime)
{
	SpawnAnimTime += DeltaTime;

	float Scale;
	const float TotalDuration = SpawnOvershootDuration + SpawnSettleDuration;

	if (SpawnAnimTime < SpawnOvershootDuration)
	{
		// Phase 1: Scale from 0 to overshoot (1.1)
		const float T = SpawnAnimTime / SpawnOvershootDuration;
		// Ease-out quadratic
		Scale = SpawnOvershootScale * (1.0f - (1.0f - T) * (1.0f - T));
	}
	else if (SpawnAnimTime < TotalDuration)
	{
		// Phase 2: Settle from overshoot to 1.0
		const float T = (SpawnAnimTime - SpawnOvershootDuration) / SpawnSettleDuration;
		Scale = FMath::Lerp(SpawnOvershootScale, 1.0f, T);
	}
	else
	{
		Scale = 1.0f;
		State = EBannerState::Active;
	}

	SetActorScale3D(FVector(Scale));

	// Also ramp opacity during spawn
	if (DynamicMaterial)
	{
		const float OpacityT = FMath::Clamp(SpawnAnimTime / SpawnOvershootDuration, 0.0f, 1.0f);
		DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), OpacityT);
	}
}

void ABannerActor::UpdateFadeOut(float DeltaTime)
{
	if (FadeOutDuration <= 0.0f)
	{
		State = EBannerState::Destroyed;
		return;
	}

	FadeAlpha -= DeltaTime / FadeOutDuration;
	FadeAlpha = FMath::Max(FadeAlpha, 0.0f);

	if (DynamicMaterial)
	{
		DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), FadeAlpha);
	}

	if (FadeAlpha <= 0.0f)
	{
		State = EBannerState::Destroyed;
	}
}

void ABannerActor::SetTrajectoryRotation(const FVector& Trajectory)
{
	if (Trajectory.IsNearlyZero()) return;

	const FRotator TrajRot = Trajectory.Rotation();
	FRotator FinalRot(0.0f, TrajRot.Yaw - 90.0f, 0.0f);
	FinalRot += BannerRotationOffset;
	SetActorRotation(FinalRot);
}

void ABannerActor::StartFadeOut()
{
	if (State == EBannerState::FadeOut || State == EBannerState::Destroyed) return;
	State = EBannerState::FadeOut;
	FadeAlpha = 1.0f;
}

void ABannerActor::ForceDestroy()
{
	State = EBannerState::Destroyed;
	Destroy();
}
