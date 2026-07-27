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

    // Pump state for simulation (pump-aware drift)
    void setPHPumpActive(bool active);
    void setChlorinePumpActive(bool active);

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
