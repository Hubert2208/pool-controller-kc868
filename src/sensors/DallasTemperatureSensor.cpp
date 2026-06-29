#include "DallasTemperatureSensor.h"

DallasTemperatureSensor::DallasTemperatureSensor(uint8_t oneWirePin, uint8_t sensorIndex, const char* sensorLabel)
    : _oneWire(oneWirePin)
    , _sensors(&_oneWire)
    , _oneWirePin(oneWirePin)
    , _sensorIndex(sensorIndex)
    , _addressResolved(false)
    , _sensorsInitialized(false)
{
    strncpy(_name, sensorLabel, sizeof(_name) - 1);
    _name[sizeof(_name) - 1] = '\0';
}

bool DallasTemperatureSensor::begin() {
    _sensors.begin();
    _sensorsInitialized = true;

    int deviceCount = _sensors.getDeviceCount();
    if (deviceCount == 0) {
        log_w("No DS18B20 sensors found on pin %d", _oneWirePin);
        _connected = false;
        return false;
    }

    if (_sensorIndex >= (uint8_t)deviceCount) {
        log_w("Sensor index %d exceeds count %d", _sensorIndex, deviceCount);
        _connected = false;
        return false;
    }

    resolveAddress();

    _sensors.setResolution(_address, 12);    // 12-bit resolution
    _sensors.setWaitForConversion(false);     // async conversion

    _connected = true;
    log_i("DS18B20 '%s' initialized on pin %d (idx %d)", _name, _oneWirePin, _sensorIndex);
    return true;
}

void DallasTemperatureSensor::resolveAddress() {
    if (_sensorsInitialized && !_addressResolved) {
        if (_sensors.getAddress(_address, _sensorIndex)) {
            _addressResolved = true;
        }
    }
}

bool DallasTemperatureSensor::read() {
    if (!_connected && !_enabled) {
        return false;
    }

    if (!_connected) {
        _sensors.begin();
        int deviceCount = _sensors.getDeviceCount();
        if (deviceCount > (int)_sensorIndex) {
            resolveAddress();
            if (_addressResolved) {
                _sensors.setResolution(_address, 12);
                _connected = true;
            }
        }
        if (!_connected) return false;
    }

    // Request conversion and wait minimum time
    _sensors.requestTemperatures();

    float tempC = _sensors.getTempC(_address);
    if (tempC == DEVICE_DISCONNECTED_C) {
        log_w("DS18B20 '%s' disconnected", _name);
        _connected = false;
        return false;
    }

    _value = tempC;
    _lastRead = millis();
    _connected = true;
    return true;
}