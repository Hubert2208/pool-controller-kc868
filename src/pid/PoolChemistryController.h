#ifndef POOL_CHEMISTRY_CONTROLLER_H
#define POOL_CHEMISTRY_CONTROLLER_H

#include <Arduino.h>
#include "../config/ConfigManager.h"
#include "../sensors/SensorManager.h"
#include "../actuators/PumpController.h"
#include "PIDController.h"

class PoolChemistryController {
public:
    PoolChemistryController(ConfigManager& config, SensorManager& sensors,
                            PumpController& phPump, PumpController& chlorinePump);

    // Initialize PID controllers with config values
    void begin();

    // Main control loop — call every loop() iteration
    void update();

    // Manual override controls
    void setPHSetpoint(float pH);
    void setORPSetpoint(float orpMV);
    void setPHEnabled(bool enabled);
    void setChlorineEnabled(bool enabled);

    bool isPHEnabled() const { return _phEnabled; }
    bool isChlorineEnabled() const { return _chlorineEnabled; }

    // Diagnostic access
    PIDController& getPHPID() { return _phPID; }
    PIDController& getChlorinePID() { return _chlorinePID; }

    float getLastPHOutput() const { return _lastPHOutput; }
    float getLastChlorineOutput() const { return _lastChlorineOutput; }

    // Get state as JSON for MQTT publishing
    String getStateJSON();

private:
    ConfigManager& _config;
    SensorManager& _sensors;
    PumpController& _phPump;
    PumpController& _chlorinePump;

    PIDController _phPID;
    PIDController _chlorinePID;

    bool _phEnabled;
    bool _chlorineEnabled;

    float _lastPHOutput;
    float _lastChlorineOutput;

    unsigned long _lastUpdate;
    unsigned long _lastPHCycleStart;
    unsigned long _lastChlorineCycleStart;

    void updatePHPID(float dtSec);
    void updateChlorinePID(float dtSec);
};

#endif // POOL_CHEMISTRY_CONTROLLER_H
