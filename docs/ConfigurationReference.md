# Configuration Reference

All configuration is on the `ARocketARSetupActor` placed in `RocketAR_Main`.
Values are live-synced every frame during PIE — changes in the Details panel take effect immediately.

## Launch Site

| Property | Default | Description |
|---|---|---|
| LaunchPadLatitude | 34.5811 | WGS84 latitude (degrees) |
| LaunchPadLongitude | -120.6257 | WGS84 longitude (degrees) |
| LaunchPadAltitude | 150.0 | Altitude above WGS84 ellipsoid (meters) |

## Camera

| Property | Default | Description |
|---|---|---|
| CameraMountOffset | (0, 500, 4000) | Body-fixed offset from vehicle center (cm). Z = toward nose, Y = lateral. |
| CameraMountRotation | (-80, -10, 0) | Body-fixed rotation (degrees). Pitch -80 = look back along body. |
| CameraOpticalRoll | 45.0 | Roll around optical axis (degrees) |
| CameraHFOV | 110.0 | Horizontal field of view (degrees, 1-180) |

## Banner Geometry

| Property | Default | Description |
|---|---|---|
| BannerWidth | 10000.0 | Event banner width (cm = 100m) |
| BannerHeight | 10000.0 | Event banner height (cm = 100m) |
| BannerRotationYaw | 0.0 | Z-axis rotation (yaw) for event banners (degrees) |
| BannerImage | None | Background texture (PNG with alpha). nullptr = solid color. Import with UserInterface2D (RGBA) compression. |
| BannerTextSize | 200.0 | Text size for event banners (cm, UTextRenderComponent WorldSize) |
| BannerTextOffset | (0, 0, 0) | Local offset of text from banner center (cm) |
| MaxActiveBanners | 20 | Maximum simultaneous banners |

## Banner Slide & Timing

| Property | Default | Description |
|---|---|---|
| TriggerTimeOffset | 0.0 | Delay between event detection and banner spawn (seconds) |
| SlideSpeed | 5000.0 | Slide velocity in local -Z (cm/s = 50 m/s) |
| SlideDuration | 10.0 | Visible time before fade-out (seconds) |
| BannerFadeInDuration | 0.3 | Opacity ramp-up at spawn (seconds) |
| BannerSpawnZOffset | 8000.0 | Local Z above vehicle center for event banners (cm = 80m) |
| MarkerSpawnZOffset | 6000.0 | Local Z above vehicle center for altitude markers (cm = 60m) |
| AnticipationSeconds | 1.5 | How early to spawn banners before trigger time (seconds) |

## Altitude Markers

| Property | Default | Description |
|---|---|---|
| bShowAltitudeMarkers | false | Enable altitude milestone banners |
| AltitudeMarkerInterval | 10000.0 | Spacing between markers (meters = 10km) |
| MarkerWidth | 4000.0 | Altitude marker width (cm = 40m) |
| MarkerHeight | 4000.0 | Altitude marker height (cm = 40m) |
| MarkerColor | (0.2, 0.8, 1.0, 1.0) | Marker color (cyan) |
| MarkerRotationYaw | 0.0 | Z-axis rotation (yaw) for altitude markers (degrees) |
| MarkerImage | None | Background texture for markers (PNG with alpha). nullptr = solid color. |
| MarkerTextSize | 150.0 | Text size for altitude markers (cm) |
| MarkerTextOffset | (0, 0, 0) | Local offset of text from marker center (cm) |
| AltitudeMarkerAnticipation | 2.0 | Predictive look-ahead for early marker firing (seconds) |

## Flight Event Detection

Configured via `FFlightEventConfig` (EventConfig property on setup actor):

| Property | Default | Description |
|---|---|---|
| LiftoffAltitudeThreshold | 1.0 | Altitude to trigger liftoff (meters) |
| MaxQRisingDuration | 20.0 | Min rising Q duration for Max-Q confirmation (seconds) |
| MaxQDropPercent | 0.05 | Fraction drop below peak to confirm Max-Q (5%) |
| MaxQConfirmationWindow | 1.0 | No higher Q in this window to confirm (seconds) |
| ThrustOnThreshold | 0.01 | Thrust level for engine on/off detection (0-1) |
| SRBEngineCount | 2 | Number of SRB engines in thrust array |
| CoreEngineCount | 4 | Number of core engines in thrust array |
| AltitudeMarkerInterval | 10000.0 | Spacing between altitude markers (meters) |
| AltitudeMarkerMinSpacing | 0.0 | Minimum spacing between markers (meters) |
| AltitudeMarkerAnticipation | 2.0 | Predictive look-ahead (seconds) |
| ReentryQThreshold | 1000.0 | Dynamic pressure threshold for reentry (Pa) |
| ChuteDeployAltitude | 8000.0 | Chute deployment altitude (meters) |
| SplashdownAltitude | 10.0 | Splashdown threshold (meters) |

### Per-Event Overrides

The `EventOverrides` array in `FFlightEventConfig` allows per-event customization of built-in events. Add entries only for events you want to modify.

| Field | Type | Default | Description |
|---|---|---|---|
| `EventType` | `EFlightEvent` | Ignition | Which built-in event to override |
| `bEnabled` | `bool` | true | Enable/disable this event |
| `LabelOverride` | `FString` | (empty) | Custom label (empty = C++ default). Supports tokens: `{alt_km}`, `{vel}`, `{mach}`, `{q_pa}`, `{met}`, `{gforce}`, `{extra}` |
| `bOverrideTextOffset` | `bool` | false | Whether to override banner text offset |
| `TextOffsetOverride` | `FVector` | (0, 0, 0) | Per-event text offset (cm) |

Custom events (`CustomEvents` array) also support `bOverrideTextOffset` and `TextOffsetOverride` fields. See the [Technical Reference](TechnicalReference.md) for `FCustomEventDefinition` details.

## Telemetry

| Property | Default | Description |
|---|---|---|
| ExtrapolationTimeout | 1.0 | Seconds before data is marked stale |
| bUseCSVProvider | true | Enable CSV playback mode |
| CSVFilePath | Data/SimulatedTelemetry.csv | Path to CSV file (relative to Content/) |

## HUD & Debug

| Property | Default | Description |
|---|---|---|
| bShowHUDTelemetry | true | Show MET/altitude/velocity readout |
| bShowHUDEvents | true | Show event announcements |
| bShowDebugMessages | true | Show on-screen BANNER:/ALTITUDE: spawn messages |
| bDevVisualization | true | Show Earth sphere + rocket cylinder |

## Dev Visualization

| Property | Default | Description |
|---|---|---|
| RocketHeight | 98.0 | Rocket body height in meters (SLS Block 1 = 98m) |
| RocketRadius | 4.2 | Rocket body radius in meters (SLS Block 1 = 4.2m) |
| bDevOpaqueBanners | false | Use opaque banner material for wireframe dev visibility (no alpha/fade) |

## Dev Camera

| Property | Default | Description |
|---|---|---|
| bUseDevCamera | false | Enable the dev inspection camera (parented to rocket) |
| DevCameraOffset | (0, 0, 15000) | Dev camera offset from rocket root (cm). Z = along rocket axis toward nose. |
| DevCameraRotation | (-90, 0, 0) | Dev camera rotation relative to rocket (default: looking down). |
| DevCameraFOV | 90.0 | Dev camera field of view (degrees, 1-180) |

## Freeze-Frame Mode

| Property | Default | Description |
|---|---|---|
| bFreezeFrameMode | false | Static rocket + banner for visual tuning |
| FreezeFrameAltitude | 30000.0 | Rocket altitude (meters) |
| FreezeFrameEventLabel | "MAX Q" | Test banner label |

## DeckLink Output

On `URocketARMediaOutput` component:

| Property | Default | Description |
|---|---|---|
| MediaOutputAssetPath | (none) | Path to BlackmagicMediaOutput asset |
| bAutoStart | false | Start capture on BeginPlay |
| bAutoRestart | true | Restart capture on failure |
| RestartDelay | 2.0 | Seconds before restart attempt |
| bEnableGenlock | false | Enable genlock via BlackmagicCustomTimeStep |
| bEnableTimecode | false | Enable timecode via BlackmagicTimecodeProvider |

## Alpha Output (DefaultEngine.ini)

Pre-configured, should not need modification:

- `r.PostProcessing.PropagateAlpha = 1`
- `r.SceneColorFormat = 0` (PF_FloatRGBA)
- Anti-aliasing: FXAA (method 1) — TAA causes alpha ghosting
- Auto-exposure: disabled
- Motion blur, bloom, DOF, vignette, lens flare, SSR: all disabled
- Large World Coordinates: enabled (required for 185km altitude)
