#ifndef ORP_SENSOR_H
#define ORP_SENSOR_H

#include "SensorBase.h"
#include "../config/ConfigManager.h"
#include <Adafruit_ADS1X15.h>

class ORPSensor : public SensorBase {
public:
    ORPSensor(uint8_t adsAddress = 0x48, uint8_t channel = 1);

    bool begin() override;
    bool read() override;
    float getValue() const override { return _value; }
    const char* getName() const override { return "ORP"; }
    const char* getUnit() const override { return "mV"; }
    bool isConnected() const override { return _connected; }

    // Calibration
    void setCalibration(float knownORP_mV);
    void loadCalibration(const CalibrationData& cal);
    void saveCalibration(CalibrationData& cal) const;
    float getCalibrationOffset() const { return _calOffset; }

    // Raw voltage for live monitoring during calibration
    float readRawVoltage();
    float readRawMV();         // Raw mV (before offset correction)
    float getLastVoltage() const { return _lastRawVoltage; }

private:
    Adafruit_ADS1115 _ads;
    uint8_t _adsAddress;
    uint8_t _channel;
    float _calOffset;          // mV offset for calibration
    float _calReferenceMV;     // Known reference value used
    float _lastRawVoltage;
};

#endif // ORP_SENSOR_H
