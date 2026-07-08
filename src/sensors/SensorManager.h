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

    bool begin();
    void update();
    String getAllStateJSON();

    float getPH() const;
    float getORP() const;
    float getWaterTemperature() const;
    float getAirTemperature() const;
    float getFilterPressure() const;

    bool isPHConnected() const;
    bool isORPConnected() const;
    bool isWaterTempConnected() const;

    void setSimulationMode(bool simulatePH, bool simulateORP);

    SensorSimulator* getPHSimulator() { return _phSim; }
    SensorSimulator* getORPSimulator() { return _orpSim; }

    // Set pump influence on simulation (computed from PID output % × coefficient)
    void setPHPumpInfluence(float influencePerHour);
    void setChlorinePumpInfluence(float influencePerHour);

private:
    ConfigManager& _config;
    std::vector<SensorBase*> _sensors;
    int _sensorCount;
    SensorBase* _phSensor;
    SensorBase* _orpSensor;
    SensorBase* _waterTempSensor;
    SensorBase* _airTempSensor;
    SensorBase* _pressureSensor;
    SensorSimulator* _phSim;
    SensorSimulator* _orpSim;
    SensorSimulator* _waterTempSim;
    SensorSimulator* _airTempSim;
    SensorSimulator* _pressureSim;
    unsigned long _lastUpdate;
    int _currentSensorIndex;

    SensorBase* createPHSensor();
    SensorBase* createORPSensor();
    SensorBase* createWaterTempSensor();
    SensorBase* createAirTempSensor();
    SensorBase* createPressureSensor();
};

#endif // SENSOR_MANAGER_H
