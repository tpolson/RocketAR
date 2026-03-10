#include "BannerActor.h"
#include "RocketARModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/Font.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABannerActor::ABannerActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Same cylinder mesh the rocket and old event disks use
	DiskMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DiskMesh"));
	DiskMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DiskMesh->CastShadow = false;
	DiskMesh->bCastDynamicShadow = false;
	RootComponent = DiskMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		DiskMesh->SetStaticMesh(CylinderFinder.Object);
	}
}

void ABannerActor::BeginPlay()
{
	Super::BeginPlay();
	SpawnTime = GetWorld()->GetTimeSeconds();
}

void ABannerActor::InitSlide(float InSlideSpeed)
{
	SlideSpeed = InSlideSpeed;
}

void ABannerActor::SetDiskColor(const FLinearColor& Color)
{
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void ABannerActor::InitBanner(
	const FFlightEventData& InEventData,
	float DiskRadius,
	float DiskThickness,
	UMaterialInterface* BannerMaterial,
	UFont* TextFont)
{
	EventData = InEventData;
	BannerFont = TextFont;

	// Default cylinder is 100cm diameter (50cm radius), 100cm tall.
	// Scale to desired disk dimensions.
	const float RadiusScale = DiskRadius / 50.0f;
	const float HeightScale = DiskThickness / 100.0f;
	DiskMesh->SetRelativeScale3D(FVector(RadiusScale, RadiusScale, HeightScale));

	// Create render target for text
	RenderTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(
		this, UCanvasRenderTarget2D::StaticClass(), 1024, 256);

	if (RenderTarget)
	{
		RenderTarget->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		RenderTextToTarget();
	}

	// Bright debug material (same as rocket/disks)
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BaseMat)
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
		if (DynamicMaterial)
		{
			DynamicMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 1.0f, 0.0f, 1.0f));
			DiskMesh->SetMaterial(0, DynamicMaterial);
		}
	}

	// Start in spawn animation state (opacity fade-in)
	State = EBannerState::SpawnAnimation;
	if (DynamicMaterial)
	{
		DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
	}

	UE_LOG(LogRocketAR, Log, TEXT("Banner initialized: '%s' radius=%.0f thickness=%.0f"),
		*EventData.EventLabel, DiskRadius, DiskThickness);
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
		const float TextWidth = Text.Len() * 20.0f * FontScale;
		const float TextHeight = 32.0f * FontScale;
		const float X = (Size.X - TextWidth) * 0.5f;
		const float Y = (Size.Y - TextHeight) * 0.5f;

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

	// Slide along local -Z (toward exhaust). Same axis as the rocket cylinder height.
	if (State != EBannerState::Destroyed && SlideSpeed > 0.0f)
	{
		AddActorLocalOffset(FVector(0.0f, 0.0f, -SlideSpeed * DeltaTime));
	}

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
}

void ABannerActor::UpdateSpawnAnimation(float DeltaTime)
{
	SpawnAnimTime += DeltaTime;

	const float Duration = FMath::Max(FadeInDuration, 0.01f);
	const float Alpha = FMath::Clamp(SpawnAnimTime / Duration, 0.0f, 1.0f);

	if (DynamicMaterial)
	{
		DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), Alpha);
	}

	if (SpawnAnimTime >= Duration)
	{
		State = EBannerState::Active;
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
