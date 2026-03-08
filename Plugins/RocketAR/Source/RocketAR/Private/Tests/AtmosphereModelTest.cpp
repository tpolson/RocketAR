#include "Misc/AutomationTest.h"
#include "AtmosphereModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtmosphereModelSeaLevelTest,
	"RocketAR.Atmosphere.SeaLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAtmosphereModelSeaLevelTest::RunTest(const FString& Parameters)
{
	// US Standard Atmosphere sea level values
	const double Temp = UAtmosphereModel::GetTemperature(0.0);
	TestTrue(TEXT("Sea level temperature ~288.15K"), FMath::IsNearlyEqual(Temp, 288.15, 0.1));

	const double Pressure = UAtmosphereModel::GetPressure(0.0);
	TestTrue(TEXT("Sea level pressure ~101325 Pa"), FMath::IsNearlyEqual(Pressure, 101325.0, 10.0));

	const double Density = UAtmosphereModel::GetDensity(0.0);
	TestTrue(TEXT("Sea level density ~1.225 kg/m3"), FMath::IsNearlyEqual(Density, 1.225, 0.01));

	const double SoS = UAtmosphereModel::GetSpeedOfSound(0.0);
	TestTrue(TEXT("Sea level speed of sound ~340.3 m/s"), FMath::IsNearlyEqual(SoS, 340.3, 1.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtmosphereModel11kmTest,
	"RocketAR.Atmosphere.Tropopause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAtmosphereModel11kmTest::RunTest(const FString& Parameters)
{
	// At 11km (tropopause)
	const double Temp = UAtmosphereModel::GetTemperature(11000.0);
	TestTrue(TEXT("11km temperature ~216.65K"), FMath::IsNearlyEqual(Temp, 216.65, 1.0));

	const double Pressure = UAtmosphereModel::GetPressure(11000.0);
	TestTrue(TEXT("11km pressure ~22632 Pa"), FMath::IsNearlyEqual(Pressure, 22632.0, 100.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtmosphereModelHighAltTest,
	"RocketAR.Atmosphere.HighAltitude",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAtmosphereModelHighAltTest::RunTest(const FString& Parameters)
{
	// Above 86km — atmosphere model returns zero density
	const double Density = UAtmosphereModel::GetDensity(100000.0);
	TestTrue(TEXT("100km density ~0"), FMath::IsNearlyEqual(Density, 0.0, 0.001));

	const double SoS = UAtmosphereModel::GetSpeedOfSound(100000.0);
	TestTrue(TEXT("100km speed of sound = 0"), FMath::IsNearlyEqual(SoS, 0.0, 0.001));

	const double Mach = UAtmosphereModel::GetMachNumber(100000.0, 7000.0);
	TestTrue(TEXT("100km Mach = 0 (undefined)"), FMath::IsNearlyEqual(Mach, 0.0, 0.001));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtmosphereModelDynPressureTest,
	"RocketAR.Atmosphere.DynamicPressure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAtmosphereModelDynPressureTest::RunTest(const FString& Parameters)
{
	// Q = 0.5 * rho * v^2
	// At sea level, 100 m/s: Q = 0.5 * 1.225 * 10000 = 6125 Pa
	const double Q = UAtmosphereModel::GetDynamicPressure(0.0, 100.0);
	TestTrue(TEXT("Sea level 100m/s dynamic pressure ~6125 Pa"), FMath::IsNearlyEqual(Q, 6125.0, 50.0));

	return true;
}
