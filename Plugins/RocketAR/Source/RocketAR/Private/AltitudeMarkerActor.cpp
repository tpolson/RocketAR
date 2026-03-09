#include "AltitudeMarkerActor.h"
#include "ProceduralArcMesh.h"
#include "RocketARModule.h"
#include "ProceduralMeshComponent.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/Font.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"

AAltitudeMarkerActor::AAltitudeMarkerActor()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MarkerMesh"));
	MeshComponent->bCastDynamicShadow = false;
	MeshComponent->CastShadow = false;
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->bUseAsOccluder = false;
	RootComponent = MeshComponent;
}

void AAltitudeMarkerActor::BeginPlay()
{
	Super::BeginPlay();
	SpawnTime = GetWorld()->GetTimeSeconds();
}

void AAltitudeMarkerActor::InitMarker(
	const FString& Label,
	double AltitudeMeters,
	float ArcAngleDeg,
	float ArcRadius,
	float ArcHeight,
	int32 ArcSegments,
	UMaterialInterface* MarkerMaterial,
	UFont* TextFont)
{
	DisplayLabel = Label;
	MarkerAltitude = AltitudeMeters;
	MarkerFont = TextFont;

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
		this, UCanvasRenderTarget2D::StaticClass(), 512, 128);

	if (RenderTarget)
	{
		RenderTarget->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		RenderTextToTarget();
	}

	// Use bright material for now (debug visibility)
	{
		UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (BaseMat)
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (DynamicMaterial)
			{
				// Cyan color to distinguish from yellow banners
				DynamicMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.0f, 0.8f, 1.0f, 1.0f));
				MeshComponent->SetMaterial(0, DynamicMaterial);
			}
		}
	}

	bInitialized = true;

	UE_LOG(LogRocketAR, Log, TEXT("AltitudeMarker initialized: '%s' at %.0fm"), *DisplayLabel, MarkerAltitude);
}

void AAltitudeMarkerActor::RenderTextToTarget()
{
	if (!RenderTarget) return;

	RenderTarget->UpdateResource();

	UCanvas* Canvas = nullptr;
	FVector2D Size;
	FDrawToRenderTargetContext Context;

	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, RenderTarget, Canvas, Size, Context);

	if (Canvas)
	{
		UFont* Font = MarkerFont ? MarkerFont : GEngine->GetLargeFont();
		if (Font)
		{
			const float FontScale = 2.0f;
			const float TextWidth = DisplayLabel.Len() * 16.0f * FontScale;
			const float X = (Size.X - TextWidth) * 0.5f;
			const float Y = (Size.Y - 32.0f * FontScale) * 0.5f;

			FCanvasTextItem TextItem(FVector2D(X, Y), FText::FromString(DisplayLabel), Font, FLinearColor::White);
			TextItem.Scale = FVector2D(FontScale, FontScale);
			TextItem.bOutlined = true;
			TextItem.OutlineColor = FLinearColor(0, 0, 0, 0.5f);
			Canvas->DrawItem(TextItem);
		}
	}

	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
}

void AAltitudeMarkerActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bInitialized) return;

	// Lifetime check
	if (LifetimeSeconds > 0.0f && !bFading)
	{
		CurrentLifetime += DeltaTime;
		if (CurrentLifetime >= LifetimeSeconds)
		{
			StartFadeOut();
		}
	}

	// Fade
	if (bFading)
	{
		if (FadeOutDuration <= 0.0f)
		{
			Destroy();
			return;
		}

		FadeAlpha -= DeltaTime / FadeOutDuration;
		if (FadeAlpha <= 0.0f)
		{
			Destroy();
			return;
		}
	}

	UpdateCameraFacing();
}

void AAltitudeMarkerActor::UpdateCameraFacing()
{
	APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CamMgr) return;

	const FVector CamPos = CamMgr->GetCameraLocation();
	const FVector MarkerPos = GetActorLocation();
	FVector ToCamera = CamPos - MarkerPos;
	ToCamera.Z = 0.0f;

	if (ToCamera.SizeSquared() > 1.0f)
	{
		FRotator LookRot = ToCamera.Rotation();
		// Apply user rotation offset
		LookRot += MarkerRotationOffset;
		SetActorRotation(LookRot);
	}
}

void AAltitudeMarkerActor::StartFadeOut()
{
	if (bFading) return;
	bFading = true;
	FadeAlpha = 1.0f;
}

void AAltitudeMarkerActor::ForceDestroy()
{
	Destroy();
}
