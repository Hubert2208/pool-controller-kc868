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
    // Use deterministic pseudo-random based on seed
    _seed = _seed * 1103515245 + 12345;
    float r = (float)(_seed & 0x7FFFFFFF) / (float)0x7FFFFFFF;  // 0..1
    return (r - 0.5f) * 2.0f;  // -1..1
}

void SensorSimulator::update() {
    unsigned long now = millis();
    if (_lastUpdate == 0) {
        _lastUpdate = now;
        return;
    }

    unsigned long deltaMs = now - _lastUpdate;
    if (deltaMs < 100) return;  // don't update too often
    _lastUpdate = now;

    // Calculate drift for this time step
    float deltaHours = deltaMs / 3600000.0f;
    float maxStep = _driftPerHour * deltaHours;

    // Oscillate target around center with slow sine wave
    float center = (_minVal + _maxVal) / 2.0f;
    float range = (_maxVal - _minVal) / 2.0f;
    float phase = (now % 7200000) / 7200000.0f * 2.0f * M_PI;  // 2 hour cycle
    _target = center + sinf(phase) * range * 0.3f;  // target moves ±30% of range

    // Random walk towards target
    float diff = _target - _currentValue;
    float pullStrength = min(0.1f, deltaHours * 2.0f);  // gradual pull
    float pull = diff * pullStrength;
    float noise = randomWalkStep() * maxStep * 0.3f;

    _currentValue += pull + noise;

    // Clamp to range
    _currentValue = max(_minVal, min(_maxVal, _currentValue));
}