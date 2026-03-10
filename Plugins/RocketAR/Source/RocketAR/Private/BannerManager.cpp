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
	QueueBanner(EventData, LastVehicleUEVelocity);
}

void UBannerManager::QueueBanner(const FFlightEventData& EventData, const FVector& TrajectoryAtTrigger)
{
	UWorld* World = GetWorld();
	if (!World) return;

	FPendingBanner Pending;
	Pending.EventData = EventData;
	Pending.TrajectoryAtTrigger = TrajectoryAtTrigger;
	Pending.SpawnPosition = LastVehicleUEPosition;
	Pending.TriggerWorldTime = World->GetTimeSeconds() + TriggerTimeOffset - AnticipationSeconds;

	PendingBanners.Add(Pending);

	UE_LOG(LogRocketAR, Log, TEXT("BannerManager: Queued banner '%s' (offset=%.2fs, anticipation=%.2fs)"),
		*EventData.EventLabel, TriggerTimeOffset, AnticipationSeconds);
}

ABannerActor* UBannerManager::SpawnBanner(const FFlightEventData& EventData)
{
	// Immediate spawn (no queue delay) using current cached state
	FPendingBanner Immediate;
	Immediate.EventData = EventData;
	Immediate.TrajectoryAtTrigger = LastVehicleUEVelocity;
	Immediate.SpawnPosition = LastVehicleUEPosition;
	return SpawnBannerFromQueue(Immediate);
}

ABannerActor* UBannerManager::SpawnBannerFromQueue(const FPendingBanner& Pending)
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	// Enforce max banner limit
	while (ActiveBanners.Num() >= MaxActiveBanners)
	{
		CullOldestBanner();
	}

	// Spawn at world origin, then attach to rocket mount point
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABannerActor* Banner = World->SpawnActor<ABannerActor>(
		BannerActorClass ? BannerActorClass.Get() : ABannerActor::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (!Banner)
	{
		UE_LOG(LogRocketAR, Error, TEXT("BannerManager: Failed to spawn banner actor"));
		return nullptr;
	}

	// Determine type early so we can pick the right spawn offset
	const bool bIsMarker = (Pending.EventData.EventType == EFlightEvent::AltitudeMarker);

	// Attach to rocket mount point — banner moves with the rocket
	if (AttachTarget)
	{
		Banner->AttachToComponent(AttachTarget, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		// Offset above vehicle center (toward nose) so camera flies past banners
		const float SpawnZ = bIsMarker ? MarkerSpawnZOffset : BannerSpawnZOffset;
		Banner->SetActorRelativeLocation(FVector(0.0f, 0.0f, SpawnZ));
		Banner->SetActorRelativeRotation(FRotator::ZeroRotator);
	}
	else
	{
		// Fallback: no parent, spawn at cached position
		Banner->SetActorLocation(Pending.SpawnPosition);
	}

	Banner->LifetimeSeconds = SlideDuration;
	Banner->FadeOutDuration = BannerFadeOutDuration;
	Banner->FadeInDuration = FadeInDuration;
	const float Width = bIsMarker ? MarkerWidth : BannerWidth;
	const float Height = bIsMarker ? MarkerHeight : BannerHeight;

	// Configure text before InitBanner
	Banner->TextWorldSize = bIsMarker ? MarkerTextSize : BannerTextSize;
	Banner->TextOffset = bIsMarker ? MarkerTextOffset : BannerTextOffset;

	const FColor WireframeColor = bIsMarker ? FColor(0, 200, 255) : FColor(255, 255, 0);

	Banner->InitBanner(
		Pending.EventData,
		Width,
		Height,
		bDevOpaqueBanners,
		WireframeColor);

	// Set marker color tint
	if (bIsMarker)
	{
		Banner->SetBannerColor(MarkerColor);
	}

	Banner->InitSlide(SlideSpeed);

	ActiveBanners.Add(Banner);
	SET_DWORD_STAT(STAT_ActiveBanners, ActiveBanners.Num());

	const TCHAR* TypeLabel = bIsMarker ? TEXT("altitude marker") : TEXT("banner");
	UE_LOG(LogRocketAR, Log, TEXT("BannerManager: Spawned %s '%s' slide=%.0f textSize=%.0f (total: %d)"),
		TypeLabel,
		*Pending.EventData.EventLabel,
		SlideSpeed,
		Banner->TextWorldSize,
		ActiveBanners.Num());

	if (GEngine && bShowDebugMessages)
	{
		const TCHAR* DebugPrefix = bIsMarker ? TEXT("ALTITUDE") : TEXT("BANNER");
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow,
			FString::Printf(TEXT("%s: %s at (%.0f, %.0f, %.0f)"),
				DebugPrefix,
				*Pending.EventData.EventLabel,
				Pending.SpawnPosition.X, Pending.SpawnPosition.Y, Pending.SpawnPosition.Z));
	}

	return Banner;
}

void UBannerManager::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Process pending queue: spawn banners whose trigger time has elapsed
	UWorld* World = GetWorld();
	if (World)
	{
		const double Now = World->GetTimeSeconds();
		for (int32 i = PendingBanners.Num() - 1; i >= 0; --i)
		{
			if (Now >= PendingBanners[i].TriggerWorldTime)
			{
				SpawnBannerFromQueue(PendingBanners[i]);
				PendingBanners.RemoveAt(i);
			}
		}
	}

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
	PendingBanners.Empty();
	UE_LOG(LogRocketAR, Log, TEXT("BannerManager: All banners destroyed"));
}
