# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Project Overview

**RocketAR** is a broadcast CG graphics engine built as a complete **Unreal Engine 5.7 project** (not a plugin). It receives rocket telemetry, detects flight milestones, and renders transparent text banner overlays. Output is via a DeckLink 8K Pro's fill/key SDI outputs to a broadcast switcher — UE never sees the video feed.

**Key architectural constraint:** UE renders CG graphics with alpha only. The client's switcher composites them over the live rocket camera. No video enters UE.

---

## Target Project Structure

```
RocketAR/
├── RocketAR.uproject
├── Config/DefaultEngine.ini          ← alpha propagation, LWC, FXAA
├── Content/
│   ├── Maps/RocketAR_Main.umap
│   ├── Blueprints/
│   │   ├── BP_RocketARSetup.uasset   ← all configurable parameters exposed here
│   │   ├── BP_TelemetryInterface.uasset
│   │   └── BP_BannerActor.uasset
│   ├── Materials/
│   ├── Fonts/ShareTechMono.uasset
│   ├── Data/SimulatedTelemetry.csv
│   └── Dev/                          ← dev-only assets (Earth sphere, rocket cylinder)
└── Plugins/RocketAR/Source/RocketAR/ ← C++ plugin with core subsystems
```

---

## Architecture

### Data Flow

```
Client's Blueprint Plugin
  → ITelemetryProvider (UInterface) / CSV fallback
  → UTelemetrySubsystem (world subsystem)
    → ECEF → ENU conversion (Cesium for Unreal)
    → Hermite interpolation (5–10Hz → 60fps)
    → UFlightEventDetector
    → UBannerManager
  → DeckLink 8K Pro fill/key SDI output
```

### Core C++ Systems (in `Plugins/RocketAR/`)

| Class | Responsibility |
|---|---|
| `ITelemetryProvider` | UInterface the client implements; defines `GetTelemetryData()` returning `FTelemetryInputData` |
| `FTelemetryInputData` | USTRUCT: VehiclePosition (ECEF), VehicleRotation (Quat), VehicleVelocity, VehicleAcceleration, EngineThrustPercent[], MissionElapsedTime, bTelemetryValid |
| `UTelemetrySubsystem` | World subsystem; auto-discovers the provider actor at runtime; runs ECEF conversion and interpolation each frame |
| `UCSVTelemetryProvider` | Dev/test provider implementing `ITelemetryProvider`; reads client's CSV with MET-timed playback |
| `UFlightEventDetector` | Derives altitude/Mach/G-force/dynamic pressure from raw telemetry; fires 15 SLS flight events + altitude markers |
| `UBannerManager` | Spawns, updates (Earth-fixed ECEF positioning via Cesium), and culls banner actors |
| `ABannerActor` | 120° cylindrical arc mesh; emissive text material; camera-facing orientation; spawn animation |

### Coordinate System

All positions use **ECEF (Earth-Centered Earth-Fixed)**. Conversion to UE world space goes through **Cesium for Unreal's** `ACesiumGeoreference::TransformEcefToUnreal()`. The reference origin (launch pad lat/lon/alt) is a configurable parameter on `BP_RocketARSetup`.

ENU→UE axis mapping: `X_ue = East × 100`, `Y_ue = -North × 100`, `Z_ue = Up × 100`

**UE5 Large World Coordinates (LWC) must be enabled** — rockets reach ~185km altitude.

### Interpolation

Telemetry arrives at 5–10Hz; render runs at 60fps. Uses **cubic Hermite interpolation** for position (using velocity as tangent), **quaternion SLERP** for rotation, linear for scalars. Extrapolates up to 1 second on late data; beyond that, freezes and sets `bTelemetryStale = true`.

### Alpha Output Configuration

Critical `DefaultEngine.ini` settings:
- `r.PostProcessing.PropagateAlpha = 1`
- `r.SceneColorFormat = 0` (PF_FloatRGBA)
- Anti-aliasing: **FXAA only** (TAA causes alpha ghosting)
- Auto-exposure: **disabled**
- Disabled post-process: Motion Blur, Bloom, DOF, Vignette, Chromatic Aberration, Film Grain
- No background geometry: no sky, no fog, no ground plane

### Dev Visualization Mode

Controlled by a single bool `bDevVisualization` on the setup actor. Adds: Earth sphere (Blue Marble texture), rocket cylinder at vehicle position, camera preview. All hidden in production output.

---

## Implementation Phases

**Phase 1** (primary deliverable):
1. Project setup + alpha output configuration
2. `ITelemetryProvider` interface + CSV playback provider + Cesium ECEF conversion + Hermite interpolation
3. Flight event detector (15 SLS events + altitude markers)
4. Banner system (geometry, text, Earth-fixed positioning, spawn animation, lifecycle)
5. CG camera (ECEF position + quaternion → UE transform, configurable mounting offset + FOV)
6. Integration testing against client's CSV data

**Phase 2** (production hardening):
1. DeckLink 8K Pro fill/key SDI output via `BlackmagicMediaOutput` + `MediaCapture`
2. Genlock via `BlackmagicCustomTimeStep`; timecode via `BlackmagicTimecodeProvider`
3. Live telemetry integration (client wires their plugin to `ITelemetryProvider`)
4. Camera calibration (focal length, sensor size, mounting offset fine-tuning)
5. Production failure handling + operator controls + HUD overlay

---

## Key Design Decisions

- **UInterface over raw variables**: `ITelemetryProvider` lets the client implement on any actor without modifying our code. The subsystem auto-discovers the provider at runtime.
- **Cesium for Unreal** for ECEF conversion — eliminates custom geodetic math, handles precision and origin shifting automatically.
- **CSV provider implements the same interface as live provider** — the entire pipeline (event detection, banners, rendering) is testable without the client's plugin.
- **Banner position is Earth-fixed ECEF** — stored at spawn, re-transformed each frame via Cesium. As the rocket moves, banners naturally recede in the camera view.
- **Deliverable is a complete UE project**, not a plugin. The client opens it, enables their telemetry plugin, implements `GetTelemetryData`, and places their actor in the level.

---

## Client Integration Contract

The client's only required work:
1. Implement `ITelemetryProvider` on their plugin actor → fill `FTelemetryInputData` from their internal data
2. Place the actor in `RocketAR_Main.umap`
3. Enter camera specs (focal length, sensor size, mounting offset) in `BP_RocketARSetup`
4. Configure DeckLink output connector in Blackmagic Desktop Video Setup

---

## Design Documents

- `docs/RocketAR_Phase1_Design_v2.md` — full Phase 1 spec (telemetry interface, ECEF conversion, interpolation, alpha output, banner system, CSV reader)
- `docs/RocketAR_Phase2_Design_v2.md` — Phase 2 spec (DeckLink fill/key, genlock, live telemetry integration, camera calibration, production hardening)
- `docs/RocketAR_PRD_v2.docx` — original PRD
