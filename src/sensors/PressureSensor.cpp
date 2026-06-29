#include "PressureSensor.h"

static const float ADS1115_MAX_VOLTAGE = 4.096f;
static const float ADS1115_RESOLUTION = 32768.0f;

PressureSensor::PressureSensor(uint8_t adsAddress, uint8_t channel)
    : _adsAddress(adsAddress)
    , _channel(channel)
    , _backwashThreshold(2.0f)       // bar — typical backwash threshold
    , _voltageAt0Bar(0.5f)
    , _voltageAtMaxBar(3.5f)
    , _maxPressureBar(3.0f)
{
}

bool PressureSensor::begin() {
    if (!_ads.begin(_adsAddress)) {
        log_w("Pressure ADS1115 not found at 0x%02x", _adsAddress);
        _connected = false;
        return false;
    }
    _ads.setGain(GAIN_ONE);
    _ads.setDataRate(RATE_ADS1115_860SPS);
    _connected = true;
    log_i("Pressure sensor initialized on ADS1115[%d]", _channel);
    return true;
}

float PressureSensor::readVoltage() {
    if (!_connected) return 0.0f;

    int16_t raw = 0;
    switch (_channel) {
        case 0: raw = _ads.readADC_SingleEnded(0); break;
        case 1: raw = _ads.readADC_SingleEnded(1); break;
        case 2: raw = _ads.readADC_SingleEnded(2); break;
        case 3: raw = _ads.readADC_SingleEnded(3); break;
        default: return 0.0f;
    }

    float voltage = (raw / ADS1115_RESOLUTION) * ADS1115_MAX_VOLTAGE;
    voltage = max(0.0f, min(ADS1115_MAX_VOLTAGE, voltage));
    return voltage;
}

bool PressureSensor::read() {
    if (!_connected && !_enabled) {
        return false;
    }

    if (!_connected) {
        _connected = _ads.begin(_adsAddress);
        if (!_connected) return false;
    }

    float voltage = readVoltage();

    // Convert voltage to pressure (linear mapping)
    float voltageRange = _voltageAtMaxBar - _voltageAt0Bar;
    if (fabs(voltageRange) < 0.001f) voltageRange = 1.0f;

    float pressure = ((voltage - _voltageAt0Bar) / voltageRange) * _maxPressureBar;
    pressure = max(0.0f, pressure);

    _value = pressure;
    _lastRead = millis();
    return true;
}

bool PressureSensor::needsBackwash() const {
    return _value >= _backwashThreshold;
}

void PressureSensor::setBackwashThreshold(float bar) {
    _backwashThreshold = bar;
    log_i("Backwash threshold set to %.2f bar", _backwashThreshold);
}

void PressureSensor::setPressureRange(float voltageAt0Bar, float voltageAtMaxBar, float maxPressureBar) {
    _voltageAt0Bar = voltageAt0Bar;
    _voltageAtMaxBar = voltageAtMaxBar;
    _maxPressureBar = maxPressureBar;
    log_i("Pressure range: %.2fV-%.2fV -> 0-%.1f bar",
          _voltageAt0Bar, _voltageAtMaxBar, _maxPressureBar);
}