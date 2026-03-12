# RocketAR User Guide

Complete guide to setting up, configuring, and operating RocketAR — a broadcast CG graphics engine that renders transparent text banner overlays synchronized to rocket telemetry.

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [Project Setup](#project-setup)
3. [Level Setup](#level-setup)
4. [Configuration Reference](#configuration-reference)
5. [CSV Playback Mode](#csv-playback-mode)
6. [Freeze-Frame Mode](#freeze-frame-mode)
7. [Banner System](#banner-system)
8. [Camera Configuration](#camera-configuration)
9. [Flight Event Detection](#flight-event-detection)
10. [HUD Overlay](#hud-overlay)
11. [Keyboard Controls](#keyboard-controls)
12. [Dev Visualization](#dev-visualization)
13. [Generating Test Data](#generating-test-data)
14. [DeckLink Output Setup](#decklink-output-setup)
15. [Client Integration](#client-integration)
16. [Deployment Workflow](#deployment-workflow)
17. [Troubleshooting](#troubleshooting)
18. [Performance Monitoring](#performance-monitoring)

---

## Getting Started

### Prerequisites

- Unreal Engine 5.7.3
- Visual Studio 2026
- Cesium for Unreal plugin (install from Epic Marketplace)
- (Optional) Blackmagic DeckLink 8K Pro + Desktop Video drivers

### First Run

1. Open `RocketAR.uproject` in UE 5.7
2. UE will compile the C++ plugin on first open
3. Open the map `Content/Maps/RocketAR_Main`
4. Verify an `ARocketARSetupActor` is placed in the level
5. Press **Play** — CSV telemetry plays back automatically with dev visualization

You should see: a blue Earth sphere, a white rocket cylinder climbing, and banners spawning at flight milestones.

---

## Project Setup

### Required Plugins

| Plugin | Source | Purpose |
|---|---|---|
| Cesium for Unreal | Epic Marketplace | ECEF coordinate conversion |
| ProceduralMeshComponent | Built-in (engine) | Procedural mesh generation |
| BlackmagicMedia | Built-in (engine) | DeckLink SDI output (Phase 2) |

### Project Structure

```
RocketAR/
├── RocketAR.uproject
├── Config/
│   ├── DefaultEngine.ini      ← Alpha output, FXAA, LWC settings
│   ├── DefaultInput.ini       ← Keyboard action mappings
│   └── DefaultGame.ini
├── Content/
│   ├── Maps/RocketAR_Main.umap
│   └── Data/SimulatedTelemetry.csv
├── Plugins/RocketAR/
│   └── Source/RocketAR/       ← C++ plugin (all systems)
├── Source/RocketARGame/       ← Minimal game module
└── Tools/
    ├── deploy_to_windows.sh   ← WSL → Windows deployment
    └── generate_telemetry.py  ← CSV trajectory generator
```

### Critical Engine Settings

These are pre-configured in `DefaultEngine.ini` and should not be changed:

| Setting | Value | Why |
|---|---|---|
| `r.PostProcessing.PropagateAlpha` | 1 | Alpha passes through post-processing |
| `r.SceneColorFormat` | 0 | 64-bit float RGBA (required for clean alpha) |
| `r.AntiAliasingMethod` | 1 | FXAA only — TAA causes alpha ghosting |
| `bUseLargeWorldCoordinates` | True | Rockets reach 185km altitude |
| Auto-exposure | Disabled | Prevents brightness flicker |
| Motion blur, bloom, DOF | All disabled | These corrupt alpha channel |

---

## Level Setup

The level should contain:

1. **ARocketARSetupActor** — the master orchestrator (required)
2. Nothing else — no sky, no fog, no ground plane, no lights (the setup actor spawns dev lights automatically)

The setup actor automatically spawns all other actors at BeginPlay:
- CesiumGeoreference (ECEF origin at launch pad)
- CineCameraActor (driven by camera manager)
- ACSVTelemetryProvider (if CSV mode enabled)
- ADevVisualizationActor (Earth + rocket)
- ARocketARHUD (canvas overlay)

---

## Configuration Reference

All parameters are on the `ARocketARSetupActor` Details panel. Changes during PIE take effect immediately (live-synced every frame).

### Launch Site

| Property | Default | Description |
|---|---|---|
| `LaunchPadLatitude` | 34.5811 | WGS84 latitude (degrees) |
| `LaunchPadLongitude` | -120.6257 | WGS84 longitude (degrees) |
| `LaunchPadAltitude` | 150.0 | Altitude above WGS84 ellipsoid (meters) |

Common launch sites:
- **KSC LC-39B:** 28.6272, -80.6208, 0
- **Vandenberg SLC-6:** 34.5811, -120.6257, 150
- **Wallops:** 37.8337, -75.4881, 0
- **Boca Chica:** 25.9972, -97.1558, 0

### Camera

| Property | Default | Description |
|---|---|---|
| `CameraMountOffset` | (0, 500, 4000) | Body-fixed offset from vehicle center (cm). Z = along rocket axis toward nose, Y = lateral. |
| `CameraMountRotation` | (-80, -10, 0) | Body-fixed rotation (degrees). Pitch = -80 looks back along rocket body. |
| `CameraOpticalRoll` | 45.0 | Roll around optical axis (degrees). Rotates the camera view. |
| `CameraHFOV` | 110.0 | Horizontal field of view (degrees, 1-180) |

The camera is attached to the rocket mount point so it moves rigidly with the vehicle. The mount offset places it near the nose looking down.

### Banner Geometry

| Property | Default | Description |
|---|---|---|
| `BannerWidth` | 10000.0 | Event banner width (cm = 100m) |
| `BannerHeight` | 10000.0 | Event banner height (cm = 100m) |
| `BannerRotationYaw` | 0.0 | Z-axis rotation (yaw) for event banners (degrees) |
| `BannerImage` | None | Background texture (PNG with alpha). Import with "UserInterface2D (RGBA)" compression and enable alpha. nullptr = solid color. |
| `BannerTextSize` | 200.0 | Text size for event banners (cm) |
| `BannerTextOffset` | (0, 0, 0) | Local offset of text from banner center (cm) |
| `MaxActiveBanners` | 20 | Maximum simultaneous banners before culling oldest |

### Banner Slide & Timing

| Property | Default | Description |
|---|---|---|
| `TriggerTimeOffset` | 0.0 | Delay between event detection and banner spawn (seconds) |
| `SlideSpeed` | 5000.0 | Banner slide velocity in local -Z (cm/s = 50 m/s) |
| `SlideDuration` | 10.0 | Time banner is visible before fade-out begins (seconds) |
| `BannerFadeInDuration` | 0.3 | Opacity ramp-up at spawn (seconds) |
| `BannerSpawnZOffset` | 8000.0 | Event banner spawn position above vehicle center (cm = 80m toward nose) |
| `MarkerSpawnZOffset` | 6000.0 | Altitude marker spawn position above vehicle center (cm = 60m toward nose) |
| `AnticipationSeconds` | 1.5 | How early to spawn banners before trigger time (seconds) |

**How spawn offset works:** Banners are attached to the rocket mount point. The Z offset places them above the vehicle center, near the nose. As the rocket ascends, the camera (also at the nose) "flies past" banners which slide down along the rocket body.

**How anticipation works:** When an event is detected, the banner is queued. Its spawn time is `now + TriggerTimeOffset - AnticipationSeconds`. With default values (offset=0, anticipation=1.5), banners begin animating 1.5 seconds before the event.

### Altitude Markers

| Property | Default | Description |
|---|---|---|
| `bShowAltitudeMarkers` | false | Enable altitude milestone banners |
| `AltitudeMarkerInterval` | 10000.0 | Spacing between markers (meters = 10km) |
| `MarkerWidth` | 4000.0 | Altitude marker width (cm = 40m) |
| `MarkerHeight` | 4000.0 | Altitude marker height (cm = 40m) |
| `MarkerColor` | (0.2, 0.8, 1.0, 1.0) | Marker color (cyan) |
| `MarkerRotationYaw` | 0.0 | Z-axis rotation (yaw) for altitude markers (degrees) |
| `MarkerImage` | None | Background texture for markers (PNG with alpha). Import with "UserInterface2D (RGBA)" compression. |
| `MarkerTextSize` | 150.0 | Text size for altitude markers (cm) |
| `MarkerTextOffset` | (0, 0, 0) | Local offset of text from marker center (cm) |
| `AltitudeMarkerAnticipation` | 2.0 | Predictive look-ahead (seconds). Uses `altitude + verticalVelocity * anticipation` to fire markers early. |

### Flight Event Config

| Property | Default | Description |
|---|---|---|
| `LiftoffAltitudeThreshold` | 1.0 | Meters to trigger liftoff |
| `MaxQRisingDuration` | 20.0 | Minimum rising Q duration before confirming Max-Q (seconds) |
| `MaxQDropPercent` | 0.05 | Fraction below peak to confirm Max-Q (5%) |
| `MaxQConfirmationWindow` | 1.0 | No higher Q in this window to confirm (seconds) |
| `ThrustOnThreshold` | 0.01 | Engine on/off threshold (0-1) |
| `SRBEngineCount` | 2 | Number of SRB engines in thrust array |
| `CoreEngineCount` | 4 | Number of core engines in thrust array |
| `AltitudeMarkerMinSpacing` | 0.0 | Minimum altitude between markers (meters) |
| `ReentryQThreshold` | 1000.0 | Dynamic pressure threshold for reentry (Pa) |
| `ChuteDeployAltitude` | 8000.0 | Chute deployment altitude (meters) |
| `SplashdownAltitude` | 10.0 | Splashdown threshold (meters) |

### Per-Event Text Offset Overrides

The `EventOverrides` array in `EventConfig` lets you adjust text positioning per built-in event. This is useful when certain events need their text offset adjusted independently of the global `BannerTextOffset`.

To use: expand **EventConfig → Event Overrides** in the Details panel, add an entry, select the `EventType`, enable `bOverrideTextOffset`, and set `TextOffsetOverride`. Custom events (defined in `EventConfig → Custom Events`) also support per-event text offset overrides via the same `bOverrideTextOffset` / `TextOffsetOverride` fields.

### Telemetry

| Property | Default | Description |
|---|---|---|
| `ExtrapolationTimeout` | 1.0 | Seconds of missing data before marking stale |
| `bUseCSVProvider` | true | Enable CSV playback mode |
| `CSVFilePath` | Data/SimulatedTelemetry.csv | Path to CSV file (relative to Content/) |

### HUD & Debug

| Property | Default | Description |
|---|---|---|
| `bShowHUDTelemetry` | true | Show MET/altitude/velocity readout |
| `bShowHUDEvents` | true | Show event announcements |
| `bShowDebugMessages` | true | Show on-screen BANNER:/ALTITUDE: messages |
| `bDevVisualization` | true | Show Earth sphere + rocket cylinder |

### Dev Visualization

| Property | Default | Description |
|---|---|---|
| `RocketHeight` | 98.0 | Rocket body height in meters (SLS Block 1 = 98m) |
| `RocketRadius` | 4.2 | Rocket body radius in meters (SLS Block 1 = 4.2m) |
| `bDevOpaqueBanners` | false | Opaque wireframe banners for debugging (no alpha/fade) |

### Dev Camera

| Property | Default | Description |
|---|---|---|
| `bUseDevCamera` | false | Enable dev inspection camera (parented to rocket) |
| `DevCameraOffset` | (0, 0, 15000) | Offset from rocket root (cm). Z = along rocket axis toward nose. |
| `DevCameraRotation` | (-90, 0, 0) | Rotation relative to rocket (default: looking down). |
| `DevCameraFOV` | 90.0 | Field of view (degrees, 1-180) |

The dev camera is useful for inspecting banner placement and rocket geometry from a fixed perspective relative to the rocket. Enable via setup actor or use `bUseDevCamera = true`.

### Freeze-Frame Mode

| Property | Default | Description |
|---|---|---|
| `bFreezeFrameMode` | false | Enable freeze-frame (no CSV playback) |
| `FreezeFrameAltitude` | 30000.0 | Rocket altitude (meters) |
| `FreezeFrameEventLabel` | "MAX Q" | Test banner label text |

---

## CSV Playback Mode

The default mode. Reads `Content/Data/SimulatedTelemetry.csv` and plays back at real time (adjustable).

### CSV Format

```
MET,PosX,PosY,PosZ,RotX,RotY,RotZ,RotW,AccX,AccY,AccZ,VelX,VelY,VelZ,Thrust1,...,ThrustN
```

| Column | Unit | Frame |
|---|---|---|
| MET | seconds | Negative = countdown |
| PosX/Y/Z | meters | ECEF |
| RotX/Y/Z/W | quaternion | ECEF (XYZW order) |
| AccX/Y/Z | m/s^2 | Body frame |
| VelX/Y/Z | m/s | ECEF |
| Thrust1-N | 0.0-1.0 | Per-engine (SRB, Core, Stage2) |

The included CSV has 6101 rows at 10Hz covering MET -10s to +600s with an SLS-like trajectory.

### Playback Controls

| Key | Action |
|---|---|
| Space | Play/Pause |
| Right Arrow | Skip forward 10s |
| Left Arrow | Skip backward 10s |
| ] | Speed up (+0.2x) |
| [ | Slow down (-0.2x) |
| R | Reset to T-0, destroy all banners |

---

## Freeze-Frame Mode

For visual tuning without playback. Places the rocket at a fixed altitude with a static banner.

1. On the setup actor, set `bFreezeFrameMode = true`
2. Set `FreezeFrameAltitude` (meters, e.g. 30000 for 30km)
3. Set `FreezeFrameEventLabel` (e.g. "MAX Q")
4. Press Play

The rocket appears at the configured altitude with one banner. The banner has `SlideSpeed = 0` so it stays in place. Adjust camera, banner geometry, and spawn offsets in the Details panel — changes apply live.

---

## Banner System

### How Banners Work

1. **Event fires** — `UFlightEventDetector` detects a milestone and broadcasts `OnFlightEvent`
2. **Queue** — `UBannerManager` creates a `FPendingBanner` with spawn time = `now + TriggerTimeOffset - AnticipationSeconds`
3. **Spawn** — When world time reaches `TriggerWorldTime`, the manager spawns an `ABannerActor`
4. **Attach** — Banner attaches to the rocket's mount point at a Z offset above vehicle center
5. **Slide** — Banner moves in local -Z (toward exhaust) at `SlideSpeed` cm/s
6. **Fade-in** — Opacity ramps from 0 to 1 over `FadeInDuration`
7. **Active** — Banner is fully visible for `SlideDuration` seconds
8. **Fade-out** — Opacity ramps from 1 to 0 over `FadeOutDuration`
9. **Destroy** — Banner is removed from the scene

### Spawn Offset (Nose Position)

Banners spawn at a configurable Z offset above the vehicle center:

- `BannerSpawnZOffset = 8000.0` (80m above center for event banners)
- `MarkerSpawnZOffset = 6000.0` (60m above center for altitude markers)

Since the camera is mounted near the nose looking down, this makes banners appear in the camera's view and "slide past" during ascent. Tune these values based on your camera position.

### Anticipation Timing

Banners spawn slightly before the trigger point:

- `AnticipationSeconds = 1.5` means banners begin animating 1.5s before the event
- Altitude markers use predictive look-ahead: `altitude + verticalVelocity * AltitudeMarkerAnticipation` to fire 2s before the rocket actually crosses the threshold

### Banner vs Marker Geometry

| Property | Event Banner | Altitude Marker |
|---|---|---|
| Width | 10000 cm (100m) | 4000 cm (40m) |
| Height | 10000 cm (100m) | 4000 cm (40m) |
| Color | Material default | Cyan (configurable) |
| Rotation yaw | BannerRotationYaw | MarkerRotationYaw |
| Background image | BannerImage | MarkerImage |
| Text size | 200 cm | 150 cm |
| Spawn Z offset | 8000 cm | 6000 cm |
| Debug label | "BANNER:" | "ALTITUDE:" |

### Max Banner Limit

When `MaxActiveBanners` (default 20) is reached, the oldest banner is force-faded to make room. This prevents performance degradation during long flights.

---

## Camera Configuration

The camera is body-fixed to the rocket — it moves and rotates with the vehicle.

### Mount Offset

`CameraMountOffset = (X, Y, Z)` in centimeters, relative to vehicle center:

- **X** = Forward/backward along rocket axis (rarely used)
- **Y** = Lateral offset (500 = 5m to the side)
- **Z** = Along rocket axis toward nose (4000 = 40m above center)

### Mount Rotation

`CameraMountRotation = (Pitch, Yaw, Roll)` in degrees:

- **Pitch = -80** = Looking steeply back along the rocket body
- **Yaw = -10** = Slight yaw offset
- **Roll = 0** = No body-frame roll

### Optical Roll

`CameraOpticalRoll = 45.0` rotates the image around the camera's looking direction. This simulates a physically rotated camera mount.

### Field of View

`CameraHFOV = 110.0` degrees horizontal. Match this to the actual on-board camera's specs for accurate alignment.

---

## Flight Event Detection

The system detects 15 SLS flight milestones. Each event (except altitude markers) fires once per flight (latched).

### Event List

| # | Event | Banner Label Example | When |
|---|---|---|---|
| 1 | Ignition | "IGNITION" | First engine thrust detected |
| 2 | Liftoff | "LIFTOFF" | Altitude > 1m |
| 3 | Mach 1 | "MACH 1 \| 340 m/s" | Speed exceeds sound |
| 4 | Max Q | "MAX Q \| 32,450 Pa" | Peak dynamic pressure confirmed |
| 5 | SRB Ignition | "SRB IGNITION" | SRB engines activate |
| 6 | SRB Separation | "SRB SEP \| 45 km" | SRB engines shut down |
| 7 | MECO | "MECO" | Core engines shut down |
| 8 | Stage Separation | "STAGE SEP" | 3s after MECO |
| 9 | 2nd Stage Ignition | "2ND STAGE IGN" | Upper stage lights |
| 10 | Fairing Jettison | "FAIRING SEP \| 105 km" | Alt > 100km, Q < 1Pa |
| 11 | SECO | "SECO" | Upper stage shuts down |
| 12 | Apogee | "APOGEE \| 185.2 km" | Vertical velocity crosses zero |
| 13 | Reentry | "REENTRY" | Q rises above threshold |
| 14 | Chute Deploy | "CHUTE DEPLOY" | Altitude drops below threshold |
| 15 | Splashdown | "SPLASHDOWN" | Near sea level after descent |
| * | Altitude Markers | "10 km", "20 km", etc. | Every N km during ascent |

### Engine Thrust Array

The thrust array indices map to engine groups:

```
Index:  [0]    [1]    [2]    [3]    [4]    [5]    [6]
Engine: SRB-1  SRB-2  Core-1 Core-2 Core-3 Core-4 Stage-2
        ←SRBEngineCount→ ←──CoreEngineCount──→ ←rest→
```

Adjust `SRBEngineCount` and `CoreEngineCount` for different vehicles.

---

## HUD Overlay

### Canvas HUD (ARocketARHUD)

Enabled by default. Shows:

- **Bottom-left corner:** MET, Altitude, Velocity
- **Top-center:** Event name with 5s display + 1s fade

Toggle with F1 or set `bShowHUDTelemetry` / `bShowHUDEvents` on the setup actor.

### Debug Messages

When `bShowDebugMessages = true`, on-screen messages appear when banners spawn:

```
BANNER: MAX Q at (x, y, z)
ALTITUDE: 10 km at (x, y, z)
```

### Status Colors

| Color | Meaning |
|---|---|
| Green | Telemetry active and fresh |
| Yellow | Predicted/extrapolating (1-3s stale) |
| Red | Signal loss (>3s stale) |

---

## Keyboard Controls

| Key | Action |
|---|---|
| **Space** | Play/Pause CSV playback |
| **Right Arrow** | Scrub forward 10 seconds |
| **Left Arrow** | Scrub backward 10 seconds |
| **]** | Increase time scale (+0.2x) |
| **[** | Decrease time scale (-0.2x) |
| **R** | Reset: destroy banners, restart from T-0 |
| **D** | Toggle dev visualization (Earth + rocket) |
| **F1** | Toggle HUD overlay |
| **F2** | Toggle performance stats (`stat RocketAR`) |
| **Tab** | Cycle through active banners |

---

## Dev Visualization

When `bDevVisualization = true`, the system renders:

- **Earth sphere** — Blue Marble at real scale, positioned via Cesium
- **Rocket cylinder** — configurable via `RocketHeight` / `RocketRadius` (SLS Block 1 defaults: 98m × 4.2m), at vehicle position, oriented to velocity
- **Directional light** — Sun-like illumination
- **Sky light** — Ambient fill

The rocket cylinder has an unscaled **mount point** at its base. Camera and banners attach to this point so they move rigidly with the rocket without being affected by the cylinder's visual scale.

Toggle with **D** key or set `bDevVisualization` on the setup actor.

---

## Generating Test Data

The Python script `Tools/generate_telemetry.py` generates synthetic SLS-like trajectories.

### Usage

```bash
# Default (KSC launch site)
python3 Tools/generate_telemetry.py

# Preset launch site
python3 Tools/generate_telemetry.py --site vandenberg

# Custom coordinates
python3 Tools/generate_telemetry.py --lat 25.9972 --lon -97.1558

# Custom output path
python3 Tools/generate_telemetry.py --output my_trajectory.csv
```

### Output

CSV at 10Hz, MET from -10s (countdown) to +600s (10 minutes). Simulates:
- Ignition and liftoff
- Gravity turn
- Mach 1 and Max Q
- SRB separation
- MECO and coast
- Second stage ignition and SECO
- Apogee

The default CSV is at `Content/Data/SimulatedTelemetry.csv` (6101 rows).

---

## DeckLink Output Setup

### Prerequisites

1. Install Blackmagic Desktop Video drivers and Desktop Video Setup utility
2. Install a DeckLink 8K Pro card
3. Enable the `BlackmagicMedia` plugin in UE (currently commented out in Build.cs — uncomment `WITH_BLACKMAGIC=1`)

### Configuration

1. In Blackmagic Desktop Video Setup:
   - Configure two SDI connectors as a fill/key output pair
   - Set output format (e.g. 1080p59.94 or 1080p60)
   - Connect tri-sync or black burst cable for genlock

2. In UE, create a `BlackmagicMediaOutput` asset in Content Browser

3. On the `URocketARMediaOutput` component (add to setup actor or configure separately):
   - Set `MediaOutputAssetPath` to the asset
   - Set `bAutoStart = true`
   - Set `bEnableGenlock = true` if using genlock
   - Set `bEnableTimecode = true` if embedding timecode

### Verification

On your broadcast switcher:
- Fill output: CG graphics (text banners)
- Key output: Alpha matte (white where graphics, black where transparent)
- Composite: CG overlaid on live camera feed
- Check: Clean alpha edges, no fringing, fully transparent background

---

## Client Integration

The client has two integration methods:

### Method A: ITelemetryProvider Interface (Recommended)

1. Create a Blueprint or C++ actor that implements `ITelemetryProvider`
2. Override three methods:
   - `GetTelemetryData()` — return `FTelemetryInputData` filled from their telemetry source
   - `IsTelemetryAvailable()` — return true when data is valid
   - `GetProviderPriority()` — return 75 (higher than setup actor's 50, lower than CSV's 100)
3. Place the actor in `RocketAR_Main`
4. Disable CSV mode: set `bUseCSVProvider = false` on setup actor
5. The telemetry subsystem auto-discovers the highest-priority provider

### Method B: Direct Blueprint Variables

1. On `ARocketARSetupActor`, wire Blueprint variables directly:
   - `InputVehiclePosition` (ECEF meters)
   - `InputVehicleRotation` (ECEF quaternion)
   - `InputVehicleVelocity` (ECEF m/s)
   - `InputVehicleAcceleration` (body-frame m/s^2)
   - `InputEngineThrustPercent` (array, 0-1)
   - `InputMissionElapsedTime` (seconds)
   - `bInputTelemetryValid` (set true when data is good)
2. The setup actor itself implements `ITelemetryProvider` at priority 50

### Telemetry Contract

All positions and velocities must be in **ECEF (Earth-Centered Earth-Fixed)** frame. The engine thrust array must follow the format: `[SRB1, SRB2, Core1, ..., CoreN, Stage2]`.

---

## Deployment Workflow

### WSL to Windows

The project is developed in WSL2 and deployed to the Windows UE project:

```bash
# Deploy with default destination
./Tools/deploy_to_windows.sh

# Deploy to custom path
./Tools/deploy_to_windows.sh "/mnt/c/Users/me/Documents/Unreal Projects/RocketAR"
```

The script:
1. Cleans `Intermediate/` and `Binaries/` directories (forces full recompile)
2. Copies: `.uproject`, `Config/`, `Source/`, `Plugins/`, `Content/Data/`

After deploying, open the project in UE and it will recompile automatically.

---

## Troubleshooting

### Banners Not Appearing

1. Check Output Log for `FLIGHT EVENT:` messages — confirms events are firing
2. Check for `BannerManager: Spawned` messages — confirms banners are created
3. Verify banner dimensions (`BannerWidth`, `BannerHeight`) are non-zero
4. If using altitude markers, verify `bShowAltitudeMarkers = true`
5. Check `MaxActiveBanners` hasn't been set to 0

### Banners Not Visible in Camera

1. Check `BannerSpawnZOffset` — banners may be spawning outside the camera's FOV
2. Verify `CameraMountOffset` Z value — camera should be above banner spawn position
3. Use freeze-frame mode to position banners statically and adjust camera
4. Check `SlideSpeed` — banners may be sliding out of view too quickly

### Alpha Not Clean

1. Verify `r.PostProcessing.PropagateAlpha = 1` in Project Settings > Engine > Rendering
2. Check anti-aliasing is FXAA (not TAA) — TAA causes alpha ghosting
3. Ensure no sky, fog, or ground objects in the level
4. Check that auto-exposure is disabled
5. Verify all post-process effects are disabled (bloom, DOF, motion blur, vignette)

### Banner Text Invisible in Alpha Output

Banner text uses a `FontSampleParameter`-based translucent material with SDF thresholding. If text is invisible in the fill/key output:
1. Ensure the font used is an **offline cached** SDF font (not a runtime-generated font)
2. The default engine font `/Engine/EngineFonts/RobotoDistanceField` works for compilation
3. Check that the text material has `MaterialDomain = Surface` and `BlendMode = Translucent`

### No Telemetry / CSV Not Loading

1. Check Output Log for `CSV loaded: N rows` or error messages
2. Verify `CSVFilePath` is correct (relative to `Content/`)
3. Verify `bUseCSVProvider = true` and `bFreezeFrameMode = false`
4. Check for `Telemetry provider discovered` in Output Log

### Events Not Firing

1. Check `EventConfig` thresholds on setup actor
2. Verify engine counts match CSV data: `SRBEngineCount + CoreEngineCount + 1 <= thrust columns`
3. Check Output Log for individual event check messages
4. Events are latched — they fire only once per flight. Press R to reset.

### DeckLink Issues

1. `Hardware not detected` — verify card in Desktop Video Setup
2. `Capture failed` — check output format matches switcher input format
3. Alpha fringing — verify FXAA, not TAA
4. Auto-restart handles transient failures (default 2s delay)

### Compile Errors After Deploy

1. Delete `Intermediate/` and `Binaries/` in the Windows project
2. Re-deploy: `./Tools/deploy_to_windows.sh`
3. If Cesium errors: ensure Cesium for Unreal is installed from Marketplace
4. If Blackmagic errors: the plugin is disabled by default (`WITH_BLACKMAGIC=0`); this is expected

---

## Performance Monitoring

### Stat Groups

Press **F2** or run `stat RocketAR` in the console to see:

| Stat | Description | Typical |
|---|---|---|
| TelemetryTick | Full subsystem tick time | < 0.1ms |
| Interpolation | Hermite interpolation | < 0.05ms |
| DerivedValues | Altitude/Mach/G computation | < 0.05ms |
| EventDetection | All 15 event checks | < 0.05ms |
| ActiveBanners | Current banner count | 0-20 |

### Performance Tips

- Keep `MaxActiveBanners` at 20 or below
- Altitude markers can generate many banners at high marker intervals — adjust `AltitudeMarkerInterval` if performance is a concern
- Dev visualization (Earth sphere) is the most expensive visual element — disable for production
- The canvas HUD is lightweight; the UMG widget has slightly more overhead
