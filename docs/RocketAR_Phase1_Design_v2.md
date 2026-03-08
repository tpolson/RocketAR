# Rocket AR — Phase 1 Design Document (Revised)
# CG Overlay Engine with Blueprint Telemetry Interface

## What Changed

Based on client requirements, the project is fundamentally different from our initial assumptions:

| Assumption | Initial | Actual |
|---|---|---|
| Deliverable | UE plugin | **Complete UE project file** |
| Video | Composited inside UE | **No video into UE — alpha-only CG output** |
| Compositing | UE does the composite | **Client's switcher does the composite** |
| Telemetry source | Our mock provider or UDP | **Client's proprietary Blueprint plugin** |
| Telemetry interface | C++ adapter wrapping their plugin | **Blueprint input variables they wire to** |
| Coordinate system | Altitude above ground (meters) | **ECEF (Earth-Centered Earth-Fixed, meters)** |
| Telemetry rate | 60Hz frame-locked | **5–10Hz with interpolation needed** |
| Sync | Complex timestamp matching | **Not our problem — client handles it** |
| Hardware | We spec and procure | **Client provides everything** |
| Output | Viewport or SDI composite | **DeckLink 8K Pro fill/key (alpha channel)** |
| Test data | Physics simulation | **CSV from client with simulated values** |

---

## 1. System Overview

This is a **broadcast CG graphics engine**. It receives telemetry from a rocket via the client's Blueprint plugin, detects flight milestones, and renders text banners wrapping the rocket axis as transparent CG overlays. These overlays are output via the DeckLink 8K Pro's fill/key SDI outputs to the client's broadcast switcher, which composites them over the live rocket camera feed.

UE never sees the video. UE only renders CG graphics with alpha.

```
CLIENT'S INFRASTRUCTURE                    OUR UE PROJECT
═══════════════════════                    ═══════════════

  Rocket Camera ──► Video ──────────────────────────────┐
                                                        │
  Rocket Telemetry ──► Client's         ┌─────────────┐│
                       Blueprint  ────► │ Blueprint    ││
                       Plugin           │ Input Vars   ││
                                        └──────┬──────┘│
                                               │       │
                                               ▼       │
                                  ┌─────────────────┐  │
                                  │ ECEF → ENU      │  │
                                  │ Conversion      │  │
                                  └──────┬──────────┘  │
                                         │             │
                                         ▼             │
                                  ┌─────────────────┐  │
                                  │ Interpolation   │  │
                                  │ (5-10Hz → 60fps)│  │
                                  └──────┬──────────┘  │
                                         │             │
                                  ┌──────▼──────────┐  │
                                  │ Event Detector  │  │
                                  │ + Banner Manager│  │
                                  └──────┬──────────┘  │
                                         │             │
                                  ┌──────▼──────────┐  │
                                  │ Render          │  │
                                  │ (transparent BG │  │
                                  │  alpha output)  │  │
                                  └──────┬──────────┘  │
                                         │             │
                                  ┌──────▼──────────┐  │
                                  │ DeckLink 8K Pro │  │
                                  │ Fill (SDI 1)    │  │
                                  │ Key  (SDI 2)    │  │
                                  └──┬──────────┬───┘  │
                                     │          │      │
                      ┌──────────────▼──────────▼──────▼──┐
                      │       CLIENT'S SWITCHER            │
                      │   Fill/Key CG over live video      │
                      └──────────────┬─────────────────────┘
                                     │
                              BROADCAST OUTPUT
```

---

## 2. Blueprint Telemetry Interface

### 2.1 The Contract

The client's proprietary Blueprint plugin will wire into our project via **exposed Blueprint input variables**. We define exactly what we need; they provide the values.

**Required telemetry inputs:**

| Variable Name | Type | Units | Description |
|---|---|---|---|
| `VehiclePosition` | Vector3 | ECEF meters | Vehicle position in Earth-Centered Earth-Fixed coordinates |
| `VehicleRotation` | Quat (Vector4) | Quaternion | Vehicle orientation (ECEF frame) |
| `VehicleAcceleration` | Vector3 | m/s² | Acceleration in body frame |
| `VehicleVelocity` | Vector3 | m/s | Velocity vector (ECEF frame) |
| `EngineThrustPercent` | Float Array | 0.0–1.0 | Per-engine thrust percentage |
| `MissionElapsedTime` | Float | Seconds | Mission elapsed time (T- negative for countdown) |
| `bTelemetryValid` | Bool | — | True when telemetry data is valid |

### 2.2 Architecture: Blueprint Interface + Subsystem

We expose a **UInterface** (`ITelemetryProvider`) with a **BlueprintType USTRUCT** (`FTelemetryInputData`) containing all the above fields. The client implements this interface on their plugin's actor. Our project queries it via a world subsystem.

**Why a UInterface, not raw variables on an actor?** The client can implement it on any actor or component in their plugin without modifying our code. Our subsystem discovers the provider at runtime. If they change their internal architecture, the contract stays the same.

**Blueprint wiring for the client is simple:**
1. Open our project in their UE instance
2. Add their plugin to the project
3. Their plugin's actor implements the `ITelemetryProvider` interface
4. In their actor's Blueprint, implement the `GetTelemetryData` event → fill the struct from their internal data
5. Place their actor in the level — our subsystem auto-discovers it

### 2.3 What We Derive from Their Data

Several values we need aren't in their telemetry. We compute them:

| Derived Value | Computed From |
|---|---|
| Altitude (AGL/ASL) | ECEF position → geodetic conversion → height above WGS84 ellipsoid |
| Velocity magnitude (m/s) | `Length(VehicleVelocity)` |
| Mach number | Velocity magnitude / speed_of_sound(altitude) |
| G-force | `Length(VehicleAcceleration)` / 9.80665 |
| Dynamic pressure (Pa) | 0.5 × air_density(altitude) × velocity² |
| Engine on/off state | Any element of `EngineThrustPercent` > 0.01 |
| Vertical velocity | Velocity projected onto the local "up" vector at the vehicle's position |

---

## 3. ECEF Coordinate Conversion

### 3.1 The Problem

ECEF positions are millions of meters from the coordinate origin (Earth's center). At the launch pad, the position might be roughly (X=920,000, Y=-5,360,000, Z=3,020,000) meters. In UE centimeters, that's hundreds of millions of units from origin — well beyond single-precision float range.

### 3.2 Solution: Local Tangent Plane (ENU)

We define a **reference origin** at the launch pad's geodetic position (lat, lon, height). All ECEF positions are converted to a local **East-North-Up (ENU)** frame relative to this origin, then mapped to UE world coordinates.

**ECEF → ENU conversion:**
1. Compute the ECEF position of the reference origin (launch pad)
2. For each telemetry update: ΔX, ΔY, ΔZ = vehicle ECEF − origin ECEF
3. Rotate the offset into ENU using the reference lat/lon
4. Map ENU → UE: X_ue = East × 100, Y_ue = -North × 100, Z_ue = Up × 100 (meters → cm, with handedness flip)

**The reference origin is configurable** — exposed as a Blueprint variable on the setup actor so the client can enter their launch site's lat/lon/alt.

### 3.3 Cesium for Unreal (Recommended)

Rather than implementing ECEF→ENU from scratch, use **Cesium for Unreal** (free, open-source). It provides:
- `ACesiumGeoreference` — place in level, set lat/lon/height of origin
- `TransformEcefToUnreal()` — direct ECEF-to-UE conversion with double precision
- `UCesiumGlobeAnchorComponent` — attach to any actor to store its position in ECEF
- Handles the coordinate system handedness flip and precision management automatically

This eliminates ~40 hours of custom math implementation and testing.

### 3.4 Precision Management

UE5 Large World Coordinates (double precision on CPU) is **required** and must be enabled. The GPU still uses float32, but UE5's camera-relative rendering mitigates jitter. For a rocket going from ground to ~185km altitude, the maximum distance from the UE origin is ~185km = 18,500,000 cm. At this distance, float32 gives ~2cm precision — acceptable for banner placement.

If the rocket travels significant downrange distance (hundreds of km), origin rebasing may be needed to keep the active area near the UE origin. Cesium handles this automatically via its `CesiumGeoreference` origin shifting.

---

## 4. Interpolation (5–10Hz → 60fps)

### 4.1 The Problem

Telemetry arrives at 5–10Hz. The render runs at 60fps. Without interpolation, banners would jump in 100–200ms increments — visually unacceptable for broadcast.

### 4.2 Solution: Hermite Spline Interpolation with Velocity

Each telemetry update provides position AND velocity. This gives us the derivative at each sample point, enabling cubic Hermite interpolation between samples:

```
At each telemetry update, store: {position, velocity, timestamp}

Between updates, for each render frame at time t:
  t_normalized = (t - t_previous) / (t_current - t_previous)

  position = HermiteInterp(
    pos_previous, vel_previous × dt,
    pos_current, vel_current × dt,
    t_normalized
  )
```

This produces smooth, physically plausible motion between samples because the velocity vectors constrain the curve's tangents.

**For rotation:** Quaternion SLERP between the previous and current orientations.

**For scalar values** (thrust, acceleration magnitude): Linear interpolation is sufficient.

### 4.3 Prediction for Late-Arriving Data

If a telemetry update is late (more than 200ms since the last), extrapolate using the last known velocity:

```
predicted_position = last_position + last_velocity × dt
```

Hold this prediction for up to 1 second. If no update arrives within 1 second, freeze at the last known position and flag `bTelemetryStale = true`.

---

## 5. Alpha-Only Rendering

### 5.1 Project Configuration

The UE project must be configured for clean alpha output with transparent background:

**Required settings:**
- `r.PostProcessing.PropagateAlpha = 1` (Linear color space only — preserves alpha through post-processing)
- `r.SceneColorFormat = 0` (PF_FloatRGBA, 64-bit with alpha channel)
- Anti-aliasing: FXAA (not TAA — TAA causes alpha flickering and ghosting at edges)
- Fixed exposure (disable auto-exposure — fluctuations interact with premultiplied alpha)

**Strip all background rendering:**
- No Sky Sphere, Sky Atmosphere, Volumetric Clouds, Exponential Height Fog
- No Sky Light with visible sky
- No ground plane or environment geometry
- Background renders as (0, 0, 0, 0) — fully transparent black in premultiplied space

**Disable these post-process effects:**
- Motion Blur (smears alpha edges)
- Bloom (expands alpha beyond geometry)
- Depth of Field (blurs alpha incorrectly)
- Vignette (makes all pixels partially opaque)
- Chromatic Aberration (bleeds color outside alpha)
- Film Grain (adds noise outside alpha)

### 5.2 What Gets Rendered

Only CG graphic elements:
- Text banners (cylindrical arc meshes with emissive text material)
- Optional HUD overlay (UMG widget rendered to texture)
- Nothing else

### 5.3 Dev Visualization Mode

For development and testing, a **toggle-able visualization mode** adds:
- Earth sphere with Blue Marble texture (for spatial context)
- Rocket cylinder at the vehicle position
- Camera preview from the onboard POV

These are hidden in production output. A single Blueprint-accessible bool (`bDevVisualization`) toggles the entire dev scene on/off.

---

## 6. Banner System

### 6.1 Geometry

Unchanged from original design: 120° cylindrical arc wrapping the rocket axis, with text rendered on the curved surface. Radius slightly larger than the rocket body.

However, since we don't render the rocket body in production, the banner radius is a **configurable parameter** — the client may need to adjust it to match their physical rocket's diameter.

### 6.2 Positioning (Updated for ECEF)

Each banner stores its spawn ECEF position. Every frame:

```
banner_UE_position = CesiumGeoreference.TransformEcefToUnreal(banner_ECEF)
```

The banner is always at its Earth-fixed ECEF position. As the rocket (and camera) move, the banner naturally recedes in the camera view. The positioning math is simpler than Phase 1's original approach because Cesium handles the coordinate transform.

### 6.3 Orientation

The banner orients perpendicular to the local "up" vector at its spawn position (horizontal relative to Earth's surface). The banner's arc faces the camera. Since we know the camera's position (it's at the rocket), we can compute the facing direction each frame:

```
banner_facing = normalize(camera_position - banner_position)
banner_rotation = LookAt(banner_facing) constrained to horizontal plane
```

### 6.4 Camera

The CG camera position and orientation are driven by the telemetry:
- Position: vehicle ECEF → UE coordinates (same transform as banners)
- Rotation: vehicle quaternion → UE rotation (with coordinate system conversion)
- Camera offset: configurable mounting offset (position and angle relative to rocket center)

The CG camera's FOV must match the physical camera's FOV. Exposed as a configurable parameter.

---

## 7. Flight Event Detection

### 7.1 Events (Same as Original)

All 15 SLS flight events plus altitude markers. Detection logic unchanged — edge detection on derived values (altitude, Mach, G-force, thrust state, vertical velocity).

### 7.2 Derived Values for Detection

Since the client provides raw ECEF position + velocity + acceleration + thrust, we derive all event-triggering values ourselves:

| Event | Derived From |
|---|---|
| Ignition | `ThrustPercent` any element > 0 (rising edge) |
| Liftoff | Altitude > 1m (rising edge) |
| Mach 1 | Velocity_magnitude / speed_of_sound(alt) ≥ 1.0 |
| Max-Q | Dynamic pressure peak (½ρv² starts decreasing) |
| MECO | `ThrustPercent` all elements drop to 0 |
| Apogee | Vertical velocity goes negative |
| Altitude markers | Height above ellipsoid crosses threshold |

---

## 8. CSV Test Data Reader

### 8.1 Format

The client will provide simulated telemetry as CSV. Expected columns match their Blueprint interface:

```
MET, PosX, PosY, PosZ, RotX, RotY, RotZ, RotW, AccX, AccY, AccZ, VelX, VelY, VelZ, Thrust1, Thrust2, ...
```

### 8.2 Playback Provider

A `UCSVTelemetryProvider` reads the CSV at startup, stores all rows, and plays them back at the original timing (using the MET column) with time scale support. It implements the same `ITelemetryProvider` interface as the live provider. This lets us develop and test the entire pipeline without the client's plugin.

### 8.3 Dev Controls

- Play / Pause / Step
- Scrub to any T+ time
- Time scale (0.1x to 10x)
- Loop playback
- Reset

---

## 9. Deliverable Structure

The deliverable is a **complete UE 5.7 project** that the client opens in their Unreal instance.

```
RocketAR/
├── RocketAR.uproject
├── Config/
│   └── DefaultEngine.ini          ← alpha propagation, LWC, etc.
├── Content/
│   ├── Maps/
│   │   └── RocketAR_Main.umap    ← main level
│   ├── Blueprints/
│   │   ├── BP_RocketARSetup.uasset
│   │   ├── BP_TelemetryInterface.uasset
│   │   └── BP_BannerActor.uasset
│   ├── Materials/
│   │   ├── M_Banner_Text.uasset
│   │   └── M_Banner_Emissive.uasset
│   ├── Fonts/
│   │   └── ShareTechMono.uasset
│   ├── Data/
│   │   └── SimulatedTelemetry.csv ← client's test data
│   └── Dev/                       ← dev visualization assets
│       ├── M_Earth_BluMarble.uasset
│       └── SM_RocketCylinder.uasset
├── Plugins/
│   ├── RocketAR/                  ← our C++ plugin
│   │   └── Source/RocketAR/
│   └── CesiumForUnreal/          ← for ECEF conversion
└── Source/                        ← project source (if needed)
```

---

## 10. Implementation Steps (Phase 1)

### Step 1: Project Setup + Alpha Output (8–12 hrs)
- Create UE 5.7 project with correct settings (PropagateAlpha, LWC, FXAA)
- Strip all background rendering
- Configure BlackmagicMediaOutput for fill/key (even if card isn't connected, asset is ready)
- Verify: render with transparent background, confirm alpha channel is clean

### Step 2: Telemetry Interface + CSV Reader (16–22 hrs)
- Define `ITelemetryProvider` UInterface and `FTelemetryInputData` USTRUCT
- Implement CSV playback provider
- Implement ECEF → UE conversion (via Cesium for Unreal)
- Implement derived value computation (altitude, Mach, G-force, dynamic pressure)
- Implement Hermite interpolation (5–10Hz → 60fps)
- Dev controls (pause, scrub, time scale)

### Step 3: Event Detection (10–14 hrs)
- Flight event detector with all 15 events
- Edge detection logic, Max-Q peak detection
- Altitude markers
- Verify against CSV data — events fire at correct times

### Step 4: Banner System (28–38 hrs)
- Procedural cylindrical arc mesh generation
- Text rendering on curved surface (dynamic material, monospace font)
- Earth-fixed ECEF positioning via Cesium
- Camera-facing orientation
- Spawn animation (scale overshoot + ease-out)
- Distance fade and visibility culling
- Ring manager (spawn, update, cull lifecycle)

### Step 5: Camera + Scene (10–14 hrs)
- CG camera driven by telemetry (ECEF position + quaternion → UE transform)
- Configurable camera mounting offset and FOV
- Dev visualization mode (Earth, rocket cylinder, toggle on/off)

### Step 6: Integration + Testing (16–22 hrs)
- Full ascent playback using client's CSV data
- Verify all events fire correctly
- Verify banner positioning and recession
- Verify alpha output is clean (no background bleed, no edge artifacts)
- Performance profiling (must hold 60fps)
- Setup actor with all configuration exposed

### Step 7: Documentation (4–6 hrs)
- Client integration guide (how to wire their plugin)
- Configuration reference (all exposed parameters)
- Dev controls guide
