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
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionFontSampleParameter.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "UObject/ConstructorHelpers.h"

/** Create a shared translucent material with Color, Opacity, and BannerTexture parameters.
 *  When no texture is bound, BannerTexture defaults to white (1,1,1,1), preserving solid-color behavior.
 *  BaseColor = TexRGB * Color, Opacity = TexAlpha * Opacity scalar. */
static UMaterial* GetBannerTranslucentMaterial()
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

	// Texture parameter — explicit white default so unbound texture gives (1,1,1,1)
	auto* TexParam = NewObject<UMaterialExpressionTextureSampleParameter2D>(Mat);
	TexParam->ParameterName = TEXT("BannerTexture");
	TexParam->Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture"));
	Mat->GetExpressionCollection().AddExpression(TexParam);

	// Multiply: TexRGB * Color → BaseColor
	auto* MulColor = NewObject<UMaterialExpressionMultiply>(Mat);
	Mat->GetExpressionCollection().AddExpression(MulColor);

	// Multiply: TexAlpha * Opacity → Opacity
	auto* MulOpacity = NewObject<UMaterialExpressionMultiply>(Mat);
	Mat->GetExpressionCollection().AddExpression(MulOpacity);

#if WITH_EDITOR
	// Wire: MulColor.A = TexParam (RGB), MulColor.B = ColorParam
	MulColor->A.Connect(0, TexParam);   // output 0 = RGB
	MulColor->B.Connect(0, ColorParam);

	// Wire: MulOpacity.A = TexParam (Alpha = output 4), MulOpacity.B = OpacityParam
	MulOpacity->A.Connect(4, TexParam); // output 4 = Alpha
	MulOpacity->B.Connect(0, OpacityParam);

	auto* Ed = Mat->GetEditorOnlyData();
	Ed->BaseColor.Connect(0, MulColor);
	Ed->Opacity.Connect(0, MulOpacity);
#endif

	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	CachedMat = Mat;
	return Mat;
}

/** Create a shared masked material with Color and BannerTexture parameters for dev visualization.
 *  BaseColor = TexRGB * Color. OpacityMask = TexAlpha (clips pixels below threshold). */
static UMaterial* GetBannerOpaqueMaterial()
{
	static TWeakObjectPtr<UMaterial> CachedMat;
	if (CachedMat.IsValid()) return CachedMat.Get();

	UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(), TEXT("BannerOpaqueMat"), RF_Transient);
	Mat->BlendMode = BLEND_Masked;
	Mat->TwoSided = true;

	// Color tint (yellow default for dev)
	auto* ColorParam = NewObject<UMaterialExpressionVectorParameter>(Mat);
	ColorParam->ParameterName = TEXT("Color");
	ColorParam->DefaultValue = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);
	Mat->GetExpressionCollection().AddExpression(ColorParam);

	// Texture parameter — explicit white default so unbound texture gives (1,1,1,1)
	auto* TexParam = NewObject<UMaterialExpressionTextureSampleParameter2D>(Mat);
	TexParam->ParameterName = TEXT("BannerTexture");
	TexParam->Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture"));
	Mat->GetExpressionCollection().AddExpression(TexParam);

	// Multiply: TexRGB * Color → BaseColor
	auto* MulColor = NewObject<UMaterialExpressionMultiply>(Mat);
	Mat->GetExpressionCollection().AddExpression(MulColor);

#if WITH_EDITOR
	MulColor->A.Connect(0, TexParam);
	MulColor->B.Connect(0, ColorParam);

	auto* Ed = Mat->GetEditorOnlyData();
	Ed->BaseColor.Connect(0, MulColor);
	Ed->OpacityMask.Connect(4, TexParam); // output 4 = Alpha
#endif

	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	CachedMat = Mat;
	return Mat;
}

/** Create a shared translucent material for UTextRenderComponent.
 *  Uses UMaterialExpressionFontSampleParameter so UTextRenderComponent's MID cache
 *  discovers the font parameter via GetAllFontParameterInfo() and binds the font texture.
 *  Color param tints the text, Opacity param controls fade.
 *  BaseColor = FontSample.RGB * Color, Opacity = FontSample.A * Opacity. */
static UMaterial* GetTextTranslucentMaterial()
{
	static TWeakObjectPtr<UMaterial> CachedMat;
	if (CachedMat.IsValid()) return CachedMat.Get();

	UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(), TEXT("TextTranslucentMat"), RF_Transient);
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

	// Font sample parameter — UTextRenderComponent binds the font texture to this via SetFontParameterValue
	auto* FontParam = NewObject<UMaterialExpressionFontSampleParameter>(Mat);
	FontParam->ParameterName = TEXT("FontTexture");
	FontParam->ExpressionGUID = FGuid::NewGuid();
	// Default font for material compilation (overridden at runtime by MID cache)
	FontParam->Font = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/RobotoDistanceField"));
	FontParam->FontTexturePage = 0;
	Mat->GetExpressionCollection().AddExpression(FontParam);

	// Multiply: FontRGB * Color → BaseColor
	auto* MulColor = NewObject<UMaterialExpressionMultiply>(Mat);
	Mat->GetExpressionCollection().AddExpression(MulColor);

	// Threshold the distance field:
	// FontAlpha is SDF: 0.5 = edge, >0.5 = inside glyph, <0.5 = outside
	// (FontAlpha - 0.5) * Sharpness → controls edge hardness
	// Adjustable via "Sharpness" scalar parameter on each banner's MID.

	// Subtract: FontAlpha - 0.5
	auto* SubThreshold = NewObject<UMaterialExpressionSubtract>(Mat);
	SubThreshold->ConstB = 0.5f;
	Mat->GetExpressionCollection().AddExpression(SubThreshold);

	// Sharpness parameter — adjustable per banner instance
	auto* SharpnessParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
	SharpnessParam->ParameterName = TEXT("Sharpness");
	SharpnessParam->DefaultValue = 50.0f;
	Mat->GetExpressionCollection().AddExpression(SharpnessParam);

	// Multiply by sharpness factor
	auto* MulSharpen = NewObject<UMaterialExpressionMultiply>(Mat);
	Mat->GetExpressionCollection().AddExpression(MulSharpen);

	// Multiply sharpened mask by Opacity parameter for fade animation
	auto* MulOpacity = NewObject<UMaterialExpressionMultiply>(Mat);
	Mat->GetExpressionCollection().AddExpression(MulOpacity);

#if WITH_EDITOR
	MulColor->A.Connect(0, FontParam);   // output 0 = RGB
	MulColor->B.Connect(0, ColorParam);

	SubThreshold->A.Connect(4, FontParam); // output 4 = Alpha (SDF value)
	MulSharpen->A.Connect(0, SubThreshold);
	MulSharpen->B.Connect(0, SharpnessParam);
	MulOpacity->A.Connect(0, MulSharpen);
	MulOpacity->B.Connect(0, OpacityParam);

	auto* Ed = Mat->GetEditorOnlyData();
	Ed->BaseColor.Connect(0, MulColor);
	Ed->Opacity.Connect(0, MulOpacity);
#endif

	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	CachedMat = Mat;
	return Mat;
}

void ABannerActor::WarmUpMaterials()
{
	GetBannerTranslucentMaterial();
	GetBannerOpaqueMaterial();
	GetTextTranslucentMaterial();
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
	if (TextDynamicMaterial)
	{
		TextDynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void ABannerActor::SetBannerTexture(UTexture2D* InTexture)
{
	BannerTexture = InTexture;
	if (DynamicMaterial && BannerTexture)
	{
		DynamicMaterial->SetTextureParameterValue(TEXT("BannerTexture"), BannerTexture);
	}
}

void ABannerActor::InitBanner(
	const FFlightEventData& InEventData,
	float InWidth,
	float InHeight,
	bool bUseOpaqueMaterial,
	FColor InWireframeColor,
	float InRotationYaw)
{
	EventData = InEventData;

	// UE Plane is 100x100cm. Scale to desired width x height.
	const float ScaleX = InWidth / 100.0f;
	const float ScaleY = InHeight / 100.0f;
	BannerMesh->SetRelativeScale3D(FVector(ScaleX, ScaleY, 1.0f));
	BannerMesh->SetRelativeRotation(FRotator(0.0f, InRotationYaw, 0.0f));

	if (bUseOpaqueMaterial)
	{
		// Dev opaque mode: custom opaque material with texture support (yellow default)
		UMaterial* OpaqueMat = GetBannerOpaqueMaterial();
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
		// Translucent material with texture support (text is a separate component)
		UMaterial* BaseMat = GetBannerTranslucentMaterial();
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
		TextComponent->SetRelativeRotation(FRotator(90.0f, InRotationYaw, 0.0f));

		// Assign translucent material so text writes to scene alpha channel
		UMaterial* TextBaseMat = GetTextTranslucentMaterial();
		if (TextBaseMat)
		{
			TextDynamicMaterial = UMaterialInstanceDynamic::Create(TextBaseMat, this);
			if (TextDynamicMaterial)
			{
				TextDynamicMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(TextColor));
				TextDynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), bUseOpaqueMaterial ? 1.0f : 0.0f);
				TextDynamicMaterial->SetScalarParameterValue(TEXT("Sharpness"), TextSDFSharpness);
				TextComponent->SetMaterial(0, TextDynamicMaterial);
			}
		}
	}

	BannerMesh->bOverrideWireframeColor = true;
	BannerMesh->WireframeColorOverride = InWireframeColor;
	BannerMesh->MarkRenderStateDirty();

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
	if (TextDynamicMaterial)
	{
		TextDynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), Alpha);
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
	if (TextDynamicMaterial)
	{
		TextDynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), FadeAlpha);
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
