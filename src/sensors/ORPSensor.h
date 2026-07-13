#ifndef ORP_SENSOR_H
#define ORP_SENSOR_H

#include "SensorBase.h"
#include <Adafruit_ADS1X15.h>

class ORPSensor : public SensorBase {
public:
    // adsAddress: I2C address of ADS1115 (default 0x48)
    // channel: ADS1115 analog input channel (0-3)
    ORPSensor(uint8_t adsAddress = 0x48, uint8_t channel = 1);

    bool begin() override;
    bool read() override;
    float getValue() const override { return _value; }
    const char* getName() const override { return "ORP"; }
    const char* getUnit() const override { return "mV"; }
    bool isConnected() const override { return _connected; }

    void setCalibration(float knownORP_mV);
    float getCalibrationOffset() const { return _calOffset; }

private:
    Adafruit_ADS1115 _ads;
    uint8_t _adsAddress;
    uint8_t _channel;
    float _calOffset;      // mV offset for calibration
    float readVoltage();
};

#endif // ORP_SENSOR_H