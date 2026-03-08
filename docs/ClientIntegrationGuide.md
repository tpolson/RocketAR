# Client Integration Guide

## Overview

RocketAR receives telemetry from your proprietary Blueprint plugin and renders CG text banner overlays. This guide covers how to connect your telemetry data to RocketAR.

## Two Integration Methods

### Method A: ITelemetryProvider Interface (Recommended)

Your plugin's actor implements the `ITelemetryProvider` interface. RocketAR auto-discovers it at runtime.

1. Open the RocketAR project in Unreal Engine 5.7
2. Enable your telemetry plugin
3. On your plugin's main actor, go to **Class Settings → Interfaces → Add** and add `TelemetryProvider`
4. In your actor's Blueprint graph, implement the **GetTelemetryData** event:
   - Create a `FTelemetryInputData` struct
   - Fill all fields from your internal telemetry data
   - Return the struct
5. Implement **IsTelemetryAvailable** — return `true` when your telemetry is valid
6. Implement **GetProviderPriority** — return `75` (default for external providers)
7. Place your actor in the `RocketAR_Main` level
8. RocketAR's subsystem will auto-discover your actor — no wiring needed

### Method B: Direct Blueprint Variables (Simpler)

Wire your Blueprint plugin's output directly to exposed variables on the `ARocketARSetupActor`.

1. In your Blueprint, get a reference to the `ARocketARSetupActor` in the level
2. Each tick, set these variables on the setup actor:
   - `InputVehiclePosition` (FVector, ECEF meters)
   - `InputVehicleRotation` (FQuat, ECEF frame)
   - `InputVehicleVelocity` (FVector, ECEF m/s)
   - `InputVehicleAcceleration` (FVector, body frame m/s²)
   - `InputEngineThrustPercent` (float array, 0.0–1.0)
   - `InputMissionElapsedTime` (double, seconds)
   - `bInputTelemetryValid` (bool)
3. Or call `SetTelemetryData()` with a filled `FTelemetryInputData` struct

## Telemetry Data Contract

| Field | Type | Units | Frame |
|---|---|---|---|
| VehiclePosition | FVector | meters | ECEF |
| VehicleRotation | FQuat (XYZW) | quaternion | ECEF |
| VehicleVelocity | FVector | m/s | ECEF |
| VehicleAcceleration | FVector | m/s² | Body |
| EngineThrustPercent | float[] | 0.0–1.0 | — |
| MissionElapsedTime | double | seconds | — |
| bTelemetryValid | bool | — | — |

### Engine Thrust Array Order

The engine thrust array should be ordered: `[SRB1, SRB2, Core1, Core2, Core3, Core4, Stage2]`

This order matches the default `FFlightEventConfig` which expects:
- Engines 0–1: SRBs
- Engines 2–5: Core stage
- Engine 6: Second stage

If your vehicle has a different configuration, adjust `FFlightEventConfig` on the setup actor.

## Camera Configuration

Enter your physical camera specs on the `ARocketARSetupActor`:

| Parameter | Description |
|---|---|
| `CameraMountOffset` | Position offset from rocket center (cm, body frame) |
| `CameraMountRotation` | Rotation offset from rocket body axis (degrees) |
| `CameraHFOV` | Horizontal field of view matching your physical camera |

## Launch Site

Set the launch pad coordinates on the setup actor:
- `LaunchPadLatitude` (degrees, e.g., 28.5729 for KSC)
- `LaunchPadLongitude` (degrees, e.g., -80.6490 for KSC)
- `LaunchPadAltitude` (meters above WGS84 ellipsoid)

## Testing with CSV

Before connecting your live plugin:
1. Set `bUseCSVProvider = true` on the setup actor
2. Play in Editor — the CSV provider plays back simulated SLS data
3. Verify banners spawn at correct positions and events fire at expected times
4. When ready, set `bUseCSVProvider = false` and place your provider actor

## DeckLink Output

See the Configuration Reference for DeckLink 8K Pro fill/key setup.
