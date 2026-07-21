#include "SensorManager.h"
#include "PHSensor.h"
#include "ORPSensor.h"
#include "DallasTemperatureSensor.h"
#include "PressureSensor.h"

// KC868-A8 I2C pins (default ESP32 I2C)
#define I2C_SDA 4
#define I2C_SCL 5
#define DS18B20_WATER_PIN 14
#define DS18B20_AIR_PIN 13

SensorManager::SensorManager(ConfigManager& configManager)
    : _config(configManager)
    , _sensorCount(0)
    , _phSensor(nullptr)
    , _orpSensor(nullptr)
    , _waterTempSensor(nullptr)
    , _airTempSensor(nullptr)
    , _pressureSensor(nullptr)
    , _phSim(nullptr)
    , _orpSim(nullptr)
    , _waterTempSim(nullptr)
    , _airTempSim(nullptr)
    , _pressureSim(nullptr)
    , _lastUpdate(0)
    , _currentSensorIndex(0)
{
}

SensorManager::~SensorManager() {
    for (auto* s : _sensors) {
        delete s;
    }
    _sensors.clear();
    delete _phSim;
    delete _orpSim;
    delete _waterTempSim;
    delete _airTempSim;
    delete _pressureSim;
}

bool SensorManager::begin() {
    // Initialize I2C for ADS1115
    Wire.begin(I2C_SDA, I2C_SCL);

    AppConfig& cfg = _config.get();

    // Create real sensors based on config
    _phSensor = createPHSensor();
    if (_phSensor && _phSensor->begin()) {
        _sensors.push_back(_phSensor);
        _sensorCount++;
        _phSensor->setUpdateIntervalMs(cfg.phSensor.updateIntervalMs);
        log_i("pH sensor initialized");
    } else {
        delete _phSensor;
        _phSensor = nullptr;
        log_w("pH sensor failed, will use simulation");
    }

    _orpSensor = createORPSensor();
    if (_orpSensor && _orpSensor->begin()) {
        _sensors.push_back(_orpSensor);
        _sensorCount++;
        _orpSensor->setUpdateIntervalMs(cfg.orpSensor.updateIntervalMs);
        log_i("ORP sensor initialized");
    } else {
        delete _orpSensor;
        _orpSensor = nullptr;
        log_w("ORP sensor failed, will use simulation");
    }

    _waterTempSensor = createWaterTempSensor();
    if (_waterTempSensor && _waterTempSensor->begin()) {
        _sensors.push_back(_waterTempSensor);
        _sensorCount++;
        _waterTempSensor->setUpdateIntervalMs(cfg.tempWaterSensor.updateIntervalMs);
        log_i("Water temp sensor initialized");
    } else {
        delete _waterTempSensor;
        _waterTempSensor = nullptr;
        log_w("Water temp sensor failed, will use simulation");
    }

    _airTempSensor = createAirTempSensor();
    if (_airTempSensor && _airTempSensor->begin()) {
        _sensors.push_back(_airTempSensor);
        _sensorCount++;
        _airTempSensor->setUpdateIntervalMs(cfg.tempAirSensor.updateIntervalMs);
        log_i("Air temp sensor initialized");
    } else {
        delete _airTempSensor;
        _airTempSensor = nullptr;
        log_w("Air temp sensor failed, will use simulation");
    }

    _pressureSensor = createPressureSensor();
    if (_pressureSensor && _pressureSensor->begin()) {
        _sensors.push_back(_pressureSensor);
        _sensorCount++;
        _pressureSensor->setUpdateIntervalMs(cfg.pressureSensor.updateIntervalMs);
        log_i("Pressure sensor initialized");
    } else {
        delete _pressureSensor;
        _pressureSensor = nullptr;
        log_w("Pressure sensor failed, will use simulation");
    }

    // Initialize simulation fallbacks
    _phSim = new SensorSimulator(
        cfg.phSensor.simMin, cfg.phSensor.simMax, cfg.phSensor.simDriftPerHour
    );
    _orpSim = new SensorSimulator(
        cfg.orpSensor.simMin, cfg.orpSensor.simMax, cfg.orpSensor.simDriftPerHour
    );
    _waterTempSim = new SensorSimulator(
        cfg.tempWaterSensor.simMin, cfg.tempWaterSensor.simMax, cfg.tempWaterSensor.simDriftPerHour
    );
    _airTempSim = new SensorSimulator(
        cfg.tempAirSensor.simMin, cfg.tempAirSensor.simMax, cfg.tempAirSensor.simDriftPerHour
    );
    _pressureSim = new SensorSimulator(
        cfg.pressureSensor.simMin, cfg.pressureSensor.simMax, cfg.pressureSensor.simDriftPerHour
    );

    // Configure pump-aware simulation drift rates
    // pH: natural drift up (+0.15/h) -- pool pH rises without dosing
    //     pump ON drops pH (-0.8/h) -- pH-minus dosing effect
    if (_phSim) _phSim->setPumpDriftRates(0.15f, -0.8f);
    // ORP: natural decay (-8 mV/h) -- chlorine degrades without dosing
    //      pump ON raises ORP (+40 mV/h) -- chlorine dosing effect
    if (_orpSim) _orpSim->setPumpDriftRates(-8.0f, 40.0f);

    log_i("Sensor manager initialized with %d real sensors + simulation fallbacks", _sensorCount);
    return true;
}

void SensorManager::update() {
    AppConfig& cfg = _config.get();
    unsigned long now = millis();

    // Round-robin through real sensors
    if (_sensorCount > 0 && (now - _lastUpdate >= cfg.loopDelayMs)) {
        SensorBase* s = _sensors[_currentSensorIndex];
        if (s->isEnabled()) {
            if (now - s->lastReadTime() >= (unsigned long)s->updateIntervalMs()) {
                s->read();
            }
        }

        _currentSensorIndex = (_currentSensorIndex + 1) % _sensorCount;
        _lastUpdate = now;
    }

    // Update simulation fallbacks
    if (_phSim) _phSim->update();
    if (_orpSim) _orpSim->update();
    if (_waterTempSim) _waterTempSim->update();
    if (_airTempSim) _airTempSim->update();
    if (_pressureSim) _pressureSim->update();
}

float SensorManager::getPH() const {
    if (_phSensor && _phSensor->isConnected()) return _phSensor->getValue();
    if (_phSim) return _phSim->getValue();
    return 7.0f;
}

float SensorManager::getORP() const {
    if (_orpSensor && _orpSensor->isConnected()) return _orpSensor->getValue();
    if (_orpSim) return _orpSim->getValue();
    return 400.0f;
}

float SensorManager::getWaterTemperature() const {
    if (_waterTempSensor && _waterTempSensor->isConnected()) return _waterTempSensor->getValue();
    if (_waterTempSim) return _waterTempSim->getValue();
    return 20.0f;
}

float SensorManager::getAirTemperature() const {
    if (_airTempSensor && _airTempSensor->isConnected()) return _airTempSensor->getValue();
    if (_airTempSim) return _airTempSim->getValue();
    return 25.0f;
}

float SensorManager::getFilterPressure() const {
    if (_pressureSensor && _pressureSensor->isConnected()) return _pressureSensor->getValue();
    if (_pressureSim) return _pressureSim->getValue();
    return 0.0f;
}

bool SensorManager::isPHConnected() const {
    return _phSensor && _phSensor->isConnected();
}

bool SensorManager::isORPConnected() const {
    return _orpSensor && _orpSensor->isConnected();
}

bool SensorManager::isWaterTempConnected() const {
    return _waterTempSensor && _waterTempSensor->isConnected();
}

void SensorManager::setPHPumpActive(bool active) {
    if (_phSim) _phSim->setPumpActive(active);
}

void SensorManager::setChlorinePumpActive(bool active) {
    if (_orpSim) _orpSim->setPumpActive(active);
}

String SensorManager::getAllStateJSON() {
    StaticJsonDocument<1024> doc;

    JsonObject ph = doc.createNestedObject("ph");
    ph["value"] = getPH();
    ph["connected"] = isPHConnected();
    ph["unit"] = "pH";

    JsonObject orp = doc.createNestedObject("orp");
    orp["value"] = getORP();
    orp["connected"] = isORPConnected();
    orp["unit"] = "mV";

    JsonObject wt = doc.createNestedObject("water_temperature");
    wt["value"] = getWaterTemperature();
    wt["connected"] = isWaterTempConnected();
    wt["unit"] = "°C";

    JsonObject at = doc.createNestedObject("air_temperature");
    at["value"] = getAirTemperature();
    at["unit"] = "°C";

    JsonObject pres = doc.createNestedObject("filter_pressure");
    pres["value"] = getFilterPressure();
    pres["unit"] = "bar";
    pres["needs_backwash"] = (_pressureSensor && _pressureSensor->isConnected())
        ? dynamic_cast<PressureSensor*>(_pressureSensor)->needsBackwash()
        : false;

    String out;
    serializeJson(doc, out);
    return out;
}

SensorBase* SensorManager::createPHSensor() {
    return new PHSensor(0x48, 0);
}

SensorBase* SensorManager::createORPSensor() {
    return new ORPSensor(0x48, 1);
}

SensorBase* SensorManager::createWaterTempSensor() {
    return new DallasTemperatureSensor(DS18B20_WATER_PIN, 0, "Water Temperature");
}

SensorBase* SensorManager::createAirTempSensor() {
    return new DallasTemperatureSensor(DS18B20_AIR_PIN, 0, "Air Temperature");
}

SensorBase* SensorManager::createPressureSensor() {
    return new PressureSensor(0x48, 2);
}
