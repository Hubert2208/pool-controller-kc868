#ifndef PRESSURE_SENSOR_H
#define PRESSURE_SENSOR_H

#include "SensorBase.h"
#include <Adafruit_ADS1X15.h>

class PressureSensor : public SensorBase {
public:
    // adsAddress: I2C address of ADS1115 (default 0x48)
    // channel: ADS1115 analog input channel (0-3)
    PressureSensor(uint8_t adsAddress = 0x48, uint8_t channel = 2);

    bool begin() override;
    bool read() override;
    float getValue() const override { return _value; }
    const char* getName() const override { return "Filter Pressure"; }
    const char* getUnit() const override { return "bar"; }
    bool isConnected() const override { return _connected; }

    // Backwash detection
    bool needsBackwash() const;
    void setBackwashThreshold(float bar);
    float getBackwashThreshold() const { return _backwashThreshold; }

    // Calibration: voltage to pressure mapping
    void setPressureRange(float voltageAt0Bar, float voltageAtMaxBar, float maxPressureBar);

private:
    Adafruit_ADS1115 _ads;
    uint8_t _adsAddress;
    uint8_t _channel;
    float _backwashThreshold;
    float _voltageAt0Bar;
    float _voltageAtMaxBar;
    float _maxPressureBar;

    float readVoltage();
};

#endif // PRESSURE_SENSOR_H