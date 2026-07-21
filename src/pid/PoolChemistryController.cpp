#include "PoolChemistryController.h"

PoolChemistryController::PoolChemistryController(ConfigManager& config, SensorManager& sensors,
                                                 PumpController& phPump, PumpController& chlorinePump)
    : _config(config)
    , _sensors(sensors)
    , _phPump(phPump)
    , _chlorinePump(chlorinePump)
    , _phEnabled(true)
    , _chlorineEnabled(true)
    , _lastPHOutput(0.0f)
    , _lastChlorineOutput(0.0f)
    , _lastUpdate(0)
    , _lastPHCycleStart(0)
    , _lastChlorineCycleStart(0)
{
}

void PoolChemistryController::begin() {
    AppConfig& cfg = _config.get();

    _phPID.setTunings(cfg.phPID.kp, cfg.phPID.ki, cfg.phPID.kd);
    _phPID.setSetpoint(cfg.phPID.setpoint);
    _phPID.setOutputLimits(cfg.phPID.outputMin, cfg.phPID.outputMax);
    _phPID.setReverseAction(cfg.phPID.reverseAction);

    _chlorinePID.setTunings(cfg.chlorinePID.kp, cfg.chlorinePID.ki, cfg.chlorinePID.kd);
    _chlorinePID.setSetpoint(cfg.chlorinePID.setpoint);
    _chlorinePID.setOutputLimits(cfg.chlorinePID.outputMin, cfg.chlorinePID.outputMax);
    _chlorinePID.setReverseAction(cfg.chlorinePID.reverseAction);

    log_i("Pool chemistry controller initialized");
    log_i("  pH PID: Kp=%.2f Ki=%.4f Kd=%.4f setpoint=%.1f reverse=%s",
          cfg.phPID.kp, cfg.phPID.ki, cfg.phPID.kd, cfg.phPID.setpoint,
          cfg.phPID.reverseAction ? "true" : "false");
    log_i("  Cl PID: Kp=%.2f Ki=%.4f Kd=%.4f setpoint=%.0f mV reverse=%s",
          cfg.chlorinePID.kp, cfg.chlorinePID.ki, cfg.chlorinePID.kd, cfg.chlorinePID.setpoint,
          cfg.chlorinePID.reverseAction ? "true" : "false");
}

void PoolChemistryController::update() {
    if (!_phEnabled && !_chlorineEnabled) return;

    unsigned long now = millis();
    float dtSec = (now - _lastUpdate) / 1000.0f;
    if (_lastUpdate == 0) dtSec = 0.1f;
    _lastUpdate = now;

    if (_phEnabled) {
        updatePHPID(dtSec);
    } else {
        _phPump.forceOff();
    }

    if (_chlorineEnabled) {
        updateChlorinePID(dtSec);
    } else {
        _chlorinePump.forceOff();
    }

    // Update simulation with current pump states for pump-aware drift
    _sensors.setPHPumpActive(_phPump.isOn());
    _sensors.setChlorinePumpActive(_chlorinePump.isOn());
}

void PoolChemistryController::updatePHPID(float dtSec) {
    AppConfig& cfg = _config.get();
    float currentPH = _sensors.getPH();
    float pidOutput = _phPID.compute(currentPH, dtSec);
    _lastPHOutput = pidOutput;

    // PID output is 0..100 representing percentage of time the pump should be on
    // For pool chemistry, we use a bang-bang approach: 
    // if output > threshold AND minimum off time elapsed, turn on
    // if output < threshold OR maximum daily runtime exceeded, turn off
    bool shouldBeOn = (pidOutput > 5.0f);  // 5% threshold

    unsigned long now = millis();
    unsigned long phOnTime = _phPump.getLastOnDuration();
    unsigned long phOffTime = _phPump.getLastOffDuration();

    if (shouldBeOn && !_phPump.isOn()) {
        // Check minimum off time
        if (phOffTime >= (unsigned long)(cfg.phPID.minOffTimeSec * 1000)) {
            // Check daily runtime limit
            if (_phPump.getRuntimeToday() < (unsigned long)(cfg.phPump.maxDailyRuntimeMin * 60000)) {
                _phPump.turnOn();
                _lastPHCycleStart = now;
                log_i("pH pump ON (output=%.1f%%, pH=%.2f, setpoint=%.2f)",
                      pidOutput, currentPH, cfg.phPID.setpoint);
            }
        }
    } else if (!shouldBeOn && _phPump.isOn()) {
        // Check minimum on time
        unsigned long onDuration = now - _lastPHCycleStart;
        if (onDuration >= (unsigned long)(cfg.phPID.minOnTimeSec * 1000)) {
            _phPump.turnOff();
            log_i("pH pump OFF (output=%.1f%%, pH=%.2f)", pidOutput, currentPH);
        }
    } else if (_phPump.isOn()) {
        // Safety: force off if max daily runtime exceeded
        if (_phPump.getRuntimeToday() >= (unsigned long)(cfg.phPump.maxDailyRuntimeMin * 60000)) {
            _phPump.forceOff();
            log_w("pH pump force OFF (daily limit reached)");
        }
    }
}

void PoolChemistryController::updateChlorinePID(float dtSec) {
    AppConfig& cfg = _config.get();
    float currentORP = _sensors.getORP();
    float pidOutput = _chlorinePID.compute(currentORP, dtSec);
    _lastChlorineOutput = pidOutput;

    bool shouldBeOn = (pidOutput > 5.0f);

    unsigned long now = millis();
    unsigned long clOnTime = _chlorinePump.getLastOnDuration();
    unsigned long clOffTime = _chlorinePump.getLastOffDuration();

    if (shouldBeOn && !_chlorinePump.isOn()) {
        if (clOffTime >= (unsigned long)(cfg.chlorinePID.minOffTimeSec * 1000)) {
            if (_chlorinePump.getRuntimeToday() < (unsigned long)(cfg.chlorinePump.maxDailyRuntimeMin * 60000)) {
                _chlorinePump.turnOn();
                _lastChlorineCycleStart = now;
                log_i("Chlorine pump ON (output=%.1f%%, ORP=%.0f, setpoint=%.0f)",
                      pidOutput, currentORP, cfg.chlorinePID.setpoint);
            }
        }
    } else if (!shouldBeOn && _chlorinePump.isOn()) {
        unsigned long onDuration = now - _lastChlorineCycleStart;
        if (onDuration >= (unsigned long)(cfg.chlorinePID.minOnTimeSec * 1000)) {
            _chlorinePump.turnOff();
            log_i("Chlorine pump OFF (output=%.1f%%, ORP=%.0f)", pidOutput, currentORP);
        }
    } else if (_chlorinePump.isOn()) {
        if (_chlorinePump.getRuntimeToday() >= (unsigned long)(cfg.chlorinePump.maxDailyRuntimeMin * 60000)) {
            _chlorinePump.forceOff();
            log_w("Chlorine pump force OFF (daily limit reached)");
        }
    }
}

void PoolChemistryController::setPHSetpoint(float pH) {
    _phPID.setSetpoint(pH);
    _config.get().phPID.setpoint = pH;
    log_i("pH setpoint updated: %.2f", pH);
}

void PoolChemistryController::setORPSetpoint(float orpMV) {
    _chlorinePID.setSetpoint(orpMV);
    _config.get().chlorinePID.setpoint = orpMV;
    log_i("ORP setpoint updated: %.0f mV", orpMV);
}

void PoolChemistryController::setPHEnabled(bool enabled) {
    _phEnabled = enabled;
    if (!enabled) _phPump.forceOff();
    log_i("pH control %s", enabled ? "enabled" : "disabled");
}

void PoolChemistryController::setChlorineEnabled(bool enabled) {
    _chlorineEnabled = enabled;
    if (!enabled) _chlorinePump.forceOff();
    log_i("Chlorine control %s", enabled ? "enabled" : "disabled");
}

String PoolChemistryController::getStateJSON() {
    StaticJsonDocument<512> doc;

    JsonObject ph = doc.createNestedObject("ph");
    ph["enabled"] = _phEnabled;
    ph["setpoint"] = _phPID.getSetpoint();
    ph["pid_output"] = round(_lastPHOutput * 100.0f) / 100.0f;
    ph["pump_on"] = _phPump.isOn();
    ph["p"] = _phPID.getP();
    ph["i"] = _phPID.getI();
    ph["d"] = _phPID.getD();

    JsonObject cl = doc.createNestedObject("chlorine");
    cl["enabled"] = _chlorineEnabled;
    cl["setpoint"] = _chlorinePID.getSetpoint();
    cl["pid_output"] = round(_lastChlorineOutput * 100.0f) / 100.0f;
    cl["pump_on"] = _chlorinePump.isOn();
    cl["p"] = _chlorinePID.getP();
    cl["i"] = _chlorinePID.getI();
    cl["d"] = _chlorinePID.getD();

    String out;
    serializeJson(doc, out);
    return out;
}
