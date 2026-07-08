#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <vector>

#include "SensorBase.h"
#include "../simulation/SensorSimulator.h"
#include "../config/ConfigManager.h"

class SensorManager {
public:
    SensorManager(ConfigManager& configManager);
    ~SensorManager();

    // Initialize all sensors based on config
    bool begin();

    // Update all sensors that need reading (round-robin)
    void update();

    // Get all sensor states as a JSON string
    String getAllStateJSON();

    // Access individual sensors
    float getPH() const;
    float getORP() const;
    float getWaterTemperature() const;
    float getAirTemperature() const;
    float getFilterPressure() const;

    bool isPHConnected() const;
    bool isORPConnected() const;
    bool isWaterTempConnected() const;

    // Enable/disable sensor simulation modes
    void setSimulationMode(bool simulatePH, bool simulateORP);

    // Get the simulation instance for reading (used by PoolChemistryController)
    SensorSimulator* getPHSimulator() { return _phSim; }
    SensorSimulator* getORPSimulator() { return _orpSim; }

    // Set pump influence on simulation (pH: acid dosing ↓, ORP: chlorine dosing ↑)
    void setPHPumpActive(bool active);
    void setChlorinePumpActive(bool active);

private:
    ConfigManager& _config;

    // Real sensors
    std::vector<SensorBase*> _sensors;
    int _sensorCount;

    // Individual sensor references
    SensorBase* _phSensor;
    SensorBase* _orpSensor;
    SensorBase* _waterTempSensor;
    SensorBase* _airTempSensor;
    SensorBase* _pressureSensor;

    // Simulation fallbacks
    SensorSimulator* _phSim;
    SensorSimulator* _orpSim;
    SensorSimulator* _waterTempSim;
    SensorSimulator* _airTempSim;
    SensorSimulator* _pressureSim;

    // State tracking
    unsigned long _lastUpdate;
    int _currentSensorIndex;

    // Internal helpers
    SensorBase* createPHSensor();
    SensorBase* createORPSensor();
    SensorBase* createWaterTempSensor();
    SensorBase* createAirTempSensor();
    SensorBase* createPressureSensor();
};

#endif // SENSOR_MANAGER_H
