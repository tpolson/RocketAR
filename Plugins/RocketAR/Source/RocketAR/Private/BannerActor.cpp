#include "BannerActor.h"
#include "RocketARModule.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "UObject/ConstructorHelpers.h"

/** Create a shared translucent material with Color and Opacity parameters (no texture) */
static UMaterial* GetBannerBaseMaterial()
{
	static TWeakObjectPtr<UMaterial> CachedMat;
	if (CachedMat.IsValid()) return CachedMat.Get();

	UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(), TEXT("BannerTranslucentMat"), RF_Transient);
	Mat->BlendMode = BLEND_Translucent;
	Mat->TwoSided = true;

	// Color tint
	auto* ColorParam = NewObject<UMaterialExpressionVectorParameter>(Mat);
	ColorParam->ParameterName = TEXT("Color");
	ColorParam->DefaultValue = FLinearColor::White;
	Mat->GetExpressionCollection().AddExpression(ColorParam);

	// Fade opacity scalar
	auto* OpacityParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
	OpacityParam->ParameterName = TEXT("Opacity");
	OpacityParam->DefaultValue = 1.0f;
	Mat->GetExpressionCollection().AddExpression(OpacityParam);

#if WITH_EDITOR
	auto* Ed = Mat->GetEditorOnlyData();
	// BaseColor = Color
	Ed->BaseColor.Connect(0, ColorParam);
	// Opacity = OpacityParam
	Ed->Opacity.Connect(0, OpacityParam);
#endif

	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	CachedMat = Mat;
	return Mat;
}

ABannerActor::ABannerActor()
{
	PrimaryActorTick.bCanEverTick = true;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = RootScene;

	BannerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BannerMesh"));
	BannerMesh->SetupAttachment(RootScene);
	BannerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BannerMesh->CastShadow = false;
	BannerMesh->bCastDynamicShadow = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneFinder.Succeeded())
	{
		BannerMesh->SetStaticMesh(PlaneFinder.Object);
	}

	TextComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BannerText"));
	TextComponent->SetupAttachment(RootScene);
	TextComponent->SetHorizontalAlignment(EHTA_Center);
	TextComponent->SetVerticalAlignment(EVRTA_TextCenter);
	TextComponent->SetWorldSize(200.0f);
	TextComponent->SetTextRenderColor(FColor::White);
	TextComponent->CastShadow = false;
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

void ABannerActor::SetBannerColor(const FLinearColor& Color)
{
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void ABannerActor::InitBanner(
	const FFlightEventData& InEventData,
	float InWidth,
	float InHeight,
	bool bUseOpaqueMaterial,
	FColor InWireframeColor)
{
	EventData = InEventData;

	// UE Plane is 100x100cm. Scale to desired width x height.
	const float ScaleX = InWidth / 100.0f;
	const float ScaleY = InHeight / 100.0f;
	BannerMesh->SetRelativeScale3D(FVector(ScaleX, ScaleY, 1.0f));

	if (bUseOpaqueMaterial)
	{
		// Dev opaque mode: use engine BasicShapeMaterial (yellow, no alpha/fade)
		UMaterial* OpaqueMat = LoadObject<UMaterial>(nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (OpaqueMat)
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(OpaqueMat, this);
			if (DynamicMaterial)
			{
				DynamicMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 1.0f, 0.0f, 1.0f));
				BannerMesh->SetMaterial(0, DynamicMaterial);
			}
		}
	}
	else
	{
		// Translucent colored material (no texture — text is a separate component)
		UMaterial* BaseMat = GetBannerBaseMaterial();
		if (BaseMat)
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (DynamicMaterial)
			{
				DynamicMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 1.0f, 0.0f, 1.0f));
				DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
				BannerMesh->SetMaterial(0, DynamicMaterial);
			}
		}
	}

	// Configure text component
	if (TextComponent)
	{
		TextComponent->SetText(FText::FromString(EventData.EventLabel));
		TextComponent->SetWorldSize(TextWorldSize);
		TextComponent->SetTextRenderColor(TextColor);
		TextComponent->SetRelativeLocation(TextOffset);
		// Rotate text to lie in the XY plane facing -Z (toward camera behind rocket)
		TextComponent->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	}

	// Set wireframe color AFTER material setup so SetMaterial doesn't reset it
	BannerMesh->bOverrideWireframeColor = true;
	BannerMesh->WireframeColorOverride = InWireframeColor;

	// Start in spawn animation state (opacity fade-in)
	State = EBannerState::SpawnAnimation;

	UE_LOG(LogRocketAR, Log, TEXT("Banner initialized: '%s' width=%.0f height=%.0f textSize=%.0f"),
		*EventData.EventLabel, InWidth, InHeight, TextWorldSize);
}

void ABannerActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Slide along local -Z (toward exhaust)
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

	// Fade text alpha in sync with the banner material
	if (TextComponent)
	{
		FColor C = TextColor;
		C.A = static_cast<uint8>(Alpha * 255.0f);
		TextComponent->SetTextRenderColor(C);
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

	// Fade text alpha in sync
	if (TextComponent)
	{
		FColor C = TextColor;
		C.A = static_cast<uint8>(FadeAlpha * 255.0f);
		TextComponent->SetTextRenderColor(C);
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
