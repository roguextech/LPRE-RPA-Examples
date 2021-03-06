/*
 * RPA - Tool for Rocket Propulsion Analysis
 * RPA Software Development Kit (SDK)
 *
 * Copyright 2009,2015 Alexander Ponomarenko.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This software is a commercial product; you can use it under the terms of the
 * RPA SDK License as published at http://www.propulsion-analysis.com/sdk_eula.htm
 *
 * This program is distributed WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the RPA SDK License for more details (a copy is included
 * in the sdk_eula.htm file that accompanied this program).
 *
 * You should have received a copy of the RPA SDK License along with this program;
 * if not, write to author <contact@propulsion-analysis.com>
 *
 * Please contact author <contact@propulsion-analysis.com> or visit http://www.propulsion-analysis.com
 * if you need additional information or have any questions.
 */

#include "utils/Util.hpp"
#include "thermodynamics/input/Input.hpp"
#include "thermodynamics/thermo/Thermodynamics.hpp"
#include "thermodynamics/dll/Export.hpp"
#include "thermodynamics/gasdynamics/StandardAtmosphere.hpp"

#include "common.hpp"

/**
 * This example calculates the performance of rocket engine at given altitude and throttle value.
 */
int main(int argc, char* argv[]) {

	util::Log::createLog("ROOT")->
		addLogger(new util::FileLogger("", 10*1024));

	// Initialize thermodatabase
	initThermoDatabase();

	bool applyCorrectionFactor = true;
	bool checkFlowSeparation = true;
	bool optimizePropellant = false;

	// Initialize configuration file object
	thermo::input::ConfigFile* data = new thermo::input::ConfigFile("examples/RD-170.cfg");

	// Read configuration file
	data->read();

	// Modify input data, if necessary
	// For instance:
	//
	// Set nozzle area ratio
	// data->getNozzleFlowOptions().setNozzleExitConditions().setAreaRatio(30, true);
	//
	// Set propellant O/F ratio (km==O/F):
	// data->getPropellant().setRatio(2.5, thermo::input::Ratio::km);
	//
	// Set chamber pressure:
	// data->getCombustionChamberConditions().setPressure(15, thermo::input::Pressure::MPa);
	//
	// etc.

	// Define ambient conditions:
	// Current altitude, meters
	double H = 10e3;	// 10 km

	double throttleValue = 0.75;

	// Initialize performance solver
	performance::TheoreticalPerformance* performance = new performance::TheoreticalPerformance(data, false);

	if (optimizePropellant && thermo::input::Ratio::fractions!=data->getPropellant().getRatioType()) {
		// Find optimal mixture ratio for given propellant
		performance->optimizeForSpecificImpulse();
	} else {
		performance->solve();
	}

	performance::ThrottlingPerformance* throttlingPerformance = new performance::ThrottlingPerformance(performance, throttleValue);

	// Get combustion chamber object
	performance::equilibrium::Chamber* chamber = throttlingPerformance->getChamber();

	// Get nozzle exit object
	performance::equilibrium::NozzleSectionConditions* exitSection = throttlingPerformance->getExitSection();
	performance::equilibrium::NozzleSectionConditions* flowSeparationSection = NULL;


	StandardAtmosphere sa;
	sa.setAltitude(H);

	// Current ambient pressure at defined altitude, Pa
	double pa = sa.getPressure();

	if (checkFlowSeparation) {

		// Critical ambient pressure
		double pa_crit = throttlingPerformance->getPaCrit();

		if (pa>pa_crit) {
			// If pa>pa_crit, there is a flow separation in the nozzle
			// Find the location of the flow separation

			thermo::input::NozzleFlowOptions* pFlowOptions = new thermo::input::NozzleFlowOptions(data->getNozzleFlowOptions());

			bool checkForFreezing = pFlowOptions->isFreezingConditionsSet() && pFlowOptions->getFreezingConditions().isCalculate();

			thermo::input::NozzleSectionConditions& nozzleSectionConditions = pFlowOptions->getNozzleExitConditions();
			double condition;
			performance::frozen::NozzleSectionConditions::CONDITION_TYPE conditionType;
			if (nozzleSectionConditions.isAreaRatio()) {
				condition = nozzleSectionConditions.getAreaRatio();
				conditionType = performance::frozen::NozzleSectionConditions::FR;
			} else if (nozzleSectionConditions.isPressureRatio()) {
				condition = nozzleSectionConditions.getPressureRatio();
				conditionType = performance::frozen::NozzleSectionConditions::pp;
			} else if (nozzleSectionConditions.isPressure()) {
				condition = nozzleSectionConditions.getPressure();
				conditionType = performance::frozen::NozzleSectionConditions::P;
			}

			flowSeparationSection = throttlingPerformance->solveNozzleSection(condition, conditionType, checkForFreezing, true, pa, false);
		}

	}

	// Performance correction factor
	double phi = 1.0;
	if (applyCorrectionFactor) {
		// Get performance correction factor
		performance::efficiency::CorrectionFactors* correctionFactors = new performance::efficiency::CorrectionFactors(throttlingPerformance);
		phi = correctionFactors->getOverallEfficiency();
		delete correctionFactors;
	}

	// Specific impulse at defined altitude, m/s
	double IsH = 0;

	if (flowSeparationSection) {
		// Calculate Isp for nozzle with flow separation.
		double p2 = flowSeparationSection->getP();
		double IsH2 = 0.3*(pa - p2)*(flowSeparationSection->getF() - exitSection->getF());
		IsH = flowSeparationSection->getIs_v()*phi - flowSeparationSection->getF()*pa + IsH2;
	} else {
		// Calculate Isp for nozzle without flow separation.
		IsH = exitSection->getIs_v()*phi - exitSection->getF()*pa;
	}

	printf("Is=%8.3f m/s, %8.3f s at H=%6.2f km (pa=%8.4f atm), throttle value %6.2f\n", IsH, IsH/CONST_G, H/1000.0, pa/CONST_ATM, throttleValue);

	delete flowSeparationSection;
	delete throttlingPerformance;
	delete performance;
	delete data;

	util::Log::finalize();

	return 0;
}


