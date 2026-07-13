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
    // Praxis-übliche Faustformel: Wassertemperatur / 2 = Stunden → ×60 = Minuten
    // z.B. 20°C → 10h → 600min, 26°C → 13h → 780min, 30°C → 15h → 900min
    // Quelle: Branchenstandard für private Pools (Temperatur-abhängige Filterlaufzeit)
    float runtime = (waterTemp / 2.0f) * 60.0f;

    // Clamp: minimum 1 hour, maximum from config (default 24h)
    FilterPumpConfig& cfg = _config.get().filterPump;
    if (runtime < 60.0f) runtime = 60.0f;
    if (runtime > cfg.maxDailyRuntimeMin) runtime = cfg.maxDailyRuntimeMin;

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
    FilterPumpConfig& cfg = _config.get().filterPump;

    if (!isInOperatingWindow()) {
        if (_pump.isOn()) {
            log_i("Filter pump OFF (outside operating window)");
            _pump.forceOff();
        }
        _lastCycleOffTime = millis();
        return;
    }

    if (_dailyRequiredMinutes <= 0) return;

    // Guard: enforce configurable daily max runtime
    unsigned long todayMinutes = _pump.getRuntimeMinutes();
    unsigned long maxMinutes = (unsigned long)cfg.maxDailyRuntimeMin;
    if (todayMinutes >= maxMinutes) {
        if (_pump.isOn()) {
            log_i("Filter pump OFF (daily max %.0f min reached, today %lu min)",
                  cfg.maxDailyRuntimeMin, todayMinutes);
            _pump.forceOff();
        }
        return;
    }

    unsigned long now = millis();
    unsigned long currentRuntime = todayMinutes * 60000;  // in ms
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
    FilterPumpConfig& cfg = _config.get().filterPump;
    StaticJsonDocument<256> doc;
    doc["enabled"] = _enabled;
    doc["required_runtime_min"] = _dailyRequiredMinutes;
    doc["pump_on"] = _pump.isOn();
    doc["runtime_today_min"] = _pump.getRuntimeMinutes();
    doc["max_daily_min"] = cfg.maxDailyRuntimeMin;
    doc["in_window"] = isInOperatingWindow();
    doc["cycles_per_day"] = getRequiredCyclesPerDay();
    String out;
    serializeJson(doc, out);
    return out;
}
