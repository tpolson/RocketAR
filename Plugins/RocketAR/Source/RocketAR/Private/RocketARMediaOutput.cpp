#include "RocketARMediaOutput.h"
#include "RocketARModule.h"
#include "Engine/World.h"

URocketARMediaOutput::URocketARMediaOutput()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URocketARMediaOutput::BeginPlay()
{
	Super::BeginPlay();

	// DeckLink support requires BlackmagicMedia plugin and MediaIOCore module.
	// When hardware is available, uncomment the dependencies in Build.cs and
	// enable the full implementation.
	bDeckLinkAvailable = false;
	UE_LOG(LogRocketAR, Log, TEXT("DeckLink: Media output disabled (BlackmagicMedia not configured)"));
}

void URocketARMediaOutput::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCapture();
	Super::EndPlay(EndPlayReason);
}

void URocketARMediaOutput::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool URocketARMediaOutput::StartCapture()
{
	UE_LOG(LogRocketAR, Warning, TEXT("DeckLink: Cannot start capture — BlackmagicMedia not configured"));
	return false;
}

void URocketARMediaOutput::StopCapture()
{
	bCaptureActive = false;
	bNeedsRestart = false;
}
