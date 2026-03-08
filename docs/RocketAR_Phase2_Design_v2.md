# Rocket AR — Phase 2 Design Document (Revised)
# DeckLink Fill/Key Output + Production Hardening

## What Phase 2 Actually Is Now

Phase 2 is **much smaller** than originally planned. Since there's no video into UE, no Composure, and no sync buffer, Phase 2 is essentially:

1. Configure and validate the DeckLink 8K Pro fill/key output
2. Genlock to the client's house sync
3. Wire the client's live Blueprint plugin (replacing the CSV reader)
4. Camera calibration matching
5. Production hardening for mission-day reliability

---

## 1. DeckLink 8K Pro Fill/Key Output

### 1.1 How Fill/Key Works

The DeckLink 8K Pro has 4 bidirectional 12G-SDI mini-BNC connectors. For fill/key output at 1080p60, two connectors are paired:

- **Fill (SDI A):** RGB — the color information of the CG graphics
- **Key (SDI B):** Alpha as luminance — white = opaque, black = transparent

Both signals go to the client's broadcast switcher, which uses the key signal to cut the fill over their live video. This is standard broadcast downstream keying (DSK).

### 1.2 Configuration in Blackmagic Desktop Video Setup

Before UE can output, the DeckLink must be configured via the Blackmagic Desktop Video Setup utility:

1. Open Desktop Video Setup on the client's workstation
2. Select the DeckLink 8K Pro
3. Configure two connectors as an **output pair** in fill/key mode
4. Set the output format to match the client's switcher (likely 1080p59.94 or 1080p60)
5. Connect tri-sync or black burst from the client's infrastructure for genlock reference

**Known issue:** Connector numbering in Desktop Video Setup doesn't always match the physical BNC positions on the card bracket. Document the physical mapping during initial setup.

### 1.3 UE Configuration

A `BlackmagicMediaOutput` asset in the project configures the output:

| Property | Setting |
|---|---|
| `output_configuration` | The fill/key connector pair from Desktop Video Setup |
| `pixel_format` | RGBA 8-bit or 10-bit (must include alpha) |
| `number_of_blackmagic_buffers` | 2–3 |
| `number_of_texture_buffers` | 2–3 |
| `wait_for_sync_event` | Enabled (when genlocked) |
| `timecode_format` | VITC (if client needs timecode on the SDI output) |

Output is started via **Window → Media Capture → Viewport Capture** targeting this asset, or controlled at runtime via Blueprint through the `UMediaCapture` object.

### 1.4 Genlock and Timecode

**Genlock** locks UE's render loop to the client's house sync, ensuring every UE frame aligns with the switcher's frame boundaries.

Setup:
1. Create a `BlackmagicCustomTimeStep` Blueprint
2. Configure it to reference the DeckLink's sync input
3. Assign as the Custom Time Step in Project Settings → General Settings → Framerate

**Timecode** embeds broadcast timecode in the SDI output:
1. Create a `BlackmagicTimecodeProvider` Blueprint
2. Configure it for the DeckLink device
3. Assign as the Timecode Provider in Project Settings

### 1.5 Alpha Output Validation

The fill/key output is only correct if:
- `PropagateAlpha = 1` is set (done in Phase 1)
- The scene background is truly transparent (no sky, no fog, no ground)
- The switcher's DSK is set to **linear/premultiplied key** mode (not additive)
- Anti-aliasing is FXAA (not TAA)

**Test procedure:** Output a simple white sphere on transparent background. On the switcher, key it over a color bars test pattern. Verify:
- Sphere edges are clean (no black halos = premultiplied alpha is correct)
- Background is fully transparent (color bars visible everywhere outside the sphere)
- No alpha flickering on edges

---

## 2. Live Telemetry Integration

### 2.1 Swapping CSV for Live Data

In Phase 1, the CSV reader implements `ITelemetryProvider`. In Phase 2, the client's Blueprint plugin implements the same interface. The swap is architectural — no changes to the event detector, banner system, or rendering pipeline.

**Client's integration steps:**
1. Open the project in their UE instance
2. Enable their proprietary telemetry plugin
3. Create (or use) an actor in their plugin that implements `ITelemetryProvider`
4. In that actor's Blueprint, implement `GetTelemetryData`:
   - Read their internal telemetry values
   - Populate the `FTelemetryInputData` struct
   - Return it
5. Place the actor in the level — our subsystem auto-discovers it
6. Remove or disable the CSV playback actor

### 2.2 What We Test Before Handoff

Using their simulated CSV data (Phase 1), we validate:
- All event detections fire at the correct mission times
- Banner positions match expected altitudes/positions
- Interpolation produces smooth motion at the correct rate
- Alpha output is clean through the DeckLink

When the client integrates their live plugin, they run the same mission profile and compare results against our CSV baseline. If the banners match, integration is correct.

### 2.3 Telemetry Update Rate Handling

The client confirmed 5–10Hz. Our interpolation system (built in Phase 1) handles this transparently. No additional work in Phase 2 — the interpolator doesn't care whether the data comes from CSV or live.

---

## 3. Camera Calibration

### 3.1 Why This Matters

The CG graphics must appear to exist in the same 3D space as the real rocket when composited at the switcher. The CG camera's perspective must match the physical camera's perspective. If they don't match, banners will appear to float at wrong positions relative to the rocket body in the composited output.

### 3.2 Parameters We Need From the Client

| Parameter | What It Is | How to Measure |
|---|---|---|
| Camera focal length | Real lens focal length in mm | Lens spec sheet |
| Camera sensor size | Sensor width × height in mm | Camera spec sheet |
| Horizontal FOV | Derived from focal length + sensor size | Compute or measure |
| Camera mounting position | Offset from rocket center (X, Y, Z in meters) | Physical measurement on the rocket |
| Camera mounting angle | Pitch/yaw/roll relative to rocket body axis | Physical measurement |
| Lens distortion | k1, k2, p1, p2 coefficients | OpenCV calibration (optional for Phase 2 — can correct in post) |

### 3.3 CG Camera Setup

The CG camera is set each frame:

```
CG Camera Position = ECEF_to_UE(VehiclePosition) + MountingOffset rotated by VehicleRotation
CG Camera Rotation = ECEF_Quat_to_UE(VehicleRotation) × MountingAngleOffset
CG Camera FOV = PhysicalCamera_HFOV
```

**Mounting offset** is exposed as editable Blueprint variables on the setup actor:
- `CameraMountOffset` (FVector, cm) — position relative to rocket center
- `CameraMountRotation` (FRotator, degrees) — angle relative to rocket axis
- `CameraFOV` (float, degrees) — horizontal field of view

The client (or an operator) can fine-tune these while watching the composited output on their switcher, nudging until the CG banners register correctly against the real rocket body.

---

## 4. Production Hardening

### 4.1 Failure Handling

| Failure | Detection | Response |
|---|---|---|
| Telemetry stops updating | No new data for > 0.5s | Extrapolate using last velocity for up to 5s. Flag `bTelemetryStale`. Show "PREDICTED" indicator in HUD. |
| Telemetry data is invalid | NaN check, non-normalized quaternion, position outside Earth + 1000km | Reject frame, hold last good data. Log warning. |
| Telemetry position jumps | Position changes > 500m between consecutive 5Hz updates (= 100km/s, physically impossible) | Reject frame, hold last good data. |
| DeckLink output failure | MediaCapture reports error | Attempt restart. Log error. HUD shows output status. |
| UE frame drop | DeltaTime > 20ms (< 50fps) | DeckLink repeats last output frame (no black frames to switcher). Log performance warning. |

### 4.2 Operator Controls

Keyboard shortcuts and/or a UMG panel:

| Control | Function |
|---|---|
| Space | Pause / resume (CSV mode only) |
| ← → | Nudge time ±1 second (CSV mode only) |
| [ ] | Time scale down / up (CSV mode only) |
| R | Reset (destroy all banners, restart from T-0) |
| D | Toggle dev visualization |
| F1 | Show/hide HUD overlay |
| F2 | Show/hide performance stats |
| Tab | Cycle through active banners (debug focus) |

### 4.3 HUD Overlay

A UMG widget showing current telemetry status, rendered as part of the alpha output:

- Mission elapsed time
- Altitude (AGL)
- Velocity (m/s)
- Mach number
- G-force
- Flight phase
- Telemetry status (LIVE / CSV / STALE / PREDICTED)

The HUD is toggle-able. In production, the client may want it off — their switcher may have its own telemetry display.

---

## 5. Implementation Steps (Phase 2)

### Step 1: DeckLink Fill/Key Setup (10–14 hrs)
- Configure DeckLink 8K Pro in Desktop Video Setup (fill/key pair, format)
- Create BlackmagicMediaOutput asset in the project
- Set up MediaCapture viewport output
- Validate fill/key output: test pattern keyed on switcher
- Alpha quality check (no halos, no bleed, clean edges)

### Step 2: Genlock + Timecode (6–10 hrs)
- Set up BlackmagicCustomTimeStep (genlock to house sync)
- Set up BlackmagicTimecodeProvider
- Verify UE frame rate locks to external sync
- Verify timecode on SDI output matches expectations

### Step 3: Live Telemetry Validation (8–12 hrs)
- Client integrates their plugin with our ITelemetryProvider interface
- Joint testing: client provides live (or simulated) telemetry, we verify events fire correctly
- Compare against CSV baseline
- Validate interpolation quality with live 5–10Hz data
- Test edge cases: telemetry dropout, data spikes, rapid orientation changes

### Step 4: Camera Registration (8–12 hrs)
- Client provides camera specs (focal length, sensor size, mounting offset)
- Configure CG camera parameters
- Fine-tune mounting offset against composited output on switcher
- Validate banner registration at multiple altitudes

### Step 5: Production Hardening (12–18 hrs)
- Implement all failure handling
- Operator controls (keyboard + optional UMG panel)
- HUD overlay
- Stress test: full simulated mission at real-time
- Performance profiling under load (many banners + DeckLink output)

### Step 6: Documentation + Handoff (6–8 hrs)
- Client integration guide with step-by-step screenshots
- Camera calibration procedure
- Operator manual for mission day
- Troubleshooting guide (common DeckLink issues, alpha problems)
- Project handoff (clean, documented, ready to open)

---

## 6. What the Client Receives

A complete UE 5.7 project file that:

1. Opens in their Unreal instance
2. Has a main level with the setup actor, Cesium georeference, and CG camera ready to go
3. Exposes a clear Blueprint interface for their telemetry plugin
4. Includes a CSV playback mode for testing without their plugin
5. Is pre-configured for DeckLink 8K Pro fill/key output (just select the correct device)
6. Outputs clean alpha CG graphics at 60fps
7. Detects flight events and spawns Earth-fixed text banners automatically
8. Has dev visualization mode for testing without a switcher
9. Includes comprehensive documentation

The client's only integration work is: implement `ITelemetryProvider` in their Blueprint plugin and enter their camera specs.
