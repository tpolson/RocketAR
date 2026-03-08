# Configuration Reference

All configuration is on the `ARocketARSetupActor` placed in `RocketAR_Main`.

## Launch Site

| Property | Default | Description |
|---|---|---|
| LaunchPadLatitude | 28.5729 | Launch site latitude (degrees) |
| LaunchPadLongitude | -80.6490 | Launch site longitude (degrees) |
| LaunchPadAltitude | 0.0 | Launch site altitude above WGS84 (meters) |

## Camera

| Property | Default | Description |
|---|---|---|
| CameraMountOffset | (0, 0, 100) | Body-fixed offset from vehicle center (cm) |
| CameraMountRotation | (0, 0, 0) | Body-fixed rotation offset (degrees) |
| CameraHFOV | 65.0 | Horizontal field of view (degrees) |

## Banners

| Property | Default | Description |
|---|---|---|
| BannerArcAngle | 120.0 | Arc angle of the banner (degrees) |
| BannerArcRadius | 300.0 | Radius of the cylindrical arc (cm) |
| BannerArcHeight | 150.0 | Height of the banner (cm) |
| BannerLifetimeSeconds | 30.0 | Time before banner fades out (seconds) |
| MaxActiveBanners | 20 | Maximum simultaneous banners |
| AltitudeMarkerInterval | 10000.0 | Altitude marker spacing (meters) |
| BannerMaterial | null | Material with BannerTexture and Opacity params |
| BannerFont | null | Font for banner text rendering |

## Flight Event Detection

Configured via `FFlightEventConfig` on the setup actor:

| Property | Default | Description |
|---|---|---|
| LiftoffAltitudeThreshold | 1.0 | Altitude to trigger liftoff (meters) |
| MaxQRisingDuration | 20.0 | Min rising Q duration for Max-Q (seconds) |
| MaxQDropPercent | 0.05 | % drop below peak to confirm Max-Q |
| MaxQConfirmationWindow | 1.0 | No higher Q in this window (seconds) |
| ThrustOnThreshold | 0.01 | Thrust level for engine on/off |
| SRBEngineCount | 2 | Number of SRB engines in thrust array |
| CoreEngineCount | 4 | Number of core engines in thrust array |
| AltitudeMarkerInterval | 10000.0 | Spacing between altitude markers (meters) |
| AltitudeMarkerMinSpacing | 5000.0 | Minimum spacing between markers (meters) |

## Telemetry

| Property | Default | Description |
|---|---|---|
| ExtrapolationTimeout | 1.0 | Seconds before data is marked stale |
| bUseCSVProvider | true | Enable CSV playback mode |
| CSVFilePath | Data/SimulatedTelemetry.csv | Path to CSV file |

## Dev/Debug

| Property | Default | Description |
|---|---|---|
| bDevVisualization | false | Show Earth sphere + rocket cylinder |
| bShowHUD | true | Show telemetry HUD overlay |

## DeckLink Output

Configure on the `URocketARMediaOutput` component (or add as component to setup actor):

| Property | Default | Description |
|---|---|---|
| MediaOutputAssetPath | (none) | Path to BlackmagicMediaOutput asset |
| bAutoStart | false | Start capture on BeginPlay |
| bAutoRestart | true | Restart capture on failure |
| RestartDelay | 2.0 | Seconds before restart attempt |
| bEnableGenlock | false | Enable genlock via BlackmagicCustomTimeStep |
| bEnableTimecode | false | Enable timecode via BlackmagicTimecodeProvider |

## Alpha Output (DefaultEngine.ini)

These are set in `Config/DefaultEngine.ini` and should not need modification:

- `r.PostProcessing.PropagateAlpha = 1`
- `r.SceneColorFormat = 0` (PF_FloatRGBA)
- Anti-aliasing: FXAA (method 1)
- Auto-exposure: disabled
- Motion blur, bloom, DOF, vignette, chromatic aberration, film grain: all disabled
