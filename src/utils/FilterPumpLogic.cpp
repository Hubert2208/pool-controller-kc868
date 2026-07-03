#include "FilterPumpLogic.h"
#include "TimingUtils.h"

FilterPumpLogic::FilterPumpLogic(ConfigManager& config, PumpController& pump)
    : _config(config)
    , _pump(pump)
    , _enabled(true)
    , _dailyRequiredMinutes(0.0f)
    , _lastCycleOffTime(0)
    , _lastUpdate(0)
{
}

void FilterPumpLogic::begin() {
    log_i("Filter pump logic initialized");
}

void FilterPumpLogic::update(float waterTemperature) {
    if (!_enabled) {
        if (_pump.isOn()) _pump.forceOff();
        return;
    }

    unsigned long now = millis();
    unsigned long dt = now - _lastUpdate;

    // Recalculate daily requirements every 5 minutes
    if (dt >= 300000 || _lastUpdate == 0) {
        _dailyRequiredMinutes = calculateDailyRuntime(waterTemperature);
        _lastUpdate = now;

        log_i("Filter: water=%.1f°C, required=%.0f min/day, window=%s",
              waterTemperature, _dailyRequiredMinutes,
              isInOperatingWindow() ? "open" : "closed");
    }

    manageCycles();
}

float FilterPumpLogic::calculateDailyRuntime(float waterTemp) const {
    FilterPumpConfig& cfg = _config.get().filterPump;

    // Linear model: warmer water needs more filtration (algae/bacteria growth)
    float runtime = cfg.tempSlope * waterTemp + cfg.tempIntercept;

    // Clamp to reasonable range
    if (runtime < 60.0f) runtime = 60.0f;      // minimum 1 hour
    if (runtime > 1440.0f) runtime = 1440.0f;  // maximum 24 hours

    return runtime;
}

bool FilterPumpLogic::isInOperatingWindow() const {
    time_t now = time(nullptr);
    if (now < 100000) {
        // NTP not yet synced, allow operation
        return true;
    }

    FilterPumpConfig& cfg = _config.get().filterPump;
    return isWithinTimeWindow(now,
                              cfg.windowStart.c_str(),
                              cfg.windowEnd.c_str());
}

int FilterPumpLogic::getRequiredCyclesPerDay() const {
    FilterPumpConfig& cfg = _config.get().filterPump;
    float runtimeMinutes = _dailyRequiredMinutes;

    // Each cycle should be between minCycleMinutes and maxCycleMinutes
    int cycles = ceil(runtimeMinutes / cfg.maxCycleMinutes);
    if (cycles < 1) cycles = 1;

    return cycles;
}

void FilterPumpLogic::manageCycles() {
    if (!isInOperatingWindow()) {
        if (_pump.isOn()) {
            log_i("Filter pump OFF (outside operating window)");
            _pump.forceOff();
        }
        _lastCycleOffTime = millis();
        return;
    }

    if (_dailyRequiredMinutes <= 0) return;

    unsigned long now = millis();
    unsigned long currentRuntime = _pump.getRuntimeMinutes() * 60000;  // in ms
    unsigned long targetRuntime = (unsigned long)(_dailyRequiredMinutes * 60000);

    // If we've already met today's target, turn off
    if (currentRuntime >= targetRuntime) {
        if (_pump.isOn()) {
            log_i("Filter pump OFF (daily target met: %.0f min)", _dailyRequiredMinutes);
            _pump.forceOff();
        }
        return;
    }

    // Calculate remaining runtime needed
    unsigned long remainingMs = targetRuntime - currentRuntime;
    FilterPumpConfig& cfg = _config.get().filterPump;

    // Determine cycle duration
    unsigned long cycleDurationMs = (unsigned long)(cfg.maxCycleMinutes * 60000);
    if (remainingMs < cycleDurationMs) {
        cycleDurationMs = remainingMs;
    }
    // But not less than minimum cycle
    unsigned long minCycleMs = (unsigned long)(cfg.minCycleMinutes * 60000);

    if (!_pump.isOn()) {
        // Decide whether to start a new cycle
        unsigned long timeSinceOff = now - _lastCycleOffTime;

        // Wait at least minCycleMinutes between cycles (unless last cycle was short)
        if (timeSinceOff >= minCycleMs) {
            _pump.turnOn();
            log_i("Filter pump ON (remaining %.0f min, cycle %lu min)",
                  remainingMs / 60000.0f, cycleDurationMs / 60000);
        }
    } else {
        // Check if current cycle has run long enough
        unsigned long cycleDuration = now - _pump.getLastOnDuration();
        // Actually getCycleStartTime isn't exposed, so use simpler check:
        unsigned long onDuration = (_pump.getLastOnDuration() > 0)
            ? _pump.getLastOnDuration() : 0;

        if (_pump.getLastOnDuration() > 0) {
            unsigned long currentCycle = millis() - _pump.getLastOffDuration();
            currentCycle = _pump.getLastOffDuration(); // still 0 while on...

            // Simpler: just check if pump has been running longer than target
            // We use runtime today to determine if we need more
            if (currentRuntime >= targetRuntime) {
                _pump.forceOff();
                _lastCycleOffTime = now;
            }
        }
    }
}

void FilterPumpLogic::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!enabled && _pump.isOn()) {
        _pump.forceOff();
    }
    log_i("Filter pump logic %s", enabled ? "enabled" : "disabled");
}

String FilterPumpLogic::getStateJSON() const {
    StaticJsonDocument<256> doc;
    doc["enabled"] = _enabled;
    doc["required_runtime_min"] = _dailyRequiredMinutes;
    doc["pump_on"] = _pump.isOn();
    doc["runtime_today_min"] = _pump.getRuntimeMinutes();
    doc["in_window"] = isInOperatingWindow();
    doc["cycles_per_day"] = getRequiredCyclesPerDay();
    String out;
    serializeJson(doc, out);
    return out;
}
