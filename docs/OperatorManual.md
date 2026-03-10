# Operator Manual

## Quick Start

1. Open the RocketAR project in Unreal Engine 5.7
2. Open `Content/Maps/RocketAR_Main`
3. Verify `ARocketARSetupActor` is in the level with correct settings
4. Press Play — CSV telemetry plays back automatically

## Keyboard Controls

| Key | Function |
|---|---|
| Space | Pause / Resume playback (CSV mode) |
| Left Arrow | Scrub backward 10 seconds |
| Right Arrow | Scrub forward 10 seconds |
| [ | Decrease time scale (-0.2x) |
| ] | Increase time scale (+0.2x) |
| R | Reset — destroy all banners, restart from T-0 |
| D | Toggle dev visualization (Earth + rocket) |
| F1 | Toggle HUD overlay |
| F2 | Toggle performance stats |
| Tab | Cycle through active banners |

## Flight Events Detected

| Event | Detection Criteria |
|---|---|
| Ignition | Any engine thrust > 1% (rising edge) |
| Liftoff | Altitude exceeds 1m |
| Mach 1 | Velocity / speed of sound >= 1.0 |
| Max Q | Dynamic pressure peaks (20s rising, 5% drop, 1s confirm) |
| SRB Ignition | SRB engines activate |
| SRB Separation | SRB engines shut down |
| MECO | Core engines shut down |
| Stage Separation | 3 seconds after MECO |
| 2nd Stage Ignition | Second stage engine activates after MECO |
| Fairing Jettison | Altitude > 100km and Q < 1 Pa |
| SECO | Second stage engine shuts down |
| Apogee | Vertical velocity goes negative (after MECO) |
| Reentry | Dynamic pressure rises above threshold |
| Chute Deploy | Altitude drops below threshold while descending |
| Splashdown | Altitude near sea level after descent |
| Altitude Markers | Predictive: every 10km during ascent (configurable) |

## Banner Behavior

Banners spawn above the rocket nose (configurable Z offset) and slide down along the rocket body. They begin animating slightly before the trigger point via anticipation timing.

| Parameter | Default | Effect |
|---|---|---|
| BannerSpawnZOffset | 8000 cm | Where banners appear relative to vehicle center |
| AnticipationSeconds | 1.5s | How early banners spawn before the trigger |
| SlideSpeed | 5000 cm/s | How fast banners slide toward exhaust |
| SlideDuration | 10s | How long banners stay visible |

## HUD Display

When enabled (F1), the HUD shows:
- **MET** — Mission Elapsed Time (T+HH:MM:SS)
- **Altitude** — meters or km above sea level
- **Velocity** — m/s

Event names display at top-center with a 5-second duration and 1-second fade.

Status color: Green = good, Yellow = predicted (1-3s stale), Red = signal loss (>3s)

## DeckLink Output (When Hardware Available)

1. Configure DeckLink 8K Pro in Blackmagic Desktop Video Setup:
   - Set two connectors as fill/key output pair
   - Set output format (1080p59.94 or 1080p60)
   - Connect tri-sync/black burst for genlock
2. In RocketAR, set `MediaOutputAssetPath` on the media output component
3. Set `bAutoStart = true` for automatic capture on play
4. Enable genlock: `bEnableGenlock = true`
5. Verify on switcher: clean alpha edges, fully transparent background

## Troubleshooting

**Banners not appearing:**
- Check Output Log for "FLIGHT EVENT" and "BannerManager: Spawned" messages
- Verify CSV is loaded ("CSV loaded: N rows")
- Check BannerMaterial and BannerFont are assigned
- For altitude markers, verify `bShowAltitudeMarkers = true`

**Banners not in camera view:**
- Adjust `BannerSpawnZOffset` — may need to match camera mount height
- Use freeze-frame mode (`bFreezeFrameMode = true`) to tune visually
- Check `CameraMountOffset` Z value is above banner spawn offset

**Alpha not clean:**
- Verify `r.PostProcessing.PropagateAlpha = 1` in Project Settings
- Check anti-aliasing is FXAA (not TAA)
- Ensure no sky, fog, or ground objects in the level

**No telemetry:**
- Check "Telemetry provider discovered" in Output Log
- Verify CSV file path is correct
- If using client plugin, ensure actor implements ITelemetryProvider

**DeckLink issues:**
- "Hardware not detected" — check Desktop Video Setup
- "Capture failed" — verify output format matches switcher
- Auto-restart handles transient failures (2s delay)
