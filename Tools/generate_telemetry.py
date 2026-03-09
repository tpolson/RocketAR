#!/usr/bin/env python3
"""
Generate synthetic SLS-like ascent telemetry CSV for RocketAR development.

Produces a realistic trajectory through:
  Ignition -> Liftoff -> Gravity Turn -> Mach 1 -> Max-Q -> SRB Sep -> MECO ->
  Coast -> Second Stage Ignition -> Fairing Sep -> SECO -> Apogee

All positions in ECEF (meters), quaternion XYZW, 10Hz output.

Usage:
  python generate_telemetry.py [options]

Options:
  --site NAME        Preset launch site: ksc, vandenberg, wallops, boca-chica
  --lat DEGREES      Custom launch latitude (decimal degrees)
  --lon DEGREES      Custom launch longitude (decimal degrees)
  --alt METERS       Custom launch altitude ASL (default: 0)
  --azimuth DEGREES  Launch azimuth from north (default: auto from site)
  --output PATH      Output CSV path (default: Content/Data/SimulatedTelemetry.csv)
  --help             Show this help
"""

import argparse
import csv
import math
import sys
import os

# WGS84 constants
WGS84_A = 6378137.0        # semi-major axis (m)
WGS84_B = 6356752.314245   # semi-minor axis (m)
WGS84_E2 = 0.00669437999014

# Preset launch sites: (lat_deg, lon_deg, alt_m, azimuth_deg, description)
LAUNCH_SITES = {
    'ksc': (28.5729, -80.6490, 0.0, 90.0,
            'Kennedy Space Center LC-39A, Florida'),
    'vandenberg': (34.5811, -120.6257, 150.0, 196.0,
                   'Vandenberg SFB SLC-6, California'),
    'wallops': (37.8337, -75.4881, 0.0, 90.0,
                'Wallops Flight Facility, Virginia'),
    'boca-chica': (25.9972, -97.1571, 0.0, 97.0,
                   'SpaceX Starbase, Boca Chica, Texas'),
}

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


def generate_trajectory(launch_lat_rad, launch_lon_rad, launch_alt, azimuth_rad):
    """Generate a synthetic SLS-like ascent trajectory from given launch site."""
    rows = []

    # Launch pad ECEF
    pad_ecef = geodetic_to_ecef(launch_lat_rad, launch_lon_rad, launch_alt)

    # Precompute ENU basis vectors at launch site
    sin_lat = math.sin(launch_lat_rad)
    cos_lat = math.cos(launch_lat_rad)
    sin_lon = math.sin(launch_lon_rad)
    cos_lon = math.cos(launch_lon_rad)

    # Downrange direction in ENU depends on launch azimuth
    # Azimuth 0=North, 90=East, 180=South, 270=West
    az_sin = math.sin(azimuth_rad)
    az_cos = math.cos(azimuth_rad)

    # State variables (local frame: downrange along azimuth, crossrange perpendicular, up)
    alt = 0.0       # meters above pad
    downrange = 0.0 # meters along azimuth
    crossrange = 0.0

    vel_up = 0.0
    vel_downrange = 0.0

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
            vel_downrange = 0.0
            a_downrange, a_cross, a_up = 0.0, 0.0, 0.0
        elif t < LIFTOFF_MET + 0.5:
            # Ignition to liftoff
            thrust_core = CORE_THRUST * min(1.0, (t - LIFTOFF_MET + 3.0) / 3.0)
            thrust_srb = SRB_THRUST if t >= -6.0 else 0.0
            if t >= 0:
                thrust_srb = SRB_THRUST

            total_accel = 15.0 * (thrust_core + thrust_srb) - 9.81
            if total_accel < 0:
                total_accel = 0
            a_downrange = 0.0
            a_cross = 0.0
            a_up = total_accel
        elif t < SRB_SEP_MET:
            # Powered ascent with SRBs
            thrust_core = CORE_THRUST
            thrust_srb = SRB_THRUST * max(0, 1.0 - (t - 100.0) / 30.0) if t > 100 else SRB_THRUST

            if t > GRAVITY_TURN_MET:
                turn_frac = min(1.0, (t - GRAVITY_TURN_MET) / 300.0)
                target_pitch = math.radians(90.0 - 70.0 * turn_frac)
                pitch = pitch + (target_pitch - pitch) * 0.02

            total_thrust = 12.0 * (thrust_core + thrust_srb)
            a_up = total_thrust * math.sin(pitch) - 9.81
            a_downrange = total_thrust * math.cos(pitch)
            a_cross = 0.0

        elif t < MECO_MET:
            # Core stage only (post SRB sep)
            thrust_core = CORE_THRUST
            thrust_srb = 0.0

            turn_frac = min(1.0, (t - GRAVITY_TURN_MET) / 300.0)
            target_pitch = math.radians(90.0 - 70.0 * turn_frac)
            pitch = pitch + (target_pitch - pitch) * 0.02

            total_thrust = 8.0 * thrust_core
            a_up = total_thrust * math.sin(pitch) - 9.81
            a_downrange = total_thrust * math.cos(pitch)
            a_cross = 0.0

        elif t < SECOND_STAGE_IGN_MET:
            # Coast phase
            thrust_core = 0.0
            thrust_srb = 0.0
            a_up = -9.81 * (WGS84_A / (WGS84_A + alt)) ** 2
            a_downrange = 0.0
            a_cross = 0.0

        elif t < SECO_MET:
            # Second stage burn
            thrust_s2 = 0.90
            total_thrust = 5.0 * thrust_s2
            turn_frac = min(1.0, (t - GRAVITY_TURN_MET) / 300.0)
            target_pitch = math.radians(90.0 - 85.0 * turn_frac)
            pitch = pitch + (target_pitch - pitch) * 0.01

            a_up = total_thrust * math.sin(pitch) - 9.81 * (WGS84_A / (WGS84_A + alt)) ** 2
            a_downrange = total_thrust * math.cos(pitch)
            a_cross = 0.0
        else:
            # Post SECO — coast to apogee
            thrust_s2 = 0.0
            a_up = -9.81 * (WGS84_A / (WGS84_A + alt)) ** 2
            a_downrange = 0.0
            a_cross = 0.0

        # Integrate (simple Euler)
        if t >= 0:
            vel_up += a_up * DT
            vel_downrange += a_downrange * DT
            alt += vel_up * DT
            downrange += vel_downrange * DT

            if alt < 0:
                alt = 0
                vel_up = 0

        # Convert local (downrange along azimuth, crossrange, up) to ENU
        enu_e = az_sin * downrange - az_cos * crossrange
        enu_n = az_cos * downrange + az_sin * crossrange
        enu_u = alt

        # ENU to ECEF
        ecef_x = pad_ecef[0] + (-sin_lon * enu_e - sin_lat * cos_lon * enu_n + cos_lat * cos_lon * enu_u)
        ecef_y = pad_ecef[1] + (cos_lon * enu_e - sin_lat * sin_lon * enu_n + cos_lat * sin_lon * enu_u)
        ecef_z = pad_ecef[2] + (cos_lat * enu_n + sin_lat * enu_u)

        # Velocity: local downrange/up to ENU then to ECEF
        vel_enu_e = az_sin * vel_downrange
        vel_enu_n = az_cos * vel_downrange
        vel_enu_u = vel_up

        vel_ecef_x = -sin_lon * vel_enu_e - sin_lat * cos_lon * vel_enu_n + cos_lat * cos_lon * vel_enu_u
        vel_ecef_y = cos_lon * vel_enu_e - sin_lat * sin_lon * vel_enu_n + cos_lat * sin_lon * vel_enu_u
        vel_ecef_z = cos_lat * vel_enu_n + sin_lat * vel_enu_u

        # Acceleration in body frame (simplified — mostly axial)
        body_accel = (a_downrange, a_cross, a_up + 9.81)  # Remove gravity to get body-frame

        # Quaternion: rocket pointing along pitch angle from vertical
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


def parse_args():
    parser = argparse.ArgumentParser(
        description='Generate synthetic SLS-like ascent telemetry CSV.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='Preset sites: ' + ', '.join(
            f'{k} ({v[4]})' for k, v in LAUNCH_SITES.items()))

    parser.add_argument('--site', type=str, default=None,
                        choices=list(LAUNCH_SITES.keys()),
                        help='Preset launch site name')
    parser.add_argument('--lat', type=float, default=None,
                        help='Launch latitude (decimal degrees)')
    parser.add_argument('--lon', type=float, default=None,
                        help='Launch longitude (decimal degrees)')
    parser.add_argument('--alt', type=float, default=None,
                        help='Launch altitude ASL (meters)')
    parser.add_argument('--azimuth', type=float, default=None,
                        help='Launch azimuth from north (degrees, 90=east)')
    parser.add_argument('--output', type=str, default=None,
                        help='Output CSV file path')

    return parser.parse_args()


def main():
    args = parse_args()

    # Determine launch site
    if args.site:
        site = LAUNCH_SITES[args.site]
        lat_deg, lon_deg, alt_m, azimuth_deg, desc = site
        print(f"Launch site: {desc}")
    else:
        # Default to KSC
        lat_deg = 28.5729
        lon_deg = -80.6490
        alt_m = 0.0
        azimuth_deg = 90.0
        desc = 'Kennedy Space Center LC-39A (default)'
        print(f"Launch site: {desc}")

    # Override with explicit args
    if args.lat is not None:
        lat_deg = args.lat
    if args.lon is not None:
        lon_deg = args.lon
    if args.alt is not None:
        alt_m = args.alt
    if args.azimuth is not None:
        azimuth_deg = args.azimuth

    print(f"  Lat: {lat_deg:.4f}  Lon: {lon_deg:.4f}  Alt: {alt_m:.0f}m  Azimuth: {azimuth_deg:.0f} deg")

    launch_lat_rad = math.radians(lat_deg)
    launch_lon_rad = math.radians(lon_deg)
    azimuth_rad = math.radians(azimuth_deg)

    output_path = args.output
    if output_path is None:
        output_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                    'Content', 'Data', 'SimulatedTelemetry.csv')

    print(f"Generating SLS-like telemetry data...")
    rows = generate_trajectory(launch_lat_rad, launch_lon_rad, alt_m, azimuth_rad)
    print(f"Generated {len(rows)} rows (MET {rows[0]['MET']:.1f}s to {rows[-1]['MET']:.1f}s)")

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
