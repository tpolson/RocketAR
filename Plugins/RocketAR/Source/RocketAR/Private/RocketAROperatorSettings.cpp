#include "RocketAROperatorSettings.h"
#include "RocketARModule.h"
#include "Kismet/GameplayStatics.h"

URocketAROperatorSettings* URocketAROperatorSettings::LoadFromSlot(const FString& SlotName)
{
	const FString EffectiveSlot = SlotName.IsEmpty() ? FString(DefaultSlotName) : SlotName;

	if (UGameplayStatics::DoesSaveGameExist(EffectiveSlot, 0))
	{
		USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(EffectiveSlot, 0);
		if (URocketAROperatorSettings* Settings = Cast<URocketAROperatorSettings>(Loaded))
		{
			UE_LOG(LogRocketAR, Log, TEXT("OperatorSettings: loaded slot '%s'"), *EffectiveSlot);
			return Settings;
		}
		UE_LOG(LogRocketAR, Warning, TEXT("OperatorSettings: slot '%s' exists but failed to cast — using defaults"), *EffectiveSlot);
	}
	else
	{
		UE_LOG(LogRocketAR, Log, TEXT("OperatorSettings: slot '%s' not found — using defaults"), *EffectiveSlot);
	}

	return Cast<URocketAROperatorSettings>(UGameplayStatics::CreateSaveGameObject(URocketAROperatorSettings::StaticClass()));
}

bool URocketAROperatorSettings::SaveToSlot(const FString& SlotName) const
{
	const FString EffectiveSlot = SlotName.IsEmpty() ? FString(DefaultSlotName) : SlotName;
	const bool bOk = UGameplayStatics::SaveGameToSlot(const_cast<URocketAROperatorSettings*>(this), EffectiveSlot, 0);
	UE_LOG(LogRocketAR, Log, TEXT("OperatorSettings: save '%s' = %s"), *EffectiveSlot, bOk ? TEXT("OK") : TEXT("FAILED"));
	return bOk;
}
