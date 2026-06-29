#include "PHSensor.h"

static const float ADS1115_MAX_VOLTAGE = 4.096f;   // 4.096V with gain=1
static const float ADS1115_RESOLUTION = 32768.0f;   // 16-bit signed

PHSensor::PHSensor(uint8_t adsAddress, uint8_t channel)
    : _adsAddress(adsAddress)
    , _channel(channel)
    , _calSlope(-3.5f)          // typical pH electrode: ~-3.5 pH/V
    , _calIntercept(7.0f)       // 0V ≈ pH 7.0
    , _voltagePH7(0.0f)
    , _voltagePH4(0.0f)
{
}

bool PHSensor::begin() {
    if (!_ads.begin(_adsAddress)) {
        log_w("pH ADS1115 not found at 0x%02x", _adsAddress);
        _connected = false;
        return false;
    }
    _ads.setGain(GAIN_ONE);       // ±4.096V range
    _ads.setDataRate(RATE_ADS1115_860SPS);
    _connected = true;
    log_i("pH sensor initialized on ADS1115[%d]", _channel);
    return true;
}

float PHSensor::readVoltage() {
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

bool PHSensor::read() {
    if (!_connected && !_enabled) {
        return false;
    }

    if (!_connected) {
        // Retry connection
        _connected = _ads.begin(_adsAddress);
        if (!_connected) return false;
    }

    float voltage = readVoltage();
    _value = _calSlope * voltage + _calIntercept;

    // Clamp to valid pH range
    _value = max(0.0f, min(14.0f, _value));

    _lastRead = millis();
    return true;
}

void PHSensor::setCalibration(float voltageAtPH7, float voltageAtPH4) {
    _voltagePH7 = voltageAtPH7;
    _voltagePH4 = voltageAtPH4;

    // Calculate slope: (7-4) / (V7 - V4)
    float voltageDiff = voltageAtPH7 - voltageAtPH4;
    if (fabs(voltageDiff) > 0.001f) {
        _calSlope = (7.0f - 4.0f) / voltageDiff;
    }
    _calIntercept = 7.0f - _calSlope * voltageAtPH7;

    log_i("pH cal updated: slope=%.3f intercept=%.3f", _calSlope, _calIntercept);
}