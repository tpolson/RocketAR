# CSV Telemetry Format

## Header

```
MET,PosX,PosY,PosZ,RotX,RotY,RotZ,RotW,AccX,AccY,AccZ,VelX,VelY,VelZ,Thrust1,Thrust2,...
```

## Columns

| Column | Type | Units | Description |
|---|---|---|---|
| MET | float | seconds | Mission Elapsed Time (negative = countdown) |
| PosX, PosY, PosZ | float | meters | Vehicle ECEF position |
| RotX, RotY, RotZ, RotW | float | — | Vehicle orientation quaternion (XYZW, ECEF frame) |
| AccX, AccY, AccZ | float | m/s² | Acceleration in body frame |
| VelX, VelY, VelZ | float | m/s | Velocity in ECEF frame |
| Thrust1..N | float | 0.0–1.0 | Per-engine thrust percentage |

## Requirements

- **Header row required** — must contain "MET" and "PosX"
- **Minimum 14 columns** — MET through VelZ. Thrust columns are optional.
- **MET must be monotonically increasing** — out-of-order rows generate warnings
- **Quaternions are auto-normalized** — magnitude outside 0.9–1.1 generates a warning
- **NaN and Inf values** — rows with non-finite values are skipped with a warning
- **Empty rows** — silently skipped
- **Recommended rate** — 10 Hz (0.1s between rows)

## Engine Thrust Array Order

Default SLS configuration: `Thrust1=SRB1, Thrust2=SRB2, Thrust3-6=Core, Thrust7=Stage2`

Adjust `SRBEngineCount` and `CoreEngineCount` in `FFlightEventConfig` if your vehicle differs.

## Example

```csv
MET,PosX,PosY,PosZ,RotX,RotY,RotZ,RotW,AccX,AccY,AccZ,VelX,VelY,VelZ,Thrust1,Thrust2,Thrust3,Thrust4,Thrust5,Thrust6,Thrust7
-10.00,920267.123456,-5360445.123456,3020569.123456,0.000000,0.000000,0.000000,1.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.0000,0.0000,0.0000,0.0000,0.0000,0.0000,0.0000
0.00,920267.123456,-5360445.123456,3020569.123456,0.000000,0.000000,0.000000,1.000000,0.000000,0.000000,9.810000,0.9500,0.9500,0.8500,0.8500,0.8500,0.8500,0.0000
```

## Generating Test Data

Use `Tools/generate_telemetry.py` to generate or customize synthetic SLS-like data:

```bash
python3 Tools/generate_telemetry.py                    # default output
python3 Tools/generate_telemetry.py custom_output.csv  # custom path
```
