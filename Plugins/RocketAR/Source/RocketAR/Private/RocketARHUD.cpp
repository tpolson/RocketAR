#include "RocketARHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"

void ARocketARHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas) return;

	const float ScreenW = Canvas->SizeX;
	const float ScreenH = Canvas->SizeY;
	const float DeltaTime = GetWorld()->GetDeltaSeconds();

	// Use the default large engine font
	UFont* HUDFont = GEngine->GetLargeFont();
	if (!HUDFont) return;

	// --- Bottom-left telemetry readout ---
	if (bHasTelemetry)
	{
		const FString METStr = FString::Printf(TEXT("MET  %s"), *FormatMET(CurrentMET));
		const FString AltStr = FString::Printf(TEXT("ALT  %s"), *FormatAltitude(CurrentAltitude));
		const FString VelStr = FString::Printf(TEXT("VEL  %.0f m/s"), CurrentVelocity);

		const float TextScale = FMath::Max(1.0f, ScreenH / 720.0f);
		const float LineHeight = 24.0f * TextScale;
		const float Margin = 20.0f * TextScale;

		// Draw from bottom-left, stacking upward
		float Y = ScreenH - Margin - LineHeight * 3.0f;
		const float X = Margin;

		// Semi-transparent background box
		const float BoxW = 300.0f * TextScale;
		const float BoxH = LineHeight * 3.0f + Margin;
		FCanvasTileItem BG(FVector2D(X - 5.0f, Y - 5.0f), FVector2D(BoxW, BoxH), FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
		BG.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(BG);

		// Draw text lines
		FCanvasTextItem TextItem(FVector2D(X, Y), FText::FromString(METStr), HUDFont, FLinearColor::White);
		TextItem.Scale = FVector2D(TextScale, TextScale);
		TextItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(TextItem);

		Y += LineHeight;
		TextItem.Text = FText::FromString(AltStr);
		TextItem.Position = FVector2D(X, Y);
		Canvas->DrawItem(TextItem);

		Y += LineHeight;
		TextItem.Text = FText::FromString(VelStr);
		TextItem.Position = FVector2D(X, Y);
		Canvas->DrawItem(TextItem);
	}

	// --- Event name display (center-top, with fade) ---
	if (EventDisplayTimer > 0.0f)
	{
		EventDisplayTimer -= DeltaTime;

		float Alpha = 1.0f;
		if (EventDisplayTimer < EventFadeDuration)
		{
			Alpha = FMath::Max(0.0f, EventDisplayTimer / EventFadeDuration);
		}

		const float EventScale = FMath::Max(1.5f, ScreenH / 480.0f);

		FCanvasTextItem EventText(
			FVector2D::ZeroVector,
			FText::FromString(CurrentEventName),
			HUDFont,
			FLinearColor(1.0f, 0.9f, 0.2f, Alpha));
		EventText.Scale = FVector2D(EventScale, EventScale);
		EventText.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, Alpha));
		EventText.bCentreX = true;

		// Position at top-center
		EventText.Position = FVector2D(ScreenW * 0.5f, ScreenH * 0.08f);
		Canvas->DrawItem(EventText);
	}
}

void ARocketARHUD::ShowEvent(const FFlightEventData& EventData)
{
	if (EventData.EventType == EFlightEvent::AltitudeMarker)
	{
		CurrentEventName = FString::Printf(TEXT("ALTITUDE  |  %s  |  %.0f m/s"),
			*EventData.EventLabel,
			EventData.Velocity);
	}
	else
	{
		CurrentEventName = FString::Printf(TEXT("%s  |  %s  |  %.0f m/s"),
			*EventData.EventLabel,
			*FormatAltitude(EventData.Altitude),
			EventData.Velocity);
	}
	EventDisplayTimer = EventDisplayDuration;
}

void ARocketARHUD::UpdateTelemetry(const FProcessedTelemetryData& Data)
{
	CurrentAltitude = Data.AltitudeASL;
	CurrentVelocity = Data.VelocityMagnitude;
	CurrentMET = Data.RawData.MissionElapsedTime;
	bHasTelemetry = true;
}

FString ARocketARHUD::FormatAltitude(double AltMeters) const
{
	if (AltMeters >= 1000.0)
	{
		return FString::Printf(TEXT("%.1f km"), AltMeters / 1000.0);
	}
	return FString::Printf(TEXT("%.0f m"), AltMeters);
}

FString ARocketARHUD::FormatMET(double MET) const
{
	const bool bNeg = MET < 0.0;
	const double Abs = FMath::Abs(MET);
	const int32 Min = static_cast<int32>(Abs / 60.0);
	const int32 Sec = static_cast<int32>(FMath::Fmod(Abs, 60.0));
	return FString::Printf(TEXT("T%s%02d:%02d"), bNeg ? TEXT("-") : TEXT("+"), Min, Sec);
}
