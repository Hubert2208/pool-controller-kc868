#ifndef SENSOR_BASE_H
#define SENSOR_BASE_H

#include <Arduino.h>

class SensorBase {
public:
    virtual ~SensorBase() {}

    // Initialize the sensor hardware
    virtual bool begin() = 0;

    // Read the current value from the sensor
    virtual bool read() = 0;

    // Get the most recently read value
    virtual float getValue() const = 0;

    // Human-readable sensor name
    virtual const char* getName() const = 0;

    // Measurement unit string (e.g. "pH", "mV", "°C", "bar")
    virtual const char* getUnit() const = 0;

    // Whether the sensor is currently communicating
    virtual bool isConnected() const = 0;

    // Get sensor state as a JSON string for MQTT publishing
    virtual String getStateJSON() const {
        StaticJsonDocument<192> doc;
        doc["name"] = getName();
        doc["value"] = getValue();
        doc["unit"] = getUnit();
        doc["connected"] = isConnected();
        doc["enabled"] = _enabled;
        String out;
        serializeJson(doc, out);
        return out;
    }

    bool isEnabled() const { return _enabled; }
    void setEnabled(bool e) { _enabled = e; }

    unsigned long lastReadTime() const { return _lastRead; }

protected:
    bool _enabled = true;
    bool _connected = false;
    float _value = 0.0f;
    unsigned long _lastRead = 0;
};

#endif // SENSOR_BASE_H