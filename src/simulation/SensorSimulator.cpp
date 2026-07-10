#include "SensorSimulator.h"

SensorSimulator::SensorSimulator()
    : _minVal(0.0f)
    , _maxVal(100.0f)
    , _driftPerHour(1.0f)
    , _currentValue(50.0f)
    , _target(50.0f)
    , _lastUpdate(0)
    , _seed(0)
{
}

SensorSimulator::SensorSimulator(float minVal, float maxVal, float driftPerHour)
    : _minVal(minVal)
    , _maxVal(maxVal)
    , _driftPerHour(driftPerHour)
    , _currentValue((minVal + maxVal) / 2.0f)
    , _target((minVal + maxVal) / 2.0f)
    , _lastUpdate(0)
    , _seed(0)
{
}

void SensorSimulator::begin(float minVal, float maxVal, float driftPerHour) {
    _minVal = minVal;
    _maxVal = maxVal;
    _driftPerHour = driftPerHour;
    _currentValue = (minVal + maxVal) / 2.0f;
    _target = (minVal + maxVal) / 2.0f;
    _lastUpdate = millis();
    _seed = esp_random();
}

void SensorSimulator::reset(unsigned long baseTime) {
    _currentValue = (_minVal + _maxVal) / 2.0f;
    _target = (_minVal + _maxVal) / 2.0f;
    _lastUpdate = baseTime > 0 ? baseTime : millis();
    _seed = esp_random();
}

float SensorSimulator::randomWalkStep() {
    // Unused — kept for API compatibility
    return 0.0f;
}

void SensorSimulator::update() {
    unsigned long now = millis();
    if (_lastUpdate == 0) {
        _lastUpdate = now;
        return;
    }

    unsigned long deltaMs = now - _lastUpdate;
    if (deltaMs < 100) return;  // don\'t update too often
    _lastUpdate = now;

    // Calculate drift for this time step
    float deltaHours = deltaMs / 3600000.0f;

    // Apply pump-aware directional drift (pump chemistry effect)
    if (_pumpActive) {
        _currentValue += _pumpDriftPerHour * deltaHours;
    } else {
        _currentValue += _naturalDriftPerHour * deltaHours;
    }

    // Clamp to range
    _currentValue = max(_minVal, min(_maxVal, _currentValue));
}
