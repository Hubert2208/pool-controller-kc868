#ifndef PH_SENSOR_H
#define PH_SENSOR_H

#include "SensorBase.h"
#include "../config/ConfigManager.h"
#include <Adafruit_ADS1X15.h>

class PHSensor : public SensorBase {
public:
    PHSensor(uint8_t adsAddress = 0x48, uint8_t channel = 0);

    bool begin() override;
    bool read() override;
    float getValue() const override { return _value; }
    const char* getName() const override { return "pH"; }
    const char* getUnit() const override { return "pH"; }
    bool isConnected() const override { return _connected; }

    // Calibration
    void setCalibration(float voltageAtPH7, float voltageAtPH4);
    void loadCalibration(const CalibrationData& cal);
    void saveCalibration(CalibrationData& cal) const;
    float getCalibrationSlope() const { return _calSlope; }
    float getCalibrationIntercept() const { return _calIntercept; }

    // Raw voltage for live monitoring during calibration
    float readRawVoltage();
    float getLastVoltage() const { return _lastRawVoltage; }

private:
    Adafruit_ADS1115 _ads;
    uint8_t _adsAddress;
    uint8_t _channel;
    float _calSlope;          // pH per volt (negative: pH drops as voltage rises)
    float _calIntercept;       // pH at 0V
    float _voltagePH7;         // Measured voltage at pH 7.0
    float _voltagePH4;         // Measured voltage at pH 4.0
    float _calTemperature;     // °C reference for Nernst correction
    float _lastRawVoltage;     // Last raw voltage reading (for calibration UI)

    // Nernst temperature compensation (≈59.16 mV/pH at 25°C)
    float getTempCorrectedSlope(float waterTempC) const;
};

#endif // PH_SENSOR_H
