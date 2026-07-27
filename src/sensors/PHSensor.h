#ifndef PH_SENSOR_H
#define PH_SENSOR_H

#include "SensorBase.h"
#include <Adafruit_ADS1X15.h>

class PHSensor : public SensorBase {
public:
    // adsAddress: I2C address of ADS1115 (default 0x48)
    // channel: ADS1115 analog input channel (0-3)
    PHSensor(uint8_t adsAddress = 0x48, uint8_t channel = 0);

    bool begin() override;
    bool read() override;
    float getValue() const override { return _value; }
    const char* getName() const override { return "pH"; }
    const char* getUnit() const override { return "pH"; }
    bool isConnected() const override { return _connected; }

    // Calibration: voltage -> pH conversion
    void setCalibration(float voltageAtPH7, float voltageAtPH4);

private:
    Adafruit_ADS1115 _ads;
    uint8_t _adsAddress;
    uint8_t _channel;
    float _calSlope;        // pH per volt (negative: pH drops as voltage rises)
    float _calIntercept;    // pH at 0V
    float readVoltage();
};

#endif // PH_SENSOR_H