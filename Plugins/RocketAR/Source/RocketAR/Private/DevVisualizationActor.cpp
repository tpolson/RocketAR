#include "DevVisualizationActor.h"
#include "RocketDefinition.h"
#include "RocketARModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "UObject/ConstructorHelpers.h"

/** Create a shared opaque unlit material — always bright, writes alpha. */
static UMaterial* GetDevOpaqueMaterial()
{
	static TWeakObjectPtr<UMaterial> CachedMat;
	if (CachedMat.IsValid()) return CachedMat.Get();

	UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(), TEXT("DevOpaqueMat"), RF_Transient);
	Mat->BlendMode = BLEND_Opaque;
	Mat->SetShadingModel(MSM_Unlit);

	auto* ColorParam = NewObject<UMaterialExpressionVectorParameter>(Mat);
	ColorParam->ParameterName = TEXT("Color");
	ColorParam->DefaultValue = FLinearColor(1.0f, 0.3f, 0.1f, 1.0f);
	Mat->GetExpressionCollection().AddExpression(ColorParam);

#if WITH_EDITOR
	Mat->GetEditorOnlyData()->EmissiveColor.Connect(0, ColorParam);
#endif

	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	CachedMat = Mat;
	return Mat;
}

/** Create a shared additive-blend material that is visible in the viewport
 *  but does NOT write to the scene alpha channel (preserves broadcast key). */
static UMaterial* GetDevAdditiveMaterial()
{
	static TWeakObjectPtr<UMaterial> CachedMat;
	if (CachedMat.IsValid()) return CachedMat.Get();

	UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(), TEXT("DevAdditiveMat"), RF_Transient);
	Mat->BlendMode = BLEND_Additive;
	Mat->SetShadingModel(MSM_Unlit);
	Mat->TwoSided = true;

	auto* ColorParam = NewObject<UMaterialExpressionVectorParameter>(Mat);
	ColorParam->ParameterName = TEXT("Color");
	ColorParam->DefaultValue = FLinearColor(0.1f, 0.2f, 0.8f, 1.0f);
	Mat->GetExpressionCollection().AddExpression(ColorParam);

#if WITH_EDITOR
	Mat->GetEditorOnlyData()->EmissiveColor.Connect(0, ColorParam);
#endif

	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	CachedMat = Mat;
	return Mat;
}

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
	EarthMesh->bOverrideWireframeColor = true;
	EarthMesh->WireframeColorOverride = FColor(50, 100, 255);

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
	RocketMesh->bOverrideWireframeColor = true;
	RocketMesh->WireframeColorOverride = FColor(255, 120, 30);

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
	const double EarthRadiusCm = 6371000.0 * 100.0;
	const double SphereDefaultRadius = 50.0;
	const double EarthScale = EarthRadiusCm / SphereDefaultRadius;
	EarthMesh->SetWorldScale3D(FVector(EarthScale));

	// Default position — will be overridden by SetEarthTransform() from setup actor
	EarthMesh->SetWorldLocation(FVector(0.0, 0.0, -EarthRadiusCm));

	// Additive materials — visible in viewport but don't write scene alpha (preserves broadcast key)
	UMaterial* AdditiveMat = GetDevAdditiveMaterial();
	if (AdditiveMat)
	{
		EarthMaterial = UMaterialInstanceDynamic::Create(AdditiveMat, this);
		if (EarthMaterial)
		{
			EarthMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.02f, 0.03f, 0.15f, 1.0f));
			EarthMesh->SetMaterial(0, EarthMaterial);
		}
	}

	// Apply rocket dimensions (configurable via RocketHeight/RocketRadius)
	UpdateRocketDimensions();

	// Rocket: opaque unlit — solid, always bright, no lighting needed
	UMaterial* RocketMat = GetDevOpaqueMaterial();
	if (RocketMat)
	{
		RocketMaterial = UMaterialInstanceDynamic::Create(RocketMat, this);
		if (RocketMaterial)
		{
			RocketMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.3f, 0.1f, 1.0f));
			RocketMesh->SetMaterial(0, RocketMaterial);
		}
	}

	UE_LOG(LogRocketAR, Log, TEXT("DevVisualization initialized"));
}

void ADevVisualizationActor::UpdateRocketDimensions()
{
	if (RocketMesh)
	{
		// UE cylinder: 50cm radius, 100cm tall at scale 1.0
		const float ScaleXY = (RocketRadius * 100.0f) / 50.0f;
		const float ScaleZ  = (RocketHeight * 100.0f) / 100.0f;
		RocketMesh->SetWorldScale3D(FVector(ScaleXY, ScaleXY, ScaleZ));
		RocketMesh->SetRelativeLocation(FVector(0.0, 0.0, RocketHeight * 50.0f));
	}
}

void ADevVisualizationActor::UpdateFromTelemetry(const FProcessedTelemetryData& Data)
{
	if (!bIsVisible && !bProductionVisible) return;

	// Use telemetry quaternion for full 3-axis orientation (proper roll, no flip)
	if (Data.RawData.bTelemetryValid)
	{
		RocketOrientation = Data.UERotation;
	}

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

void ADevVisualizationActor::SetVisible(bool bVisible)
{
	bIsVisible = bVisible;
	// Earth is always dev-only — never in production output
	if (EarthMesh) EarthMesh->SetVisibility(bVisible);
	// Rocket stays visible if production visibility is enabled (for CG fill-in)
	if (RocketMesh) RocketMesh->SetVisibility(bVisible || bProductionVisible);
	// Only fully hide the actor if neither dev nor production needs it visible
	SetActorHiddenInGame(!bVisible && !bProductionVisible);

	UE_LOG(LogRocketAR, Log, TEXT("DevVisualization: %s"), bVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void ADevVisualizationActor::ApplyRocketDefinition(URocketDefinition* Definition)
{
	ActiveDefinition = Definition;

	if (!Definition)
	{
		// Null definition → restore cylinder using existing RocketHeight/RocketRadius
		static const FSoftObjectPath CylinderPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (UStaticMesh* CylinderMesh = Cast<UStaticMesh>(CylinderPath.TryLoad()))
		{
			RocketMesh->SetStaticMesh(CylinderMesh);
		}
		LoadedRocketMesh = nullptr;
		UpdateRocketDimensions();
		SetProductionVisible(false);
		return;
	}

	RocketHeight = Definition->Height;
	RocketRadius = Definition->Radius;

	// Resolve soft pointer synchronously at BeginPlay time
	UStaticMesh* ResolvedMesh = Definition->RocketMesh.IsValid()
		? Definition->RocketMesh.Get()
		: Definition->RocketMesh.LoadSynchronous();
	LoadedRocketMesh = ResolvedMesh;

	if (ResolvedMesh)
	{
		// Real mesh: set directly, reset scale so the asset's own import scale is used
		RocketMesh->SetStaticMesh(ResolvedMesh);
		RocketMesh->SetWorldScale3D(FVector(1.0f));
		RocketMesh->SetRelativeLocation(FVector::ZeroVector);
		UE_LOG(LogRocketAR, Log, TEXT("DevVisualization: loaded mesh '%s' for rocket '%s'"),
			*ResolvedMesh->GetName(), *Definition->RocketName.ToString());
	}
	else
	{
		// No mesh → cylinder fallback with definition's real dimensions
		static const FSoftObjectPath CylinderPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (UStaticMesh* CylinderMesh = Cast<UStaticMesh>(CylinderPath.TryLoad()))
		{
			RocketMesh->SetStaticMesh(CylinderMesh);
		}
		UpdateRocketDimensions();
		UE_LOG(LogRocketAR, Log, TEXT("DevVisualization: no mesh for '%s', using cylinder fallback (H=%.1fm R=%.1fm)"),
			*Definition->RocketName.ToString(), RocketHeight, RocketRadius);
	}

	SetProductionVisible(Definition->bRocketVisibleInProduction);
}

void ADevVisualizationActor::SetProductionVisible(bool bVisible)
{
	bProductionVisible = bVisible;
	if (!RocketMesh) return;

	// Switch material for all slots: opaque writes fill+key; additive is viewport-only
	UMaterial* Mat = bVisible ? GetDevOpaqueMaterial() : GetDevAdditiveMaterial();
	const int32 NumSlots = FMath::Max(RocketMesh->GetNumMaterials(), 1);
	for (int32 i = 0; i < NumSlots; ++i)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.3f, 0.1f, 1.0f));
		RocketMesh->SetMaterial(i, MID);
		if (i == 0) RocketMaterial = MID; // keep in sync for single-slot cylinder case
	}

	// Rocket must be visible in-world if it's writing to production output
	if (RocketMesh) RocketMesh->SetVisibility(bIsVisible || bVisible);
	if (bVisible && !bIsVisible)
	{
		SetActorHiddenInGame(false);
	}
}
