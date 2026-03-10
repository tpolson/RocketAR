#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "RocketARSetupActorDetails.h"

class FRocketAREditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FPropertyEditorModule& PropertyModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.RegisterCustomClassLayout(
			TEXT("RocketARSetupActor"),
			FOnGetDetailCustomizationInstance::CreateStatic(&FRocketARSetupActorDetails::MakeInstance));
	}

	virtual void ShutdownModule() override
	{
		if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
		{
			FPropertyEditorModule& PropertyModule =
				FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomClassLayout(TEXT("RocketARSetupActor"));
		}
	}
};

IMPLEMENT_MODULE(FRocketAREditorModule, RocketAREditor)
