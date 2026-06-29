#ifndef DHT_SENSOR_H
#define DHT_SENSOR_H

#include "SensorBase.h"
#include <DHT.h>

class DHTSensor : public SensorBase {
public:
    // pin: GPIO pin
    // type: DHT22 (preferred) or DHT11
    DHTSensor(uint8_t pin, uint8_t type = DHT22);

    bool begin() override;
    bool read() override;
    float getValue() const override { return _value; }
    const char* getName() const override { return "Air Temperature"; }
    const char* getUnit() const override { return "°C"; }
    bool isConnected() const override { return _connected; }

    // Additional: get humidity reading
    float getHumidity() const { return _humidity; }

    // Override getStateJSON to include humidity
    String getStateJSON() const override;

private:
    DHT _dht;
    uint8_t _pin;
    uint8_t _type;
    float _humidity;
};

#endif // DHT_SENSOR_H