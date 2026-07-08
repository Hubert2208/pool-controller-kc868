#ifndef SENSOR_SIMULATOR_H
#define SENSOR_SIMULATOR_H

#include <Arduino.h>

class SensorSimulator {
public:
    SensorSimulator();
    SensorSimulator(float minVal, float maxVal, float driftPerHour);
    ~SensorSimulator() = default;

    // Initialize or reset the simulator
    void begin(float minVal, float maxVal, float driftPerHour);
    void reset(unsigned long baseTime = 0);

    // Update the simulated value — call periodically
    void update();

    // Set external influence (pump effect) in units per hour
    // Positive = value rises, negative = value falls
    void setExternalDrift(float driftPerHour) { _externalDrift = driftPerHour; }

    // Get the current simulated value
    float getValue() const { return _currentValue; }

    // Get the target/center value
    float getTarget() const { return _target; }

    // Force a specific value
    void setValue(float val) { _currentValue = val; }

private:
    float _minVal;
    float _maxVal;
    float _driftPerHour;
    float _currentValue;
    float _target;
    float _externalDrift;
    unsigned long _lastUpdate;
    unsigned long _seed;

    // Random walk step
    float randomWalkStep();
};

#endif // SENSOR_SIMULATOR_H