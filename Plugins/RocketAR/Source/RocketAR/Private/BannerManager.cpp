#include "BannerManager.h"
#include "BannerActor.h"
#include "FlightEventDetector.h"
#include "RocketARModule.h"
#include "Engine/World.h"

DECLARE_STATS_GROUP(TEXT("RocketAR"), STATGROUP_RocketAR, STATCAT_Advanced);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Banners"), STAT_ActiveBanners, STATGROUP_RocketAR);

UBannerManager::UBannerManager()
{
	PrimaryComponentTick.bCanEverTick = true;

	BannerActorClass = ABannerActor::StaticClass();
}

void UBannerManager::SetEventDetector(UFlightEventDetector* Detector)
{
	if (EventDetector)
	{
		EventDetector->OnFlightEvent.RemoveDynamic(this, &UBannerManager::OnFlightEventDetected);
	}

	EventDetector = Detector;

	if (EventDetector)
	{
		EventDetector->OnFlightEvent.AddDynamic(this, &UBannerManager::OnFlightEventDetected);
		UE_LOG(LogRocketAR, Log, TEXT("BannerManager: Connected to event detector"));
	}
}

void UBannerManager::OnFlightEventDetected(const FFlightEventData& EventData)
{
	SpawnBanner(EventData);
}

ABannerActor* UBannerManager::SpawnBanner(const FFlightEventData& EventData)
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	// Enforce max banner limit
	while (ActiveBanners.Num() >= MaxActiveBanners)
	{
		CullOldestBanner();
	}

	// Spawn the banner actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABannerActor* Banner = World->SpawnActor<ABannerActor>(
		BannerActorClass ? BannerActorClass.Get() : ABannerActor::StaticClass(),
		LastVehicleUEPosition, FRotator::ZeroRotator, SpawnParams);

	if (!Banner)
	{
		UE_LOG(LogRocketAR, Error, TEXT("BannerManager: Failed to spawn banner actor"));
		return nullptr;
	}

	Banner->LifetimeSeconds = BannerLifetimeSeconds;
	Banner->FadeOutDuration = BannerFadeOutDuration;

	Banner->InitBanner(
		EventData,
		BannerArcAngle,
		BannerArcRadius,
		BannerArcHeight,
		BannerArcSegments,
		BannerMaterial,
		BannerFont);

	ActiveBanners.Add(Banner);
	SET_DWORD_STAT(STAT_ActiveBanners, ActiveBanners.Num());

	UE_LOG(LogRocketAR, Log, TEXT("BannerManager: Spawned banner '%s' at (%.0f, %.0f, %.0f) mat=%s (total: %d)"),
		*EventData.EventLabel,
		LastVehicleUEPosition.X, LastVehicleUEPosition.Y, LastVehicleUEPosition.Z,
		BannerMaterial ? *BannerMaterial->GetName() : TEXT("NULL"),
		ActiveBanners.Num());

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow,
			FString::Printf(TEXT("BANNER: %s at (%.0f, %.0f, %.0f)"),
				*EventData.EventLabel,
				LastVehicleUEPosition.X, LastVehicleUEPosition.Y, LastVehicleUEPosition.Z));
	}

	return Banner;
}

ABannerActor* UBannerManager::SpawnBannerAtPosition(const FFlightEventData& EventData, const FVector& UEPosition)
{
	FVector SavedPos = LastVehicleUEPosition;
	LastVehicleUEPosition = UEPosition;
	ABannerActor* Banner = SpawnBanner(EventData);
	LastVehicleUEPosition = SavedPos;
	return Banner;
}

void UBannerManager::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	CleanupDestroyedBanners();
	SET_DWORD_STAT(STAT_ActiveBanners, ActiveBanners.Num());
}

void UBannerManager::CullOldestBanner()
{
	if (ActiveBanners.Num() == 0) return;

	// Find the oldest banner
	ABannerActor* Oldest = nullptr;
	int32 OldestIdx = -1;
	double OldestTime = TNumericLimits<double>::Max();

	for (int32 i = 0; i < ActiveBanners.Num(); ++i)
	{
		if (ActiveBanners[i] && ActiveBanners[i]->SpawnTime < OldestTime)
		{
			OldestTime = ActiveBanners[i]->SpawnTime;
			Oldest = ActiveBanners[i];
			OldestIdx = i;
		}
	}

	if (Oldest)
	{
		Oldest->StartFadeOut();
		UE_LOG(LogRocketAR, Log, TEXT("BannerManager: Force-fading oldest banner '%s'"),
			*Oldest->GetEventData().EventLabel);
	}
}

void UBannerManager::CleanupDestroyedBanners()
{
	ActiveBanners.RemoveAll([](const ABannerActor* Banner)
	{
		return !Banner || !IsValid(Banner) || Banner->GetBannerState() == EBannerState::Destroyed;
	});
}

void UBannerManager::DestroyAllBanners()
{
	for (ABannerActor* Banner : ActiveBanners)
	{
		if (Banner && IsValid(Banner))
		{
			Banner->ForceDestroy();
		}
	}
	ActiveBanners.Empty();
	UE_LOG(LogRocketAR, Log, TEXT("BannerManager: All banners destroyed"));
}
