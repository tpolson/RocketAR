#include "CSVTelemetryProvider.h"
#include "RocketARModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

ACSVTelemetryProvider::ACSVTelemetryProvider()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
}

void ACSVTelemetryProvider::BeginPlay()
{
	Super::BeginPlay();

	// Resolve path
	FString FullPath;
	if (FPaths::IsRelative(CSVFilePath))
	{
		FullPath = FPaths::Combine(FPaths::ProjectContentDir(), CSVFilePath);
	}
	else
	{
		FullPath = CSVFilePath;
	}

	if (LoadCSV(FullPath))
	{
		UE_LOG(LogRocketAR, Log, TEXT("CSV loaded: %d rows from %s"), Rows.Num(), *FullPath);
		bCSVLoaded = true;

		if (bAutoPlay && Rows.Num() > 0)
		{
			CurrentMET = Rows[0].MET;
			Play();
		}
	}
	else
	{
		UE_LOG(LogRocketAR, Error, TEXT("Failed to load CSV from: %s"), *FullPath);
	}
}

bool ACSVTelemetryProvider::LoadCSV(const FString& FilePath)
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		UE_LOG(LogRocketAR, Error, TEXT("Cannot read CSV file: %s"), *FilePath);
		return false;
	}

	TArray<FString> Lines;
	FileContent.ParseIntoArrayLines(Lines);

	if (Lines.Num() < 2)
	{
		UE_LOG(LogRocketAR, Error, TEXT("CSV has fewer than 2 lines (header + data): %s"), *FilePath);
		return false;
	}

	// Validate header (first line)
	const FString& Header = Lines[0];
	if (!Header.Contains(TEXT("MET")) || !Header.Contains(TEXT("PosX")))
	{
		UE_LOG(LogRocketAR, Warning, TEXT("CSV header may not match expected format: %s"), *Header);
	}

	// Parse data rows
	Rows.Empty();
	int32 SkippedRows = 0;

	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		const FString& Line = Lines[i];
		if (Line.TrimStartAndEnd().IsEmpty())
		{
			continue;
		}

		FCSVTelemetryRow Row;
		if (ParseRow(Line, i + 1, Row))
		{
			Rows.Add(MoveTemp(Row));
		}
		else
		{
			SkippedRows++;
		}
	}

	if (SkippedRows > 0)
	{
		UE_LOG(LogRocketAR, Warning, TEXT("Skipped %d malformed rows in CSV"), SkippedRows);
	}

	return ValidateRows();
}

bool ACSVTelemetryProvider::ParseRow(const FString& Line, int32 LineNumber, FCSVTelemetryRow& OutRow)
{
	TArray<FString> Columns;
	Line.ParseIntoArray(Columns, TEXT(","), false);

	// Minimum columns: MET, PosX, PosY, PosZ, RotX, RotY, RotZ, RotW, AccX, AccY, AccZ, VelX, VelY, VelZ = 14
	if (Columns.Num() < 14)
	{
		UE_LOG(LogRocketAR, Warning, TEXT("CSV line %d: expected >= 14 columns, got %d"), LineNumber, Columns.Num());
		return false;
	}

	// Parse with validation
	auto ParseDouble = [&](int32 Idx, double& Out) -> bool
	{
		if (Idx >= Columns.Num()) return false;
		const FString& Val = Columns[Idx].TrimStartAndEnd();
		if (Val.IsEmpty()) return false;
		Out = FCString::Atod(*Val);
		return !FMath::IsNaN(Out) && FMath::IsFinite(Out);
	};

	auto ParseFloat = [&](int32 Idx, float& Out) -> bool
	{
		if (Idx >= Columns.Num()) return false;
		const FString& Val = Columns[Idx].TrimStartAndEnd();
		if (Val.IsEmpty()) return false;
		Out = FCString::Atof(*Val);
		return !FMath::IsNaN(Out) && FMath::IsFinite(Out);
	};

	// MET
	if (!ParseDouble(0, OutRow.MET))
	{
		UE_LOG(LogRocketAR, Warning, TEXT("CSV line %d: invalid MET"), LineNumber);
		return false;
	}

	// Position (ECEF)
	double PX, PY, PZ;
	if (!ParseDouble(1, PX) || !ParseDouble(2, PY) || !ParseDouble(3, PZ))
	{
		UE_LOG(LogRocketAR, Warning, TEXT("CSV line %d: invalid position"), LineNumber);
		return false;
	}
	OutRow.Position = FVector(PX, PY, PZ);

	// Rotation (quaternion XYZW)
	double RX, RY, RZ, RW;
	if (!ParseDouble(4, RX) || !ParseDouble(5, RY) || !ParseDouble(6, RZ) || !ParseDouble(7, RW))
	{
		UE_LOG(LogRocketAR, Warning, TEXT("CSV line %d: invalid rotation"), LineNumber);
		return false;
	}
	OutRow.Rotation = FQuat(RX, RY, RZ, RW);

	// Normalize quaternion (warn if significantly off)
	const double QuatMag = OutRow.Rotation.Size();
	if (QuatMag < 0.9 || QuatMag > 1.1)
	{
		UE_LOG(LogRocketAR, Warning, TEXT("CSV line %d: quaternion magnitude %.3f (expected ~1.0)"), LineNumber, QuatMag);
	}
	if (QuatMag > 0.01)
	{
		OutRow.Rotation.Normalize();
	}

	// Acceleration (body frame)
	double AX, AY, AZ;
	if (!ParseDouble(8, AX) || !ParseDouble(9, AY) || !ParseDouble(10, AZ))
	{
		UE_LOG(LogRocketAR, Warning, TEXT("CSV line %d: invalid acceleration"), LineNumber);
		return false;
	}
	OutRow.Acceleration = FVector(AX, AY, AZ);

	// Velocity (ECEF)
	double VX, VY, VZ;
	if (!ParseDouble(11, VX) || !ParseDouble(12, VY) || !ParseDouble(13, VZ))
	{
		UE_LOG(LogRocketAR, Warning, TEXT("CSV line %d: invalid velocity"), LineNumber);
		return false;
	}
	OutRow.Velocity = FVector(VX, VY, VZ);

	// Engine thrust (remaining columns)
	for (int32 j = 14; j < Columns.Num(); ++j)
	{
		float ThrustVal;
		if (ParseFloat(j, ThrustVal))
		{
			OutRow.EngineThrustPercent.Add(FMath::Clamp(ThrustVal, 0.0f, 1.0f));
		}
	}

	return true;
}

bool ACSVTelemetryProvider::ValidateRows()
{
	if (Rows.Num() == 0)
	{
		UE_LOG(LogRocketAR, Error, TEXT("CSV has no valid data rows"));
		return false;
	}

	// Remove non-monotonic MET rows (would break Hermite interpolator binary search)
	int32 RemovedCount = 0;
	for (int32 i = Rows.Num() - 1; i >= 1; --i)
	{
		if (Rows[i].MET <= Rows[i - 1].MET)
		{
			UE_LOG(LogRocketAR, Warning, TEXT("CSV row %d removed: non-monotonic MET (%.3f <= %.3f)"),
				i, Rows[i].MET, Rows[i - 1].MET);
			Rows.RemoveAt(i);
			RemovedCount++;
		}
	}
	if (RemovedCount > 0)
	{
		UE_LOG(LogRocketAR, Warning, TEXT("CSV: removed %d non-monotonic rows"), RemovedCount);
	}

	UE_LOG(LogRocketAR, Log, TEXT("CSV validated: %d rows, MET range [%.1f, %.1f]"),
		Rows.Num(), Rows[0].MET, Rows.Last().MET);

	return true;
}

void ACSVTelemetryProvider::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsPlaying || !bCSVLoaded || Rows.Num() == 0)
	{
		return;
	}

	CurrentMET += DeltaTime * TimeScale;

	// Handle end of data
	if (CurrentMET > Rows.Last().MET)
	{
		if (bLooping)
		{
			CurrentMET = Rows[0].MET;
			CurrentRowIndex = 0;
		}
		else
		{
			CurrentMET = Rows.Last().MET;
			bIsPlaying = false;
		}
	}

	// Update current row index
	CurrentRowIndex = FindRowIndexForMET(CurrentMET);

	// Build interpolated data
	if (CurrentRowIndex >= 0 && CurrentRowIndex < Rows.Num())
	{
		const FCSVTelemetryRow& Row = Rows[CurrentRowIndex];

		// If we have a next row, interpolate between current and next
		if (CurrentRowIndex + 1 < Rows.Num())
		{
			const FCSVTelemetryRow& NextRow = Rows[CurrentRowIndex + 1];
			const double DeltaMET = NextRow.MET - Row.MET;
			const float Alpha = (DeltaMET > 0.0) ?
				static_cast<float>((CurrentMET - Row.MET) / DeltaMET) : 0.0f;

			CurrentData.VehiclePosition = FMath::Lerp(Row.Position, NextRow.Position, static_cast<double>(Alpha));
			CurrentData.VehicleVelocity = FMath::Lerp(Row.Velocity, NextRow.Velocity, static_cast<double>(Alpha));
			CurrentData.VehicleRotation = FQuat::Slerp(Row.Rotation, NextRow.Rotation, Alpha);
			CurrentData.VehicleAcceleration = FMath::Lerp(Row.Acceleration, NextRow.Acceleration, static_cast<double>(Alpha));

			// Lerp thrust
			const int32 ThrustCount = FMath::Max(Row.EngineThrustPercent.Num(), NextRow.EngineThrustPercent.Num());
			CurrentData.EngineThrustPercent.SetNum(ThrustCount);
			for (int32 i = 0; i < ThrustCount; ++i)
			{
				const float T0 = (i < Row.EngineThrustPercent.Num()) ? Row.EngineThrustPercent[i] : 0.0f;
				const float T1 = (i < NextRow.EngineThrustPercent.Num()) ? NextRow.EngineThrustPercent[i] : 0.0f;
				CurrentData.EngineThrustPercent[i] = FMath::Lerp(T0, T1, Alpha);
			}
		}
		else
		{
			CurrentData.VehiclePosition = Row.Position;
			CurrentData.VehicleVelocity = Row.Velocity;
			CurrentData.VehicleRotation = Row.Rotation;
			CurrentData.VehicleAcceleration = Row.Acceleration;
			CurrentData.EngineThrustPercent = Row.EngineThrustPercent;
		}

		CurrentData.MissionElapsedTime = CurrentMET;
		CurrentData.bTelemetryValid = true;
	}
}

int32 ACSVTelemetryProvider::FindRowIndexForMET(double MET) const
{
	// Binary search for the row just before or at the given MET
	int32 Low = 0;
	int32 High = Rows.Num() - 1;

	while (Low < High)
	{
		const int32 Mid = (Low + High + 1) / 2;
		if (Rows[Mid].MET <= MET)
		{
			Low = Mid;
		}
		else
		{
			High = Mid - 1;
		}
	}

	return Low;
}

// ITelemetryProvider
FTelemetryInputData ACSVTelemetryProvider::GetTelemetryData_Implementation() const
{
	return CurrentData;
}

bool ACSVTelemetryProvider::IsTelemetryAvailable_Implementation() const
{
	return bCSVLoaded && CurrentData.bTelemetryValid;
}

int32 ACSVTelemetryProvider::GetProviderPriority_Implementation() const
{
	return 100; // CSV takes highest priority when active
}

// Playback controls
void ACSVTelemetryProvider::Play()
{
	bIsPlaying = true;
	UE_LOG(LogRocketAR, Log, TEXT("CSV playback: Play (MET=%.1f, TimeScale=%.1fx)"), CurrentMET, TimeScale);
}

void ACSVTelemetryProvider::Pause()
{
	bIsPlaying = false;
	UE_LOG(LogRocketAR, Log, TEXT("CSV playback: Pause (MET=%.1f)"), CurrentMET);
}

void ACSVTelemetryProvider::SetTimeScale(float Scale)
{
	TimeScale = FMath::Clamp(Scale, 0.1f, 10.0f);
	UE_LOG(LogRocketAR, Log, TEXT("CSV playback: TimeScale = %.1fx"), TimeScale);
}

void ACSVTelemetryProvider::ScrubToTime(double MET)
{
	if (Rows.Num() == 0) return;
	CurrentMET = FMath::Clamp(MET, Rows[0].MET, Rows.Last().MET);
	CurrentRowIndex = FindRowIndexForMET(CurrentMET);
	UE_LOG(LogRocketAR, Log, TEXT("CSV playback: Scrub to MET=%.1f"), CurrentMET);
}

void ACSVTelemetryProvider::StepForward()
{
	if (CurrentRowIndex + 1 < Rows.Num())
	{
		CurrentRowIndex++;
		CurrentMET = Rows[CurrentRowIndex].MET;
	}
}

void ACSVTelemetryProvider::StepBack()
{
	if (CurrentRowIndex > 0)
	{
		CurrentRowIndex--;
		CurrentMET = Rows[CurrentRowIndex].MET;
	}
}

void ACSVTelemetryProvider::SetLooping(bool bLoop)
{
	bLooping = bLoop;
}

void ACSVTelemetryProvider::ResetPlayback()
{
	if (Rows.Num() > 0)
	{
		CurrentMET = Rows[0].MET;
		CurrentRowIndex = 0;
		bIsPlaying = false;
		CurrentData.bTelemetryValid = false;
		UE_LOG(LogRocketAR, Log, TEXT("CSV playback: Reset"));
	}
}
