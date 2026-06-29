#ifndef FILTER_PUMP_LOGIC_H
#define FILTER_PUMP_LOGIC_H

#include <Arduino.h>
#include "../config/ConfigManager.h"
#include "../actuators/PumpController.h"

class FilterPumpLogic {
public:
    FilterPumpLogic(ConfigManager& config, PumpController& pump);

    void begin();

    // Main update — call every loop
    void update(float waterTemperature);

    // Calculate required daily runtime based on water temperature
    float calculateDailyRuntime(float waterTemp) const;

    // Check if current time is within the operating window
    bool isInOperatingWindow() const;

    // Get number of filter cycles per day
    int getRequiredCyclesPerDay() const;

    // Force on/off
    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }

    String getStateJSON() const;

private:
    ConfigManager& _config;
    PumpController& _pump;

    bool _enabled;
    float _dailyRequiredMinutes;
    unsigned long _lastCycleOffTime;
    unsigned long _lastUpdate;

    void manageCycles();
};

#endif // FILTER_PUMP_LOGIC_H