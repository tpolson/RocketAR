#include "RocketARModule.h"

DEFINE_LOG_CATEGORY(LogRocketAR);

#define LOCTEXT_NAMESPACE "FRocketARModule"

void FRocketARModule::StartupModule()
{
	UE_LOG(LogRocketAR, Log, TEXT("RocketAR module starting up"));
}

void FRocketARModule::ShutdownModule()
{
	UE_LOG(LogRocketAR, Log, TEXT("RocketAR module shutting down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRocketARModule, RocketAR)
