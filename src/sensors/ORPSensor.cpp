#include "ORPSensor.h"

static const float ADS1115_MAX_VOLTAGE = 4.096f;
static const float ADS1115_RESOLUTION = 32768.0f;

ORPSensor::ORPSensor(uint8_t adsAddress, uint8_t channel)
    : _adsAddress(adsAddress)
    , _channel(channel)
    , _calOffset(0.0f)
{
}

bool ORPSensor::begin() {
    if (!_ads.begin(_adsAddress)) {
        log_w("ORP ADS1115 not found at 0x%02x", _adsAddress);
        _connected = false;
        return false;
    }
    _ads.setGain(GAIN_ONE);
    _ads.setDataRate(RATE_ADS1115_860SPS);
    _connected = true;
    log_i("ORP sensor initialized on ADS1115[%d]", _channel);
    return true;
}

float ORPSensor::readVoltage() {
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

bool ORPSensor::read() {
    if (!_connected && !_enabled) {
        return false;
    }

    if (!_connected) {
        _connected = _ads.begin(_adsAddress);
        if (!_connected) return false;
    }

    float voltage = readVoltage();
    // ORP probes typically output 0-2.5V corresponding to 0-1000mV ORP
    _value = (voltage / ADS1115_MAX_VOLTAGE) * 1000.0f + _calOffset;

    // Clamp to reasonable ORP range
    _value = max(-200.0f, min(1000.0f, _value));

    _lastRead = millis();
    return true;
}

void ORPSensor::setCalibration(float knownORP_mV) {
    // Read actual probe voltage and compute offset against known ORP value
    float currentVoltage = readVoltage();
    float measured_mV = (currentVoltage / ADS1115_MAX_VOLTAGE) * 1000.0f;
    _calOffset = knownORP_mV - measured_mV;
    log_i("ORP cal: known=%.1f mV, measured=%.1f mV, offset=%.1f mV",
          knownORP_mV, measured_mV, _calOffset);
}