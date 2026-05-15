#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RocketARMediaOutput.h"
#include "RocketAROperatorSettings.generated.h"

/**
 * Persistent operator-editable settings (per-mission presets).
 * Mirrors the most-used editable subset of ARocketARSetupActor's UPROPERTYs.
 *
 * Loaded into the setup actor in BeginPlay (before SetupCamera/SetupDeckLink)
 * so cooked builds start with the operator's last-applied configuration.
 *
 * Saved to slot "RocketAR_Operator" by default; named slots can be used
 * for per-mission presets ("SLS-1", "Falcon-9", etc.).
 */
UCLASS()
class ROCKETAR_API URocketAROperatorSettings : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* DefaultSlotName = TEXT("RocketAR_Operator");

	/** Load from a save slot. Returns a fresh defaults instance if the slot does not exist. */
	UFUNCTION(BlueprintCallable, Category = "Operator Settings")
	static URocketAROperatorSettings* LoadFromSlot(const FString& SlotName);

	/** Persist this instance to a save slot. */
	UFUNCTION(BlueprintCallable, Category = "Operator Settings")
	bool SaveToSlot(const FString& SlotName) const;

	// --- Launch Site ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch Site")
	double LaunchPadLatitude = 34.5811;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch Site")
	double LaunchPadLongitude = -120.6257;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch Site")
	double LaunchPadAltitude = 150.0;

	// --- Camera ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FVector CameraMountOffset = FVector(0.0f, 500.0f, 4000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FRotator CameraMountRotation = FRotator(-80.0f, -10.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraOpticalRoll = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta=(ClampMin="1.0", ClampMax="180.0"))
	float CameraHFOV = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta=(ClampMin="0"))
	int32 ActiveCameraRigIndex = 0;

	// --- Banner ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	float BannerWidth = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	float BannerHeight = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	float BannerRotationYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	int32 MaxActiveBanners = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner")
	float BannerTextSize = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner", meta=(ClampMin="1.0", ClampMax="200.0"))
	float TextSDFSharpness = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Slide")
	float SlideSpeed = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Slide")
	float SlideDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Slide")
	float BannerFadeInDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Slide")
	float BannerFadeOutDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Slide")
	float BannerSpawnZOffset = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner|Slide")
	float AnticipationSeconds = 1.5f;

	// --- Altitude Marker ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	bool bShowAltitudeMarkers = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	float AltitudeMarkerInterval = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Marker")
	FLinearColor MarkerColor = FLinearColor(0.2f, 0.8f, 1.0f, 1.0f);

	// --- HUD ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bShowHUDTelemetry = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bShowHUDEvents = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bShowDebugMessages = true;

	// --- DeckLink ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	bool bEnableDeckLink = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	ERocketAROutputResolution DeckLinkOutputResolution = ERocketAROutputResolution::Res_1080p60;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	bool bDeckLinkFillAndKey = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	bool bDeckLinkAutoStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	bool bDeckLinkGenlock = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeckLink")
	bool bDeckLinkTimecode = false;

	// --- CSV ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSV")
	bool bUseCSVProvider = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSV")
	FString CSVFilePath = TEXT("Data/SimulatedTelemetry.csv");
};
