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
| BannerDiskRadius | 5000.0 | Event banner disk radius (cm = 50m) |
| BannerDiskThickness | 100.0 | Event banner thickness (cm = 1m) |
| MaxActiveBanners | 20 | Maximum simultaneous banners |
| BannerMaterial | null | Material with BannerTexture and Opacity params |
| BannerFont | null | Font for banner text rendering |

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
| MarkerDiskRadius | 2000.0 | Marker disk radius (cm = 20m) |
| MarkerDiskThickness | 50.0 | Marker thickness (cm = 0.5m) |
| MarkerColor | (0.2, 0.8, 1.0, 1.0) | Marker color (cyan) |
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
| AltitudeMarkerMinSpacing | 5000.0 | Minimum spacing between markers (meters) |
| AltitudeMarkerAnticipation | 2.0 | Predictive look-ahead (seconds) |
| ReentryQThreshold | 1000.0 | Dynamic pressure threshold for reentry (Pa) |
| ChuteDeployAltitude | 8000.0 | Chute deployment altitude (meters) |
| SplashdownAltitude | 10.0 | Splashdown threshold (meters) |

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
