#!/usr/bin/env python3
"""
Generate synthetic SLS-like ascent telemetry CSV for RocketAR development.

Produces a realistic trajectory from KSC launch pad through:
  Ignition → Liftoff → Gravity Turn → Mach 1 → Max-Q → SRB Sep → MECO →
  Coast → Second Stage Ignition → Fairing Sep → SECO → Apogee

All positions in ECEF (meters), quaternion XYZW, 10Hz output.
"""

import csv
import math
import sys
import os

# WGS84 constants
WGS84_A = 6378137.0        # semi-major axis (m)
WGS84_B = 6356752.314245   # semi-minor axis (m)
WGS84_E2 = 0.00669437999014

# KSC Launch Complex 39A
LAUNCH_LAT = math.radians(28.5729)
LAUNCH_LON = math.radians(-80.6490)
LAUNCH_ALT = 0.0

# Time parameters
DT = 0.1  # 10 Hz
MET_START = -10.0  # T-10 countdown
MET_END = 600.0    # 10 minutes of flight

# SLS-like flight profile parameters
SRB_THRUST = 0.95       # SRB thrust level
CORE_THRUST = 0.85      # Core stage thrust
LIFTOFF_MET = 0.0
GRAVITY_TURN_MET = 15.0
MACH1_APPROX_MET = 60.0
MAXQ_APPROX_MET = 80.0
SRB_SEP_MET = 126.0
MECO_MET = 480.0
STAGE_SEP_MET = 486.0
SECOND_STAGE_IGN_MET = 496.0
FAIRING_SEP_MET = 220.0  # fairing jettison
SECO_MET = 570.0


def geodetic_to_ecef(lat, lon, alt):
    """Convert geodetic (rad, rad, m) to ECEF (m)."""
    sin_lat = math.sin(lat)
    cos_lat = math.cos(lat)
    sin_lon = math.sin(lon)
    cos_lon = math.cos(lon)
    N = WGS84_A / math.sqrt(1.0 - WGS84_E2 * sin_lat * sin_lat)
    x = (N + alt) * cos_lat * cos_lon
    y = (N + alt) * cos_lat * sin_lon
    z = (N * (1.0 - WGS84_E2) + alt) * sin_lat
    return (x, y, z)


def euler_to_quat(pitch_rad, yaw_rad, roll_rad):
    """Convert Euler angles to quaternion (X, Y, Z, W)."""
    cp = math.cos(pitch_rad / 2)
    sp = math.sin(pitch_rad / 2)
    cy = math.cos(yaw_rad / 2)
    sy = math.sin(yaw_rad / 2)
    cr = math.cos(roll_rad / 2)
    sr = math.sin(roll_rad / 2)

    w = cr * cp * cy + sr * sp * sy
    x = sr * cp * cy - cr * sp * sy
    y = cr * sp * cy + sr * cp * sy
    z = cr * cp * sy - sr * sp * cy
    return (x, y, z, w)


def us_std_atm_density(alt_m):
    """Simple US Standard Atmosphere density model."""
    if alt_m < 0:
        alt_m = 0
    if alt_m > 86000:
        return 0.0
    if alt_m <= 11000:
        T = 288.15 - 0.0065 * alt_m
        P = 101325 * (T / 288.15) ** (9.80665 * 0.0289644 / (8.31447 * 0.0065))
    elif alt_m <= 20000:
        T = 216.65
        P = 22632.10 * math.exp(-9.80665 * 0.0289644 * (alt_m - 11000) / (8.31447 * 216.65))
    elif alt_m <= 32000:
        T = 216.65 + 0.001 * (alt_m - 20000)
        P = 5474.89 * (216.65 / T) ** (9.80665 * 0.0289644 / (8.31447 * 0.001))
    else:
        T = 228.65 + 0.0028 * (alt_m - 32000)
        P = 868.019 * (228.65 / T) ** (9.80665 * 0.0289644 / (8.31447 * 0.0028))
    rho = P / (287.058 * T)
    return rho


def generate_trajectory():
    """Generate a synthetic SLS-like ascent trajectory."""
    rows = []

    # Launch pad ECEF
    pad_ecef = geodetic_to_ecef(LAUNCH_LAT, LAUNCH_LON, LAUNCH_ALT)

    # State variables (local ENU frame, then convert to ECEF)
    alt = 0.0       # meters above pad
    downrange = 0.0 # meters east
    crossrange = 0.0

    vel_up = 0.0
    vel_east = 0.0

    pitch = math.radians(90.0)  # Start vertical

    t = MET_START
    while t <= MET_END:
        # Determine flight phase and compute accelerations
        thrust_srb = 0.0
        thrust_core = 0.0
        thrust_s2 = 0.0

        if t < 0:
            # Countdown — sitting on pad
            alt = 0.0
            vel_up = 0.0
            vel_east = 0.0
            ax, ay, az = 0.0, 0.0, 0.0
        elif t < LIFTOFF_MET + 0.5:
            # Ignition to liftoff
            thrust_core = CORE_THRUST * min(1.0, (t - LIFTOFF_MET + 3.0) / 3.0)
            thrust_srb = SRB_THRUST if t >= -6.0 else 0.0
            # SRBs ignite at T+0, but we simplify
            if t >= 0:
                thrust_srb = SRB_THRUST

            total_accel = 15.0 * (thrust_core + thrust_srb) - 9.81  # simplified
            if total_accel < 0:
                total_accel = 0
            ax = 0.0
            ay = 0.0
            az = total_accel
        elif t < SRB_SEP_MET:
            # Powered ascent with SRBs
            thrust_core = CORE_THRUST
            thrust_srb = SRB_THRUST * max(0, 1.0 - (t - 100.0) / 30.0) if t > 100 else SRB_THRUST

            # Gravity turn — pitch gradually
            if t > GRAVITY_TURN_MET:
                turn_frac = min(1.0, (t - GRAVITY_TURN_MET) / 300.0)
                target_pitch = math.radians(90.0 - 70.0 * turn_frac)
                pitch = pitch + (target_pitch - pitch) * 0.02

            total_thrust = 12.0 * (thrust_core + thrust_srb)

            az = total_thrust * math.sin(pitch) - 9.81
            ax = total_thrust * math.cos(pitch)
            ay = 0.0

        elif t < MECO_MET:
            # Core stage only (post SRB sep)
            thrust_core = CORE_THRUST
            thrust_srb = 0.0

            turn_frac = min(1.0, (t - GRAVITY_TURN_MET) / 300.0)
            target_pitch = math.radians(90.0 - 70.0 * turn_frac)
            pitch = pitch + (target_pitch - pitch) * 0.02

            total_thrust = 8.0 * thrust_core
            az = total_thrust * math.sin(pitch) - 9.81
            ax = total_thrust * math.cos(pitch)
            ay = 0.0

        elif t < SECOND_STAGE_IGN_MET:
            # Coast phase
            thrust_core = 0.0
            thrust_srb = 0.0
            az = -9.81 * (WGS84_A / (WGS84_A + alt)) ** 2  # gravity decreases with altitude
            ax = 0.0
            ay = 0.0

        elif t < SECO_MET:
            # Second stage burn
            thrust_s2 = 0.90
            total_thrust = 5.0 * thrust_s2
            turn_frac = min(1.0, (t - GRAVITY_TURN_MET) / 300.0)
            target_pitch = math.radians(90.0 - 85.0 * turn_frac)
            pitch = pitch + (target_pitch - pitch) * 0.01

            az = total_thrust * math.sin(pitch) - 9.81 * (WGS84_A / (WGS84_A + alt)) ** 2
            ax = total_thrust * math.cos(pitch)
            ay = 0.0
        else:
            # Post SECO — coast to apogee
            thrust_s2 = 0.0
            az = -9.81 * (WGS84_A / (WGS84_A + alt)) ** 2
            ax = 0.0
            ay = 0.0

        # Integrate (simple Euler)
        if t >= 0:
            vel_up += az * DT
            vel_east += ax * DT
            alt += vel_up * DT
            downrange += vel_east * DT

            if alt < 0:
                alt = 0
                vel_up = 0

        # Convert local position to ECEF
        # Local frame: East=downrange, North=crossrange, Up=altitude
        sin_lat = math.sin(LAUNCH_LAT)
        cos_lat = math.cos(LAUNCH_LAT)
        sin_lon = math.sin(LAUNCH_LON)
        cos_lon = math.cos(LAUNCH_LON)

        # ENU to ECEF rotation
        ecef_x = pad_ecef[0] + (-sin_lon * downrange - sin_lat * cos_lon * crossrange + cos_lat * cos_lon * alt)
        ecef_y = pad_ecef[1] + (cos_lon * downrange - sin_lat * sin_lon * crossrange + cos_lat * sin_lon * alt)
        ecef_z = pad_ecef[2] + (cos_lat * crossrange + sin_lat * alt)

        # Velocity in ECEF (approximate)
        vel_ecef_x = -sin_lon * vel_east + cos_lat * cos_lon * vel_up
        vel_ecef_y = cos_lon * vel_east + cos_lat * sin_lon * vel_up
        vel_ecef_z = sin_lat * vel_up

        # Acceleration in body frame (simplified — mostly axial)
        total_accel = math.sqrt(ax * ax + ay * ay + az * az)
        body_accel = (ax, ay, az + 9.81)  # Remove gravity to get body-frame

        # Quaternion: rocket pointing along pitch angle from vertical
        yaw = math.atan2(downrange, max(alt, 1.0))
        quat = euler_to_quat(pitch - math.radians(90), 0.0, 0.0)

        # Build thrust array: [SRB1, SRB2, Core1, Core2, Core3, Core4, Stage2]
        thrusts = [
            thrust_srb, thrust_srb,
            thrust_core, thrust_core, thrust_core, thrust_core,
            thrust_s2
        ]

        rows.append({
            'MET': round(t, 2),
            'PosX': ecef_x,
            'PosY': ecef_y,
            'PosZ': ecef_z,
            'RotX': quat[0],
            'RotY': quat[1],
            'RotZ': quat[2],
            'RotW': quat[3],
            'AccX': body_accel[0],
            'AccY': body_accel[1],
            'AccZ': body_accel[2],
            'VelX': vel_ecef_x,
            'VelY': vel_ecef_y,
            'VelZ': vel_ecef_z,
            'Thrust1': thrusts[0],
            'Thrust2': thrusts[1],
            'Thrust3': thrusts[2],
            'Thrust4': thrusts[3],
            'Thrust5': thrusts[4],
            'Thrust6': thrusts[5],
            'Thrust7': thrusts[6],
        })

        t = round(t + DT, 2)

    return rows


def write_csv(rows, filepath):
    """Write trajectory rows to CSV."""
    if not rows:
        return

    fieldnames = list(rows[0].keys())
    with open(filepath, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            # Format floats
            formatted = {}
            for k, v in row.items():
                if isinstance(v, float):
                    if k == 'MET':
                        formatted[k] = f'{v:.2f}'
                    elif k.startswith('Thrust'):
                        formatted[k] = f'{v:.4f}'
                    else:
                        formatted[k] = f'{v:.6f}'
                else:
                    formatted[k] = v
            writer.writerow(formatted)


def main():
    output_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                'Content', 'Data', 'SimulatedTelemetry.csv')

    if len(sys.argv) > 1:
        output_path = sys.argv[1]

    print(f"Generating SLS-like telemetry data...")
    rows = generate_trajectory()
    print(f"Generated {len(rows)} rows (MET {rows[0]['MET']:.1f}s to {rows[-1]['MET']:.1f}s)")

    # Print key events timing
    for row in rows:
        met = row['MET']
        alt_approx = math.sqrt(row['PosX']**2 + row['PosY']**2 + row['PosZ']**2) - WGS84_A
        vel_mag = math.sqrt(row['VelX']**2 + row['VelY']**2 + row['VelZ']**2)

    write_csv(rows, output_path)
    print(f"Written to: {output_path}")

    # Summary
    last = rows[-1]
    final_alt = math.sqrt(last['PosX']**2 + last['PosY']**2 + last['PosZ']**2) - WGS84_A
    final_vel = math.sqrt(last['VelX']**2 + last['VelY']**2 + last['VelZ']**2)
    print(f"Final altitude: {final_alt/1000:.1f} km")
    print(f"Final velocity: {final_vel:.0f} m/s")


if __name__ == '__main__':
    main()
