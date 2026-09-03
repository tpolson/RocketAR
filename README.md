# RocketAR

Real-time, telemetry-driven AR graphics for rocket launch broadcasts, built on Unreal Engine 5.7.

RocketAR takes live vehicle telemetry (ECEF position, attitude, velocity, acceleration, engine state), detects flight milestones, and renders Earth-fixed banner graphics at the vehicle's true position. The scene renders alpha-only and leaves a Blackmagic DeckLink 8K Pro as fill and key over 12G-SDI, so a broadcast switcher composites it over the live camera. No video enters Unreal.

**Status:** working prototype. The full pipeline runs end to end against a recorded SLS-class ascent, and fill/key output is verified on DeckLink hardware. Live telemetry connects through a Blueprint-implementable interface.

## What it does

- **Telemetry in.** `ITelemetryProvider` interface for live data at 5–10 Hz. Cubic Hermite interpolation with velocity tangents and quaternion SLERP to 60 fps. Extrapolates up to 1 s on late packets, then freezes and flags stale.
- **Derived values.** Altitude, Mach, dynamic pressure, and G-load from a US Standard Atmosphere 1976 model.
- **Flight events.** Ignition, liftoff, Mach 1, Max Q, SRB ignition/separation, MECO, stage separation, second-stage ignition, fairing jettison, SECO, apogee, reentry, chute deploy, splashdown, predictive altitude markers, and user-defined threshold events. All thresholds are configurable properties.
- **Georeferenced graphics.** Banners stored in ECEF and re-transformed each frame through Cesium for Unreal, so they stay fixed in the sky as the vehicle climbs past. Procedural arc meshes, SDF text tuned for a clean key matte, trajectory-aligned spawn and slide animation.
- **Broadcast out.** DeckLink fill/key at 1080p or 2160p, 59.94 or 60. Genlock via `BlackmagicCustomTimeStep`, VITC timecode, alpha-propagation render config (FXAA only, exposure and post effects off).
- **Camera.** Vehicle-mounted POV from ECEF position and attitude with a configurable body-fixed mount offset.
- **Rehearsal.** CSV provider with pause, scrub, and time-scale controls. Python generator for synthetic ascents from KSC, Vandenberg, Wallops, or Boca Chica.

## Requirements

- Unreal Engine 5.7 and Visual Studio 2026
- [Cesium for Unreal](https://cesium.com/platform/cesium-for-unreal/) (Fab)
- Optional: Blackmagic DeckLink 8K Pro with Desktop Video drivers

Enabled plugins: RocketAR, CesiumForUnreal, ProceduralMeshComponent, BlackmagicMedia, MediaIOFramework.

## Quick start

1. Open `RocketAR.uproject`. The C++ plugin compiles on first open.
2. Open `Content/Maps/RocketAR_Main` and confirm an `ARocketARSetupActor` is in the level.
3. Press **Play**. The bundled CSV plays back automatically with dev visualization on.

| Key | Action |
|---|---|
| Space | Pause / resume |
| ← / → | Scrub −10 s / +10 s |
| [ / ] | Time scale down / up |
| R | Reset playback |
| D | Toggle dev visualization |
| F1 / F2 | Toggle HUD / stats |
| F4 | Toggle DeckLink output |
| Tab | Cycle banners |

## Feeding live telemetry

Implement `ITelemetryProvider` on any actor (Blueprint or C++), return an `FTelemetryInputData` from `GetTelemetryData`, and place the actor in the level. The subsystem discovers providers by priority. Alternatively, set the telemetry variables directly on `ARocketARSetupActor`. See `docs/ClientIntegrationGuide.md`.

## Documentation

| Document | Contents |
|---|---|
| `docs/UserGuide.md` | Setup, configuration, operation, troubleshooting |
| `docs/OperatorManual.md` | On-air controls and event table |
| `docs/ClientIntegrationGuide.md` | Connecting a live telemetry source |
| `docs/ConfigurationReference.md` | Every tunable property |
| `docs/TechnicalReference.md` | Full C++ API |
| `docs/CSVFormat.md` | Telemetry CSV schema |
| `docs/RocketAR_Phase1_Design_v2.md`, `docs/RocketAR_Phase2_Design_v2.md` | Design rationale |

## Layout

```
Plugins/RocketAR/Source/RocketAR/        Runtime module: telemetry, events, banners, camera, DeckLink output, HUD
Plugins/RocketAR/Source/RocketAREditor/  Editor module: Details-panel customization
Plugins/RocketAR/Source/RocketAR/Private/Tests/  Automation tests (detector, interpolator, atmosphere, CSV)
Config/DefaultEngine.ini                 Alpha output, FXAA, LWC
Content/Data/SimulatedTelemetry.csv      Sample 10 Hz ascent, T-10 to T+600
Tools/generate_telemetry.py              Synthetic telemetry generator
```

## Roadmap

- Public sample map and content assets committed to the repo
- Live transports: UDP/JSON, WebSocket, Live Link
- Generic vehicle profiles
- Lens distortion, camera calibration, FreeD tracked cameras, switchable views
- NDI and Spout alpha output for teams without DeckLink hardware

## License

Apache License 2.0. See `LICENSE`.

Copyright 2026 Artists & Algorithms.
