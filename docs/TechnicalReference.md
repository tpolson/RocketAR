# RocketAR Technical Reference

Complete API and architecture reference for the RocketAR broadcast CG graphics engine.

**Engine:** Unreal Engine 5.7.3
**Plugin:** `Plugins/RocketAR/Source/RocketAR/`
**Game Module:** `Source/RocketARGame/` (minimal boilerplate)

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Data Flow](#data-flow)
3. [Coordinate Systems](#coordinate-systems)
4. [Enumerations](#enumerations)
5. [Structs](#structs)
6. [Interfaces](#interfaces)
7. [Delegates](#delegates)
8. [Core Classes](#core-classes)
9. [Banner System](#banner-system)
10. [Camera System](#camera-system)
11. [Flight Event Detection](#flight-event-detection)
12. [Atmosphere Model](#atmosphere-model)
13. [HUD System](#hud-system)
14. [Dev Visualization](#dev-visualization)
15. [Media Output](#media-output)
16. [Input System](#input-system)
17. [Build Configuration](#build-configuration)
18. [Stat Groups](#stat-groups)

---

## Architecture Overview

RocketAR is a complete UE5 project (not a plugin) that renders transparent CG graphics for broadcast compositing. It receives rocket telemetry via a UInterface, detects flight milestones, and renders text banners attached to the rocket's trajectory. Output goes through a DeckLink 8K Pro's fill/key SDI outputs.

**Key constraint:** UE renders CG graphics with alpha only. No video enters UE. The client's broadcast switcher composites the CG over their live camera feed.

### Plugin Dependencies

| Module | Purpose |
|---|---|
| `CesiumForUnreal` | ECEF-to-UE coordinate conversion (optional, WITH_CESIUM) |
| `ProceduralMeshComponent` | Procedural arc mesh generation |
| `BlackmagicMedia` | DeckLink fill/key SDI output (deferred, WITH_BLACKMAGIC) |
| `CinematicCamera` | CineCameraActor for configurable FOV |
| `UMG`, `Slate`, `SlateCore` | HUD widget system |

---

## Data Flow

```
ITelemetryProvider (client plugin or CSV)
  │
  ▼
UTelemetrySubsystem (world subsystem)
  ├─ Provider auto-discovery (highest priority wins)
  ├─ UHermiteInterpolator (5-10Hz → 60fps)
  ├─ ECEF → UE conversion (Cesium)
  ├─ Derived values (altitude, Mach, G, Q, vertical velocity)
  └─ Broadcasts OnTelemetryUpdated
       │
       ▼
  ARocketARSetupActor (master orchestrator)
  ├─ UFlightEventDetector → detects 15 events + altitude markers
  │    └─ Broadcasts OnFlightEvent
  ├─ UBannerManager → queues/spawns ABannerActor
  ├─ URocketARCameraManager → drives CineCameraActor
  ├─ ADevVisualizationActor → Earth + rocket visualization
  └─ ARocketARHUD → canvas HUD overlay
```

---

## Coordinate Systems

| Frame | Description |
|---|---|
| **ECEF** | Earth-Centered Earth-Fixed. All raw telemetry positions/velocities. |
| **UE World** | Cesium-converted, Large World Coordinates enabled. Origin at launch pad. |
| **Local Body** | Relative to vehicle center. Z = nose direction (up along rocket axis). |

**ENU-to-UE mapping:** `X_ue = East * 100`, `Y_ue = -North * 100`, `Z_ue = Up * 100`

**WGS84 constants:** Semi-major axis = 6,378,137 m, eccentricity^2 = 0.00669437999014

---

## Enumerations

### EFlightEvent
**File:** `Public/FlightEventTypes.h`

| Value | Display Name |
|---|---|
| `Ignition` | Ignition |
| `Liftoff` | Liftoff |
| `Mach1` | Mach 1 |
| `MaxQ` | Max Q |
| `SRBIgnition` | SRB Ignition |
| `SRBSeparation` | SRB Separation |
| `MECO` | MECO |
| `StageSeparation` | Stage Separation |
| `SecondStageIgnition` | Second Stage Ignition |
| `FairingJettison` | Fairing Jettison |
| `SecondStageCutoff` | SECO |
| `Apogee` | Apogee |
| `ReentryStart` | Reentry Start |
| `ChuteDeployment` | Chute Deployment |
| `Splashdown` | Splashdown |
| `AltitudeMarker` | Altitude Marker |

### EBannerState
**File:** `Public/BannerActor.h`

| Value | Description |
|---|---|
| `SpawnAnimation` | Fade-in animation in progress |
| `Active` | Fully visible, sliding |
| `FadeOut` | Fade-out animation in progress |
| `Destroyed` | Pending garbage collection |

---

## Structs

### FTelemetryInputData
**File:** `Public/TelemetryTypes.h` | BlueprintType

Raw telemetry from the client's provider or CSV playback.

| Field | Type | Default | Description |
|---|---|---|---|
| `VehiclePosition` | `FVector` | Zero | ECEF position (meters) |
| `VehicleRotation` | `FQuat` | Identity | ECEF-frame quaternion (XYZW) |
| `VehicleVelocity` | `FVector` | Zero | ECEF velocity (m/s) |
| `VehicleAcceleration` | `FVector` | Zero | Body-frame acceleration (m/s^2) |
| `EngineThrustPercent` | `TArray<float>` | Empty | Per-engine thrust 0.0-1.0 |
| `MissionElapsedTime` | `double` | 0.0 | Seconds (negative = countdown) |
| `bTelemetryValid` | `bool` | false | Data validity flag |

### FProcessedTelemetryData
**File:** `Public/TelemetryTypes.h` | BlueprintType

Processed telemetry with all derived values and UE-space transforms.

| Field | Type | Description |
|---|---|---|
| `RawData` | `FTelemetryInputData` | Original input data |
| `AltitudeASL` | `double` | Meters above sea level (WGS84) |
| `VelocityMagnitude` | `double` | Speed (m/s) |
| `MachNumber` | `double` | Velocity / speed of sound |
| `GForce` | `double` | |acceleration| / 9.80665 |
| `DynamicPressurePa` | `double` | 0.5 * rho * v^2 (Pascals) |
| `VerticalVelocity` | `double` | Upward velocity (m/s) |
| `bAnyEngineActive` | `bool` | Any engine > 1% thrust |
| `UEPosition` | `FVector` | UE world space position (cm) |
| `UERotation` | `FQuat` | UE world space rotation |
| `bTelemetryStale` | `bool` | True if extrapolating beyond timeout |
| `StaleDurationSeconds` | `float` | How long data has been stale |
| `VehicleECEFPosition` | `FVector` | ECEF for banner placement |

### FFlightEventConfig
**File:** `Public/FlightEventTypes.h` | BlueprintType

Data-driven event detection thresholds.

| Field | Type | Default | Description |
|---|---|---|---|
| `LiftoffAltitudeThreshold` | `float` | 1.0 | Meters to trigger liftoff |
| `MaxQRisingDuration` | `float` | 20.0 | Min rising duration (seconds) |
| `MaxQDropPercent` | `float` | 0.05 | Fraction drop to confirm (5%) |
| `MaxQConfirmationWindow` | `float` | 1.0 | Confirmation window (seconds) |
| `ThrustOnThreshold` | `float` | 0.01 | Engine on/off threshold |
| `SRBEngineCount` | `int32` | 2 | SRB engines in thrust array |
| `CoreEngineCount` | `int32` | 4 | Core engines in thrust array |
| `AltitudeMarkerInterval` | `float` | 10000.0 | Marker spacing (meters) |
| `AltitudeMarkerMinSpacing` | `float` | 5000.0 | Minimum spacing (meters) |
| `AltitudeMarkerAnticipation` | `float` | 2.0 | Predictive look-ahead (seconds) |
| `ReentryQThreshold` | `float` | 1000.0 | Reentry Q threshold (Pa) |
| `ChuteDeployAltitude` | `float` | 8000.0 | Chute deploy altitude (meters) |
| `SplashdownAltitude` | `float` | 10.0 | Splashdown threshold (meters) |

### FFlightEventData
**File:** `Public/FlightEventTypes.h` | BlueprintType

Data about a detected flight event, passed to banners and HUD.

| Field | Type | Description |
|---|---|---|
| `EventType` | `EFlightEvent` | Event type enum |
| `MET` | `double` | Mission elapsed time at detection |
| `Altitude` | `double` | Altitude ASL at detection (meters) |
| `Velocity` | `double` | Velocity at detection (m/s) |
| `EventLabel` | `FString` | Display text (e.g. "MAX Q \| 32,450 Pa") |
| `ECEFPosition` | `FVector` | ECEF position for banner placement |

### FPendingBanner
**File:** `Public/BannerManager.h` | USTRUCT

Banner queued for deferred spawn.

| Field | Type | Description |
|---|---|---|
| `EventData` | `FFlightEventData` | Event that triggered this banner |
| `TrajectoryAtTrigger` | `FVector` | Rocket UE velocity at trigger time |
| `SpawnPosition` | `FVector` | Rocket UE position at trigger time |
| `TriggerWorldTime` | `double` | World time when banner should spawn |

### FTelemetrySample
**File:** `Public/HermiteInterpolator.h`

Single timestamped sample for the Hermite interpolation ring buffer.

| Field | Type | Description |
|---|---|---|
| `Position` | `FVector` | ECEF position (meters) |
| `Velocity` | `FVector` | ECEF velocity (m/s) |
| `Rotation` | `FQuat` | ECEF-frame quaternion |
| `Acceleration` | `FVector` | Body-frame acceleration (m/s^2) |
| `EngineThrustPercent` | `TArray<float>` | Per-engine thrust |
| `MET` | `double` | Mission elapsed time |
| `Timestamp` | `double` | Wall-clock receive time |
| `bValid` | `bool` | Sample validity |

### FCSVTelemetryRow
**File:** `Public/CSVTelemetryProvider.h`

Parsed CSV row.

| Field | Type | Description |
|---|---|---|
| `MET` | `double` | Mission elapsed time |
| `Position` | `FVector` | ECEF position |
| `Rotation` | `FQuat` | ECEF quaternion |
| `Acceleration` | `FVector` | Body-frame acceleration |
| `Velocity` | `FVector` | ECEF velocity |
| `EngineThrustPercent` | `TArray<float>` | Per-engine thrust |

---

## Interfaces

### ITelemetryProvider
**File:** `Public/TelemetryProvider.h` | UInterface, MinimalAPI, Blueprintable

The contract that any telemetry source must implement. The subsystem auto-discovers the highest-priority provider at runtime.

| Method | Return | Description |
|---|---|---|
| `GetTelemetryData()` | `FTelemetryInputData` | Return current telemetry frame |
| `IsTelemetryAvailable()` | `bool` | True if provider has valid data |
| `GetProviderPriority()` | `int32` | Higher value = preferred (CSV=100, Setup=50) |

**Implementors:**
- `ACSVTelemetryProvider` (priority 100)
- `ARocketARSetupActor` (priority 50, Method B direct variables)

---

## Delegates

### FOnTelemetryUpdated
**File:** `Public/TelemetrySubsystem.h`
**Signature:** `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTelemetryUpdated, const FProcessedTelemetryData&, Data)`
**Fired:** Each frame after telemetry is processed by the subsystem.

### FOnFlightEvent
**File:** `Public/FlightEventDetector.h`
**Signature:** `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFlightEvent, const FFlightEventData&, EventData)`
**Fired:** When a flight event is detected. Most events fire once (latched); altitude markers repeat.

---

## Core Classes

### UTelemetrySubsystem
**File:** `Public/TelemetrySubsystem.h` | Parent: `UTickableWorldSubsystem`

World subsystem managing the complete telemetry pipeline. Auto-created by UE when any world loads.

**Responsibilities:**
- Auto-discovers `ITelemetryProvider` actors (polls periodically, picks highest priority)
- Feeds raw data into `UHermiteInterpolator` for 5-10Hz to 60fps upsampling
- Converts ECEF to UE world space via Cesium (or fallback math)
- Computes derived values: altitude (WGS84), Mach, G-force, dynamic pressure, vertical velocity
- Manages extrapolation timeout and staleness detection
- Broadcasts `OnTelemetryUpdated` each tick

**Key Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `ExtrapolationTimeout` | `float` | 1.0 | Seconds before marking data stale |
| `OnTelemetryUpdated` | `FOnTelemetryUpdated` | — | BlueprintAssignable delegate |

**Key Methods:**

| Method | Description |
|---|---|
| `SetGeoreference(ACesiumGeoreference*)` | Set the Cesium georeference for ECEF conversion |
| `GetProcessedData()` | Returns latest `FProcessedTelemetryData` |
| `GetRawData()` | Returns latest `FTelemetryInputData` |
| `IsTelemetryActive()` | True if a valid provider is active |
| `DiscoverProviders()` | Manually trigger provider discovery |

### UHermiteInterpolator
**File:** `Public/HermiteInterpolator.h` | Parent: `UObject`

Cubic Hermite interpolation for smooth 60fps rendering from 5-10Hz telemetry.

- **Position:** Cubic Hermite spline using velocity as tangents
- **Rotation:** Quaternion SLERP between two nearest samples
- **Scalars (thrust, MET):** Linear interpolation
- **Ring buffer:** 16 samples maximum
- **Extrapolation:** Up to `ExtrapolationTimeout` seconds, then freezes

**Key Methods:**

| Method | Description |
|---|---|
| `AddSample(FTelemetrySample)` | Push sample into ring buffer |
| `GetInterpolated(...)` | Interpolate at given timestamp; returns false if no valid samples |
| `Reset()` | Clear all samples |
| `GetSampleCount()` | Number of valid samples in buffer |

---

## Banner System

### UBannerManager
**File:** `Public/BannerManager.h` | Parent: `UActorComponent`

Manages the complete banner lifecycle: event listening, deferred queuing, spawning, attachment, culling.

**Responsibilities:**
- Listens to `UFlightEventDetector::OnFlightEvent`
- Queues banners with configurable time offset and anticipation
- Spawns `ABannerActor` instances attached to the rocket mount point
- Enforces maximum active banner count (culls oldest)
- Applies separate geometry configuration for event banners vs altitude markers
- Spawns banners at a configurable Z offset above vehicle center (toward nose)

**Banner Geometry Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `BannerDiskRadius` | `float` | 5000.0 | Event banner radius (cm) |
| `BannerDiskThickness` | `float` | 100.0 | Event banner thickness (cm) |
| `MaxActiveBanners` | `int32` | 20 | Max simultaneous banners |
| `BannerFadeOutDuration` | `float` | 1.0 | Fade-out time (seconds) |
| `BannerMaterial` | `UMaterialInterface*` | null | Material with BannerTexture + Opacity params |
| `BannerFont` | `UFont*` | null | Font for banner text |
| `BannerActorClass` | `TSubclassOf<ABannerActor>` | ABannerActor | Spawned actor class |

**Altitude Marker Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `MarkerDiskRadius` | `float` | 2000.0 | Marker radius (cm) |
| `MarkerDiskThickness` | `float` | 50.0 | Marker thickness (cm) |
| `MarkerColor` | `FLinearColor` | (0.2, 0.8, 1.0, 1.0) | Marker color (cyan) |

**Slide & Timing Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `TriggerTimeOffset` | `float` | 0.0 | Delay before spawn (seconds) |
| `SlideSpeed` | `float` | 5000.0 | Slide velocity (cm/s) |
| `SlideDuration` | `float` | 10.0 | Time before fade begins (seconds) |
| `FadeInDuration` | `float` | 0.3 | Opacity ramp-up (seconds) |
| `BannerSpawnZOffset` | `float` | 8000.0 | Local Z above vehicle center for banners (cm) |
| `MarkerSpawnZOffset` | `float` | 6000.0 | Local Z above vehicle center for markers (cm) |
| `AnticipationSeconds` | `float` | 1.5 | Seconds before trigger to begin spawn |

**Key Methods:**

| Method | Description |
|---|---|
| `SpawnBanner(FFlightEventData)` | Immediate spawn (bypasses queue) |
| `QueueBanner(FFlightEventData, FVector)` | Deferred spawn with time offset - anticipation |
| `DestroyAllBanners()` | Remove all active and queued banners |
| `GetActiveBannerCount()` | Number of active banners |
| `SetEventDetector(UFlightEventDetector*)` | Connect to event detector |
| `SetAttachTarget(USceneComponent*)` | Set rocket mount point for attachment |
| `UpdateVehiclePosition(FVector)` | Cache current vehicle UE position |
| `UpdateVehicleVelocity(FVector)` | Cache current vehicle UE velocity |

### ABannerActor
**File:** `Public/BannerActor.h` | Parent: `AActor`

Individual banner instance: flat disk mesh with rendered text, slide motion, fade animation.

**State Machine:** `SpawnAnimation` -> `Active` -> `FadeOut` -> `Destroyed`

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `LifetimeSeconds` | `float` | 30.0 | Total visible time before fade-out |
| `FadeInDuration` | `float` | 0.3 | Opacity ramp-up at spawn |
| `FadeOutDuration` | `float` | 1.0 | Opacity ramp-down at end |
| `SpawnTime` | `double` | — | World time when spawned |

**Key Methods:**

| Method | Description |
|---|---|
| `InitBanner(FFlightEventData, Radius, Thickness, Material, Font)` | Configure geometry and render text |
| `InitSlide(float SlideSpeed)` | Set slide velocity (local -Z) |
| `SetDiskColor(FLinearColor)` | Override material color (for markers) |
| `StartFadeOut()` | Begin fade-out animation |
| `ForceDestroy()` | Immediate destruction |
| `GetBannerState()` | Current lifecycle state |
| `GetEventData()` | Event data this banner represents |

---

## Camera System

### URocketARCameraManager
**File:** `Public/RocketARCameraManager.h` | Parent: `UActorComponent`

Drives a `CineCameraActor` with body-fixed mounting offset, configurable FOV, and optical roll.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `CameraMountOffset` | `FVector` | (0, 500, 4000) | Body-frame offset from vehicle center (cm). Z = toward nose. |
| `CameraMountRotation` | `FRotator` | (-80, -10, 0) | Body-frame rotation. Pitch down = look back along body. |
| `CameraOpticalRoll` | `float` | 45.0 | Roll around the camera's optical axis (degrees) |
| `CameraHFOV` | `float` | 110.0 | Horizontal field of view (degrees, clamped 1-180) |

**Key Methods:**

| Method | Description |
|---|---|
| `SetCameraActor(ACineCameraActor*)` | Assign the camera to drive |
| `SetGeoreference(ACesiumGeoreference*)` | Set Cesium for ECEF conversion |
| `AttachToComponent(USceneComponent*)` | Attach camera to rocket mount point |
| `UpdateFromTelemetry(FProcessedTelemetryData)` | Update camera position (when not attached) |
| `UpdateRelativeTransform()` | Apply mount offset + optical roll |

---

## Flight Event Detection

### UFlightEventDetector
**File:** `Public/FlightEventDetector.h` | Parent: `UObject`

Detects 15 SLS flight milestones plus altitude markers. All events except altitude markers are latched (fire once per flight).

**Properties:**

| Property | Type | Description |
|---|---|---|
| `Config` | `FFlightEventConfig` | Data-driven detection thresholds |
| `OnFlightEvent` | `FOnFlightEvent` | BlueprintAssignable event delegate |

**Detection Logic:**

| Event | Trigger Condition |
|---|---|
| Ignition | Any engine thrust > threshold (rising edge) |
| Liftoff | Altitude crosses threshold (rising edge) |
| Mach 1 | Mach number >= 1.0 (rising edge) |
| Max Q | Dynamic pressure peak: 20s rising, 5% drop, 1s confirmation |
| SRB Ignition | SRB engines activate |
| SRB Separation | SRB engines shut down (after SRB ignition latched) |
| MECO | Core engines shut down (after ignition latched) |
| Stage Separation | 3 seconds after MECO |
| 2nd Stage Ignition | Second stage activates (after MECO latched) |
| Fairing Jettison | Altitude > 100km AND Q < 1 Pa |
| SECO | Second stage shuts down (after 2nd stage ignition) |
| Apogee | Vertical velocity goes positive -> negative (after MECO) |
| Reentry | Q rises above threshold (after apogee) |
| Chute Deployment | Altitude drops below threshold while descending |
| Splashdown | Altitude near sea level (after apogee) |
| Altitude Markers | Predictive: fires when `altitude + verticalVelocity * anticipation` crosses interval |

**Engine Group Layout** (thrust array indices):

```
[SRB1, SRB2, Core1, Core2, Core3, Core4, Stage2]
 ←SRBCount→  ←────CoreCount────→  ←remainder→
```

**Key Methods:**

| Method | Description |
|---|---|
| `ProcessTelemetry(FProcessedTelemetryData)` | Call each tick to check all events |
| `Reset()` | Clear all latches and event history |
| `GetDetectedEvents()` | Cumulative list of all fired events |

---

## Atmosphere Model

### UAtmosphereModel
**File:** `Public/AtmosphereModel.h` | Parent: `UObject`

US Standard Atmosphere 1976 implementation. All methods are static.

**7 Layers:** Troposphere (0-11km), Tropopause (11-20km), Stratosphere 1 (20-32km), Stratosphere 2 (32-47km), Stratopause (47-51km), Mesosphere 1 (51-71km), Mesosphere 2 (71-85km). Returns 0 above 86km.

**Static Methods:**

| Method | Return | Description |
|---|---|---|
| `GetTemperature(altitude)` | `double` | Temperature in Kelvin |
| `GetPressure(altitude)` | `double` | Pressure in Pascals |
| `GetDensity(altitude)` | `double` | Density in kg/m^3 |
| `GetSpeedOfSound(altitude)` | `double` | Speed of sound in m/s |
| `GetDynamicPressure(altitude, velocity)` | `double` | Dynamic pressure in Pa |
| `GetMachNumber(altitude, velocity)` | `double` | Mach number (0 above 86km) |

---

## HUD System

### ARocketARHUD
**File:** `Public/RocketARHUD.h` | Parent: `AHUD`

Canvas-based HUD overlay. No UMG dependency.

**Display elements:**
- Bottom-left: MET, Altitude, Velocity
- Top-center: Event name with fade-out (5s display, 1s fade)

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `bShowTelemetry` | `bool` | true | Show telemetry readout |
| `bShowEvents` | `bool` | true | Show event announcements |

### URocketARHUDWidget
**File:** `Public/RocketARHUDWidget.h` | Parent: `UUserWidget`

C++ base class for UMG-based HUD widget. Binds directly to `UTelemetrySubsystem`.

**Display properties** (BlueprintReadOnly, bindable in UMG):

| Property | Type | Description |
|---|---|---|
| `METDisplay` | `FString` | Formatted MET (T+HH:MM:SS) |
| `AltitudeDisplay` | `FString` | Altitude with unit |
| `VelocityDisplay` | `FString` | Speed in m/s |
| `MachDisplay` | `FString` | Mach number |
| `GForceDisplay` | `FString` | G-force |
| `FlightPhaseDisplay` | `FString` | Current flight regime |
| `TelemetryStatusDisplay` | `FString` | LIVE / CSV / PREDICTED / STALE |
| `DynamicPressureDisplay` | `FString` | Dynamic pressure |
| `StatusColor` | `FLinearColor` | Green/Yellow/Red health indicator |

---

## Dev Visualization

### ADevVisualizationActor
**File:** `Public/DevVisualizationActor.h` | Parent: `AActor`

Development-only visualization with Earth sphere and rocket cylinder.

**Components:**
- `EarthMesh` — Sphere at real scale (~6.4M km radius), positioned at ECEF origin
- `RocketMesh` — Cylinder (8m diameter x 100m tall) at vehicle position
- `RocketMountPoint` — Unscaled `USceneComponent` at vehicle position; camera and banners attach here

**Key Methods:**

| Method | Description |
|---|---|
| `UpdateFromTelemetry(FProcessedTelemetryData)` | Move rocket, orient to velocity |
| `SetVisible(bool)` | Toggle visibility |
| `SetEarthTransform(FVector, FVector)` | Set Earth center and pole direction in UE space |
| `GetRocketMountPoint()` | Returns attachment point for camera/banners |

---

## Media Output

### URocketARMediaOutput
**File:** `Public/RocketARMediaOutput.h` | Parent: `UActorComponent`

DeckLink 8K Pro fill/key SDI output. Currently a stub awaiting hardware.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `MediaOutputAssetPath` | `FSoftObjectPath` | — | BlackmagicMediaOutput asset |
| `bAutoStart` | `bool` | false | Start capture on BeginPlay |
| `bAutoRestart` | `bool` | true | Restart on failure |
| `RestartDelay` | `float` | 2.0 | Seconds before restart |
| `bEnableGenlock` | `bool` | false | BlackmagicCustomTimeStep |
| `bEnableTimecode` | `bool` | false | BlackmagicTimecodeProvider |

---

## Input System

### URocketARInputComponent
**File:** `Public/RocketARInputComponent.h` | Parent: `UActorComponent`

Keyboard bindings for operator controls during playback.

| Key | Action |
|---|---|
| Space | Play/Pause CSV playback |
| Right Arrow | Scrub forward 10s |
| Left Arrow | Scrub backward 10s |
| `]` | Increase time scale (+0.2x) |
| `[` | Decrease time scale (-0.2x) |
| R | Reset playback |
| D | Toggle dev visualization |
| F1 | Toggle HUD overlay |
| F2 | Toggle performance stats |
| Tab | Cycle through active banners |

---

## Build Configuration

### DefaultEngine.ini — Alpha Output Settings

```ini
r.PostProcessing.PropagateAlpha=1          ; Alpha through post-processing
r.SceneColorFormat=0                       ; PF_FloatRGBA (64-bit)
r.AntiAliasingMethod=1                     ; FXAA only (TAA = alpha ghosting)
bUseLargeWorldCoordinates=True             ; Required for 185km altitude
r.DefaultFeature.AutoExposure=False        ; No auto-exposure
r.DefaultFeature.MotionBlur=False          ; No motion blur
r.DefaultFeature.Bloom=False               ; No bloom
r.DepthOfFieldQuality=0                    ; No DOF
r.DefaultFeature.LensFlare=False           ; No lens flare
r.SSR.Quality=0                            ; No screen-space reflections
```

### Conditional Compilation Flags

| Flag | Description |
|---|---|
| `WITH_CESIUM` | Cesium for Unreal available; enables ECEF conversion |
| `WITH_BLACKMAGIC` | Blackmagic Media plugin; enables DeckLink output |

---

## Stat Groups

### STATGROUP_RocketAR

| Stat | Type | Location |
|---|---|---|
| `STAT_TelemetryTick` | Cycle | UTelemetrySubsystem::Tick |
| `STAT_Interpolation` | Cycle | Hermite interpolation |
| `STAT_DerivedValues` | Cycle | Derived value computation |
| `STAT_EventDetection` | Cycle | Flight event detection |
| `STAT_ActiveBanners` | Counter | Active banner count |

Toggle in-editor: `stat RocketAR` or press F2.

---

## Master Setup Actor

### ARocketARSetupActor
**File:** `Public/RocketARSetupActor.h` | Parents: `AActor`, `ITelemetryProvider`

The single actor that orchestrates all RocketAR systems. Place one in the level and configure via its Details panel.

**Created in constructor:**
- `UBannerManager` (component)
- `URocketARCameraManager` (component)

**Created in BeginPlay:**
- `UFlightEventDetector` (UObject)
- `ACesiumGeoreference` (actor, if not already present)
- `ACineCameraActor` (actor)
- `ACSVTelemetryProvider` (actor, if CSV mode enabled)
- `ADevVisualizationActor` (actor)
- `ARocketARHUD` (actor)

**Live-synced in Tick:** All configurable properties are pushed to subsystems every frame, enabling real-time tuning during PIE.

See the [User Guide](UserGuide.md) for complete property documentation.
