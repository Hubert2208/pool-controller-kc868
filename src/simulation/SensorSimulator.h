#ifndef SENSOR_SIMULATOR_H
#define SENSOR_SIMULATOR_H

#include <Arduino.h>

class SensorSimulator {
public:
    SensorSimulator();
    SensorSimulator(float minVal, float maxVal, float driftPerHour);
    ~SensorSimulator() = default;

    // Reset the simulator
    void reset(unsigned long baseTime = 0);

    // Update the simulated value — call periodically
    void update();

    // Get the current simulated value
    float getValue() const { return _currentValue; }

    // Force a specific value
    void setValue(float val) { _currentValue = val; }

    // Pump-aware simulation: pump ON → directed effect; pump OFF → natural drift
    void setPumpActive(bool active) { _pumpActive = active; }
    bool isPumpActive() const { return _pumpActive; }

    // Set drift rates:
    //   naturalDriftPerHour: drift when pump is OFF (positive = rising, negative = falling)
    //   pumpDriftPerHour:   effect when pump is ON (positive = rising, negative = falling)
    void setPumpDriftRates(float naturalDriftPerHour, float pumpDriftPerHour) {
        _naturalDriftPerHour = naturalDriftPerHour;
        _pumpDriftPerHour = pumpDriftPerHour;
    }

private:
    float _minVal;
    float _maxVal;
    float _currentValue;
    unsigned long _lastUpdate;

    // Pump-aware simulation state
    bool _pumpActive = false;
    float _naturalDriftPerHour = 0.0f;   // drift when pump OFF
    float _pumpDriftPerHour = 0.0f;      // extra effect when pump ON
};

#endif // SENSOR_SIMULATOR_H
