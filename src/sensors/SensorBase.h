#ifndef SENSOR_BASE_H
#define SENSOR_BASE_H

#include <Arduino.h>
#include <ArduinoJson.h>

class SensorBase {
public:
    virtual ~SensorBase() {}

    virtual bool begin() = 0;
    virtual bool read() = 0;
    virtual float getValue() const = 0;
    virtual const char* getName() const = 0;
    virtual const char* getUnit() const = 0;
    virtual bool isConnected() const = 0;

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

    void setUpdateIntervalMs(int ms) { _updateIntervalMs = ms; }
    int updateIntervalMs() const { return _updateIntervalMs; }

    unsigned long lastReadTime() const { return _lastRead; }

protected:
    bool _enabled = true;
    bool _connected = false;
    float _value = 0.0f;
    unsigned long _lastRead = 0;
    int _updateIntervalMs = 2000;
};

#endif // SENSOR_BASE_H
