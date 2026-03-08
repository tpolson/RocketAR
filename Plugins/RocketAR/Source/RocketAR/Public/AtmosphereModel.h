#pragma once

#include "CoreMinimal.h"
#include "AtmosphereModel.generated.h"

/**
 * US Standard Atmosphere 1976 model.
 * 7-layer piecewise computation of temperature, pressure, density, and speed of sound
 * as functions of geometric altitude (meters ASL).
 * Above 86km, density approaches zero and Mach number becomes undefined.
 */
UCLASS(BlueprintType)
class ROCKETAR_API UAtmosphereModel : public UObject
{
	GENERATED_BODY()

public:
	/** Temperature in Kelvin at the given altitude (meters ASL) */
	UFUNCTION(BlueprintCallable, Category = "Atmosphere")
	static double GetTemperature(double AltitudeMeters);

	/** Pressure in Pascals at the given altitude (meters ASL) */
	UFUNCTION(BlueprintCallable, Category = "Atmosphere")
	static double GetPressure(double AltitudeMeters);

	/** Air density in kg/m^3 at the given altitude (meters ASL) */
	UFUNCTION(BlueprintCallable, Category = "Atmosphere")
	static double GetDensity(double AltitudeMeters);

	/** Speed of sound in m/s at the given altitude (meters ASL) */
	UFUNCTION(BlueprintCallable, Category = "Atmosphere")
	static double GetSpeedOfSound(double AltitudeMeters);

	/** Dynamic pressure in Pa given altitude and velocity magnitude */
	UFUNCTION(BlueprintCallable, Category = "Atmosphere")
	static double GetDynamicPressure(double AltitudeMeters, double VelocityMagnitude);

	/** Mach number. Returns 0 above 86km. */
	UFUNCTION(BlueprintCallable, Category = "Atmosphere")
	static double GetMachNumber(double AltitudeMeters, double VelocityMagnitude);

private:
	// US Standard Atmosphere 1976 layer boundaries and lapse rates
	struct FAtmosphereLayer
	{
		double BaseAltitude;   // meters (geopotential)
		double TopAltitude;    // meters (geopotential)
		double BaseTemperature;// Kelvin
		double LapseRate;      // K/m (negative = temperature decreases with altitude)
		double BasePressure;   // Pascals
	};

	static const TArray<FAtmosphereLayer>& GetLayers();
	static int32 FindLayerIndex(double GeopotentialAltitude);
	static double GeometricToGeopotential(double GeometricAltitude);

	static constexpr double EarthRadius = 6356766.0;     // m (for geopotential conversion)
	static constexpr double GravAccel = 9.80665;          // m/s^2
	static constexpr double MolarMassAir = 0.0289644;     // kg/mol
	static constexpr double GasConstant = 8.31447;        // J/(mol·K)
	static constexpr double SpecificHeatRatio = 1.4;      // for dry air
	static constexpr double SpecificGasConstant = 287.058; // J/(kg·K)
	static constexpr double MaxAtmosphereAltitude = 86000.0; // meters
};
