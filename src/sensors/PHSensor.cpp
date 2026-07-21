#include "PHSensor.h"

static const float ADS1115_MAX_VOLTAGE = 4.096f;   // 4.096V with gain=1
static const float ADS1115_RESOLUTION = 32768.0f;   // 16-bit signed
static const float NERNST_SLOPE_25C = 59.16f;       // mV/pH at 25°C
static const float NERNST_BASE_KELVIN = 273.15f;    // 0°C in Kelvin
static const float REFERENCE_TEMP_K = 298.15f;       // 25°C in Kelvin

PHSensor::PHSensor(uint8_t adsAddress, uint8_t channel)
    : _adsAddress(adsAddress)
    , _channel(channel)
    , _calSlope(-3.5f)          // typical pH electrode: ~-3.5 pH/V = -59.16 mV/pH ÷ ~16.9 V/V
    , _calIntercept(7.0f)       // 0V ≈ pH 7.0
    , _voltagePH7(0.0f)
    , _voltagePH4(0.0f)
    , _calTemperature(25.0f)
    , _lastRawVoltage(0.0f)
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

    // Load persisted calibration
    CalibrationData cal;
    if (cal.load()) {
        loadCalibration(cal);
    }

    _connected = true;
    log_i("pH sensor initialized on ADS1115[%d] (slope=%.3f, intercept=%.3f)", _channel, _calSlope, _calIntercept);
    return true;
}

float PHSensor::getTempCorrectedSlope(float waterTempC) const {
    // Nernst slope scales linearly with absolute temperature:
    // Slope(T) = Slope(25°C) × (T + 273.15) / (25 + 273.15)
    float tempK = waterTempC + NERNST_BASE_KELVIN;
    if (tempK < 278.15f) tempK = 278.15f;  // clamp 5°C–45°C
    if (tempK > 318.15f) tempK = 318.15f;

    // Convert the stored slope (pH/V) to corrected value
    // _calSlope is stored as pH/volt, which is mV_slope / (mV_per_volt_ratio)
    // The ratio mV_slope / NERNST_SLOPE_25C gives us the electrode efficiency
    // Correct: new_slope = cal_slope * (tempK / REFERENCE_TEMP_K)
    return _calSlope * (tempK / REFERENCE_TEMP_K);
}

float PHSensor::readRawVoltage() {
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
    _lastRawVoltage = voltage;
    return voltage;
}

bool PHSensor::read() {
    if (!_connected && !_enabled) {
        return false;
    }

    if (!_connected) {
        _connected = _ads.begin(_adsAddress);
        if (!_connected) return false;
    }

    float voltage = readRawVoltage();

    // Apply temperature-corrected slope if calTemperature was set
    // (uses default uncorrected slope when no calibration temp available)
    float slope = _calSlope;
    if (_calTemperature > 0.0f) {
        slope = getTempCorrectedSlope(_calTemperature);
    }

    _value = slope * voltage + _calIntercept;

    // Clamp to valid pH range
    _value = max(0.0f, min(14.0f, _value));

    _lastRead = millis();
    return true;
}

void PHSensor::setCalibration(float voltageAtPH7, float voltageAtPH4) {
    _voltagePH7 = voltageAtPH7;
    _voltagePH4 = voltageAtPH4;

    // Calculate slope: (7-4.01) / (V7 - V4)
    float voltageDiff = voltageAtPH7 - voltageAtPH4;
    if (fabs(voltageDiff) > 0.001f) {
        _calSlope = (7.0f - 4.01f) / voltageDiff;
    }
    _calIntercept = 7.0f - _calSlope * voltageAtPH7;

    log_i("pH cal updated: slope=%.3f pH/V intercept=%.3f (V7=%.4f V4=%.4f)",
          _calSlope, _calIntercept, voltageAtPH7, voltageAtPH4);
}

void PHSensor::loadCalibration(const CalibrationData& cal) {
    _calSlope = cal.phSlope;
    _calIntercept = cal.phIntercept;
    _voltagePH7 = cal.phVoltagePH7;
    _voltagePH4 = cal.phVoltagePH4;
    _calTemperature = cal.calTemperature;
    log_i("pH cal loaded: slope=%.3f pH/V intercept=%.3f", _calSlope, _calIntercept);
}

void PHSensor::saveCalibration(CalibrationData& cal) const {
    cal.phSlope = _calSlope;
    cal.phIntercept = _calIntercept;
    cal.phVoltagePH7 = _voltagePH7;
    cal.phVoltagePH4 = _voltagePH4;
    time_t now = time(nullptr);
    cal.phCalibratedAt = (now > 100000) ? (unsigned long)now : millis() / 1000;
}
