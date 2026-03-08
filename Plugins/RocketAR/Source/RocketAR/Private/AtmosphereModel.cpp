#include "AtmosphereModel.h"

const TArray<UAtmosphereModel::FAtmosphereLayer>& UAtmosphereModel::GetLayers()
{
	// US Standard Atmosphere 1976 — 7 layers from sea level to 86km
	// BaseAltitude, TopAltitude (geopotential meters), BaseTemperature (K), LapseRate (K/m), BasePressure (Pa)
	static const TArray<FAtmosphereLayer> Layers = {
		{     0.0, 11000.0, 288.15,  -0.0065,  101325.0    },  // Troposphere
		{ 11000.0, 20000.0, 216.65,   0.0,      22632.10   },  // Tropopause
		{ 20000.0, 32000.0, 216.65,   0.001,    5474.89    },  // Stratosphere 1
		{ 32000.0, 47000.0, 228.65,   0.0028,   868.019    },  // Stratosphere 2
		{ 47000.0, 51000.0, 270.65,   0.0,      110.906    },  // Stratopause
		{ 51000.0, 71000.0, 270.65,  -0.0028,   66.9389    },  // Mesosphere 1
		{ 71000.0, 84852.0, 214.65,  -0.002,    3.95642    },  // Mesosphere 2
	};
	return Layers;
}

double UAtmosphereModel::GeometricToGeopotential(double GeometricAltitude)
{
	return (EarthRadius * GeometricAltitude) / (EarthRadius + GeometricAltitude);
}

int32 UAtmosphereModel::FindLayerIndex(double GeopotentialAltitude)
{
	const auto& Layers = GetLayers();
	for (int32 i = Layers.Num() - 1; i >= 0; --i)
	{
		if (GeopotentialAltitude >= Layers[i].BaseAltitude)
		{
			return i;
		}
	}
	return 0;
}

double UAtmosphereModel::GetTemperature(double AltitudeMeters)
{
	if (AltitudeMeters < 0.0) AltitudeMeters = 0.0;
	if (AltitudeMeters >= MaxAtmosphereAltitude) return 186.87; // approximate at 86km

	const double H = GeometricToGeopotential(AltitudeMeters);
	const auto& Layers = GetLayers();
	const int32 Idx = FindLayerIndex(H);
	const auto& Layer = Layers[Idx];

	return Layer.BaseTemperature + Layer.LapseRate * (H - Layer.BaseAltitude);
}

double UAtmosphereModel::GetPressure(double AltitudeMeters)
{
	if (AltitudeMeters < 0.0) AltitudeMeters = 0.0;
	if (AltitudeMeters >= MaxAtmosphereAltitude) return 0.0;

	const double H = GeometricToGeopotential(AltitudeMeters);
	const auto& Layers = GetLayers();
	const int32 Idx = FindLayerIndex(H);
	const auto& Layer = Layers[Idx];

	const double DeltaH = H - Layer.BaseAltitude;

	if (FMath::IsNearlyZero(Layer.LapseRate))
	{
		// Isothermal layer: P = Pb * exp(-g*M*dH / (R*Tb))
		return Layer.BasePressure * FMath::Exp(
			-(GravAccel * MolarMassAir * DeltaH) / (GasConstant * Layer.BaseTemperature));
	}
	else
	{
		// Gradient layer: P = Pb * (Tb / (Tb + L*dH))^(g*M / (R*L))
		const double Exponent = (GravAccel * MolarMassAir) / (GasConstant * Layer.LapseRate);
		return Layer.BasePressure * FMath::Pow(
			Layer.BaseTemperature / (Layer.BaseTemperature + Layer.LapseRate * DeltaH),
			Exponent);
	}
}

double UAtmosphereModel::GetDensity(double AltitudeMeters)
{
	if (AltitudeMeters >= MaxAtmosphereAltitude) return 0.0;

	const double P = GetPressure(AltitudeMeters);
	const double T = GetTemperature(AltitudeMeters);

	if (T <= 0.0) return 0.0;

	// Ideal gas: rho = P / (R_specific * T)
	return P / (SpecificGasConstant * T);
}

double UAtmosphereModel::GetSpeedOfSound(double AltitudeMeters)
{
	if (AltitudeMeters >= MaxAtmosphereAltitude) return 0.0;

	const double T = GetTemperature(AltitudeMeters);
	if (T <= 0.0) return 0.0;

	// a = sqrt(gamma * R_specific * T)
	return FMath::Sqrt(SpecificHeatRatio * SpecificGasConstant * T);
}

double UAtmosphereModel::GetDynamicPressure(double AltitudeMeters, double VelocityMagnitude)
{
	const double Rho = GetDensity(AltitudeMeters);
	return 0.5 * Rho * VelocityMagnitude * VelocityMagnitude;
}

double UAtmosphereModel::GetMachNumber(double AltitudeMeters, double VelocityMagnitude)
{
	const double SoS = GetSpeedOfSound(AltitudeMeters);
	if (SoS <= 0.0) return 0.0;
	return VelocityMagnitude / SoS;
}
