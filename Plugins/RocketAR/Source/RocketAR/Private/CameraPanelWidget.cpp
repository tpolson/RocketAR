#include "CameraPanelWidget.h"
#include "RocketARSetupActor.h"
#include "Components/SpinBox.h"

void UCameraPanelWidget::Refresh(ARocketARSetupActor* InSetupActor)
{
	if (!InSetupActor) return;
	if (OffsetX)     OffsetX->SetValue(InSetupActor->CameraMountOffset.X);
	if (OffsetY)     OffsetY->SetValue(InSetupActor->CameraMountOffset.Y);
	if (OffsetZ)     OffsetZ->SetValue(InSetupActor->CameraMountOffset.Z);
	if (Pitch)       Pitch->SetValue(InSetupActor->CameraMountRotation.Pitch);
	if (Yaw)         Yaw->SetValue(InSetupActor->CameraMountRotation.Yaw);
	if (Roll)        Roll->SetValue(InSetupActor->CameraMountRotation.Roll);
	if (OpticalRoll) OpticalRoll->SetValue(InSetupActor->CameraOpticalRoll);
	if (HFOV)        HFOV->SetValue(InSetupActor->CameraHFOV);
	if (RigIndex)    RigIndex->SetValue(static_cast<float>(InSetupActor->ActiveCameraRigIndex));
}

void UCameraPanelWidget::Apply(ARocketARSetupActor* InSetupActor)
{
	if (!InSetupActor) return;

	if (OffsetX) InSetupActor->CameraMountOffset.X = OffsetX->GetValue();
	if (OffsetY) InSetupActor->CameraMountOffset.Y = OffsetY->GetValue();
	if (OffsetZ) InSetupActor->CameraMountOffset.Z = OffsetZ->GetValue();
	if (Pitch)   InSetupActor->CameraMountRotation.Pitch = Pitch->GetValue();
	if (Yaw)     InSetupActor->CameraMountRotation.Yaw   = Yaw->GetValue();
	if (Roll)    InSetupActor->CameraMountRotation.Roll  = Roll->GetValue();
	if (OpticalRoll) InSetupActor->CameraOpticalRoll = OpticalRoll->GetValue();
	if (HFOV)        InSetupActor->CameraHFOV       = FMath::Clamp(HFOV->GetValue(), 1.0f, 180.0f);

	if (RigIndex)
	{
		const int32 NewIdx = FMath::Max(0, FMath::RoundToInt(RigIndex->GetValue()));
		if (NewIdx != InSetupActor->ActiveCameraRigIndex)
		{
			InSetupActor->SetActiveCameraRig(NewIdx);
		}
	}
}
