#include "DHTSensor.h"

DHTSensor::DHTSensor(uint8_t pin, uint8_t type)
    : _dht(pin, type)
    , _pin(pin)
    , _type(type)
    , _humidity(0.0f)
{
}

bool DHTSensor::begin() {
    _dht.begin();
    // DHT22 needs ~1s to stabilize after power-on
    delay(500);
    _connected = true;
    log_i("DHT22 initialized on pin %d", _pin);
    return true;
}

bool DHTSensor::read() {
    if (!_connected && !_enabled) {
        return false;
    }

    if (!_connected) {
        _dht.begin();
        delay(500);
        _connected = true;
    }

    float temp = _dht.readTemperature();
    float humidity = _dht.readHumidity();

    if (isnan(temp) || isnan(humidity)) {
        log_w("DHT22 read failed on pin %d", _pin);
        _connected = false;
        return false;
    }

    _value = temp;
    _humidity = humidity;
    _lastRead = millis();
    _connected = true;
    return true;
}

String DHTSensor::getStateJSON() const {
    StaticJsonDocument<256> doc;
    doc["name"] = getName();
    doc["value"] = getValue();
    doc["unit"] = getUnit();
    doc["connected"] = isConnected();
    doc["enabled"] = _enabled;
    doc["humidity"] = _humidity;
    String out;
    serializeJson(doc, out);
    return out;
}