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

	// Unscaled mount point at rocket base (for camera attachment)
	RocketMountPoint = CreateDefaultSubobject<USceneComponent>(TEXT("RocketMountPoint"));
	RocketMountPoint->SetupAttachment(Root);

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
		CylinderMeshAsset = CylinderMeshFinder.Object;
	}
}

void ADevVisualizationActor::BeginPlay()
{
	Super::BeginPlay();

	// Earth: real scale. Default sphere is 100cm diameter = 50cm radius.
	const double EarthRadiusCm = 6371000.0 * 100.0;
	const double SphereDefaultRadius = 50.0;
	const double EarthScale = EarthRadiusCm / SphereDefaultRadius;
	EarthMesh->SetWorldScale3D(FVector(EarthScale));

	// Default position — will be overridden by SetEarthTransform() from setup actor
	EarthMesh->SetWorldLocation(FVector(0.0, 0.0, -EarthRadiusCm));

	// Create a simple blue material for Earth
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

	// Offset mesh up by half its height so the base sits at the vehicle position
	RocketMesh->SetRelativeLocation(FVector(0.0, 0.0, 5000.0));

	if (BaseMat)
	{
		RocketMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
		if (RocketMaterial)
		{
			RocketMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.3f, 0.1f, 1.0f));
			RocketMesh->SetMaterial(0, RocketMaterial);
		}
	}

	UE_LOG(LogRocketAR, Log, TEXT("DevVisualization initialized"));
}

void ADevVisualizationActor::UpdateFromTelemetry(const FProcessedTelemetryData& Data)
{
	if (!bIsVisible) return;

	// Compute rocket orientation from velocity (align Z axis to direction of travel)
	if (bHasPrevPosition)
	{
		const FVector Delta = Data.UEPosition - PrevUEPosition;
		const double DeltaLen = Delta.Size();

		if (DeltaLen > 1.0) // Moving — align to velocity
		{
			const FVector VelDir = Delta / DeltaLen;
			// Build rotation that maps +Z to velocity direction
			RocketOrientation = FQuat::FindBetweenNormals(FVector::UpVector, VelDir);
		}
		// If not moving, keep previous orientation (starts as identity = straight up)
	}
	PrevUEPosition = Data.UEPosition;
	bHasPrevPosition = true;

	if (RocketMesh)
	{
		RocketMesh->SetWorldLocation(Data.UEPosition);
		RocketMesh->SetWorldRotation(RocketOrientation.Rotator());
	}

	// Mount point tracks rocket base with velocity-aligned orientation
	if (RocketMountPoint)
	{
		RocketMountPoint->SetWorldLocation(Data.UEPosition);
		RocketMountPoint->SetWorldRotation(RocketOrientation.Rotator());
	}
}

void ADevVisualizationActor::SetEarthTransform(const FVector& CenterUE, const FVector& NorthPoleDirectionUE)
{
	if (!EarthMesh) return;

	EarthMesh->SetWorldLocation(CenterUE);

	// Rotate sphere so mesh Z-pole aligns with Earth's actual north pole direction
	const FVector PoleDir = NorthPoleDirectionUE.GetSafeNormal();
	if (!PoleDir.IsNearlyZero())
	{
		const FQuat EarthRot = FQuat::FindBetweenNormals(FVector::UpVector, PoleDir);
		EarthMesh->SetWorldRotation(EarthRot.Rotator());
	}

	UE_LOG(LogRocketAR, Log, TEXT("DevVisualization: Earth center set, pole direction (%.3f, %.3f, %.3f)"),
		PoleDir.X, PoleDir.Y, PoleDir.Z);
}

void ADevVisualizationActor::SpawnEventDisk(const FVector& UEPosition, const FQuat& UERotation, const FString& Label)
{
	if (!CylinderMeshAsset) return;

	UStaticMeshComponent* Disk = NewObject<UStaticMeshComponent>(this);
	Disk->SetStaticMesh(CylinderMeshAsset);
	Disk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disk->CastShadow = false;
	Disk->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
	Disk->RegisterComponent();

	// Rocket is 8m (800cm) diameter, radius = 400cm
	// Disk is 4x radius = 1600cm radius = 3200cm diameter
	// Default cylinder: 100cm diameter, 100cm tall
	// Scale X,Y = 3200/100 = 32, Z = thin (1.0 → 100cm tall disk)
	Disk->SetWorldScale3D(FVector(32.0, 32.0, 1.0));
	Disk->SetWorldLocation(UEPosition);

	// Align disk with current rocket orientation
	Disk->SetWorldRotation(RocketOrientation.Rotator());

	// Create or reuse disk material (green-ish, distinct from rocket orange)
	if (!DiskMaterial)
	{
		UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (BaseMat)
		{
			DiskMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (DiskMaterial)
			{
				DiskMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.2f, 1.0f, 0.3f, 1.0f));
			}
		}
	}

	if (DiskMaterial)
	{
		Disk->SetMaterial(0, DiskMaterial);
	}

	Disk->SetVisibility(bIsVisible);
	EventDiskMeshes.Add(Disk);

	UE_LOG(LogRocketAR, Log, TEXT("DevVisualization: Event disk spawned for '%s' (%d total)"),
		*Label, EventDiskMeshes.Num());
}

void ADevVisualizationActor::ClearEventDisks()
{
	for (UStaticMeshComponent* Disk : EventDiskMeshes)
	{
		if (Disk)
		{
			Disk->DestroyComponent();
		}
	}
	EventDiskMeshes.Empty();
}

void ADevVisualizationActor::SetVisible(bool bVisible)
{
	bIsVisible = bVisible;
	SetActorHiddenInGame(!bVisible);
	if (EarthMesh) EarthMesh->SetVisibility(bVisible);
	if (RocketMesh) RocketMesh->SetVisibility(bVisible);

	for (UStaticMeshComponent* Disk : EventDiskMeshes)
	{
		if (Disk) Disk->SetVisibility(bVisible);
	}

	UE_LOG(LogRocketAR, Log, TEXT("DevVisualization: %s"), bVisible ? TEXT("Visible") : TEXT("Hidden"));
}
