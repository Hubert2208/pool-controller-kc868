#ifndef DALLAS_TEMPERATURE_SENSOR_H
#define DALLAS_TEMPERATURE_SENSOR_H

#include "SensorBase.h"
#include <OneWire.h>
#include <DallasTemperature.h>

class DallasTemperatureSensor : public SensorBase {
public:
    // oneWirePin: GPIO pin for the OneWire bus
    // sensorIndex: which sensor on the bus (0 = first)
    // sensorLabel: "Water Temperature" or "Air Temperature"
    DallasTemperatureSensor(uint8_t oneWirePin, uint8_t sensorIndex = 0, const char* sensorLabel = "Water Temperature");

    bool begin() override;
    bool read() override;
    float getValue() const override { return _value; }
    const char* getName() const override { return _name; }
    const char* getUnit() const override { return "°C"; }
    bool isConnected() const override { return _connected; }

    // Get address of the sensor on the bus
    const uint8_t* getAddress() const { return _address; }

private:
    OneWire _oneWire;
    DallasTemperature _sensors;
    uint8_t _oneWirePin;
    uint8_t _sensorIndex;
    char _name[32];
    DeviceAddress _address;
    bool _addressResolved;
    bool _sensorsInitialized;

    void resolveAddress();
};

#endif // DALLAS_TEMPERATURE_SENSOR_H