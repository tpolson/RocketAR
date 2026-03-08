#include "DevVisualizationActor.h"
#include "RocketARModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ADevVisualizationActor::ADevVisualizationActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Earth sphere — uses engine's default sphere mesh
	EarthMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EarthMesh"));
	EarthMesh->SetupAttachment(Root);
	EarthMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EarthMesh->CastShadow = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshFinder.Succeeded())
	{
		EarthMesh->SetStaticMesh(SphereMeshFinder.Object);
	}

	// Rocket cylinder
	RocketMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RocketMesh"));
	RocketMesh->SetupAttachment(Root);
	RocketMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RocketMesh->CastShadow = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshFinder.Succeeded())
	{
		RocketMesh->SetStaticMesh(CylinderMeshFinder.Object);
	}
}

void ADevVisualizationActor::BeginPlay()
{
	Super::BeginPlay();

	// Earth: real scale. Default sphere is 100cm diameter = 50cm radius.
	// Earth radius = 6,371,000m = 637,100,000cm
	// Scale = 637,100,000 / 50 = 12,742,000
	const double EarthRadiusCm = 6371000.0 * 100.0;
	const double SphereDefaultRadius = 50.0;
	const double EarthScale = EarthRadiusCm / SphereDefaultRadius;
	EarthMesh->SetWorldScale3D(FVector(EarthScale));

	// Position Earth center at (0,0,-EarthRadius) in UE space
	// because our UE origin is at the launch pad on Earth's surface
	EarthMesh->SetWorldLocation(FVector(0.0, 0.0, -EarthRadiusCm));

	// Create a simple blue wireframe-ish material for Earth
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BaseMat)
	{
		EarthMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
		if (EarthMaterial)
		{
			EarthMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.2f, 0.8f, 1.0f));
			EarthMesh->SetMaterial(0, EarthMaterial);
		}
	}

	// Rocket: ~100m tall, ~8m diameter
	// Default cylinder is 100cm diameter (50cm radius), 100cm tall
	// Scale X,Y = 800cm / 100cm = 8, Z = 10000cm / 100cm = 100
	RocketMesh->SetWorldScale3D(FVector(8.0, 8.0, 100.0));

	if (BaseMat)
	{
		RocketMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
		if (RocketMaterial)
		{
			RocketMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.3f, 0.1f, 1.0f));
			RocketMesh->SetMaterial(0, RocketMaterial);
		}
	}

	UE_LOG(LogRocketAR, Log, TEXT("DevVisualization: Earth scale=%.0f, centered at Z=%.0f"),
		EarthScale, -EarthRadiusCm);
}

void ADevVisualizationActor::UpdateFromTelemetry(const FProcessedTelemetryData& Data)
{
	if (RocketMesh && bIsVisible)
	{
		RocketMesh->SetWorldLocation(Data.UEPosition);
		RocketMesh->SetWorldRotation(Data.UERotation.Rotator());
	}
}

void ADevVisualizationActor::SetVisible(bool bVisible)
{
	bIsVisible = bVisible;
	SetActorHiddenInGame(!bVisible);
	if (EarthMesh) EarthMesh->SetVisibility(bVisible);
	if (RocketMesh) RocketMesh->SetVisibility(bVisible);

	UE_LOG(LogRocketAR, Log, TEXT("DevVisualization: %s"), bVisible ? TEXT("Visible") : TEXT("Hidden"));
}
