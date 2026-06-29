#include "PumpController.h"
#include <ArduinoJson.h>

PumpController::PumpController(RelayManager& relayManager, uint8_t relayChannel, const char* name)
    : _relayManager(relayManager)
    , _relayChannel(relayChannel)
    , _minOnTimeMs(30000)
    , _minOffTimeMs(120000)
    , _lastOnTime(0)
    , _lastOffTime(0)
    , _cycleStartTime(0)
    , _dailyRuntimeMs(0)
    , _lastDailyReset(0)
    , _initialized(false)
{
    strncpy(_name, name, sizeof(_name) - 1);
    _name[sizeof(_name) - 1] = '\0';
}

void PumpController::begin() {
    _lastOnTime = 0;
    _lastOffTime = millis();  // So we can turn on immediately = factory state
    _cycleStartTime = 0;
    _dailyRuntimeMs = 0;
    _lastDailyReset = millis();
    _initialized = true;

    log_i("Pump '%s' initialized on relay %d", _name, _relayChannel);
}

bool PumpController::turnOn() {
    if (!_initialized) return false;

    unsigned long now = millis();
    unsigned long offDuration = now - _lastOffTime;

    // Check minimum off time
    if (_lastOffTime > 0 && offDuration < (unsigned long)_minOffTimeMs) {
        unsigned long remaining = _minOffTimeMs - offDuration;
        log_w("Pump '%s' min off time not met: %lu ms remaining", _name, remaining);
        return false;
    }

    if (_relayManager.setRelay(_relayChannel, true)) {
        _lastOnTime = now;
        _cycleStartTime = now;
        return true;
    }
    return false;
}

bool PumpController::turnOff() {
    if (!_initialized) return false;

    unsigned long now = millis();
    unsigned long onDuration = (_cycleStartTime > 0) ? (now - _cycleStartTime) : 0;

    // Check minimum on time
    if (onDuration > 0 && onDuration < (unsigned long)_minOnTimeMs) {
        unsigned long remaining = _minOnTimeMs - onDuration;
        log_w("Pump '%s' min on time not met: %lu ms remaining", _name, remaining);
        return false;
    }

    if (_relayManager.setRelay(_relayChannel, false)) {
        // Add to daily runtime
        _dailyRuntimeMs += onDuration;
        _lastOffTime = now;
        _cycleStartTime = 0;
        return true;
    }
    return false;
}

bool PumpController::forceOff() {
    if (!_initialized) return false;

    unsigned long now = millis();
    unsigned long onDuration = (_cycleStartTime > 0) ? (now - _cycleStartTime) : 0;

    if (_relayManager.setRelay(_relayChannel, false)) {
        _dailyRuntimeMs += onDuration;
        _lastOffTime = now;
        _cycleStartTime = 0;
        log_w("Pump '%s' force turned OFF", _name);
        return true;
    }
    return false;
}

bool PumpController::isOn() const {
    return _relayManager.getRelayState(_relayChannel);
}

unsigned long PumpController::getRuntimeToday() const {
    updateDailyReset();
    unsigned long currentCycle = 0;
    if (isOn() && _cycleStartTime > 0) {
        currentCycle = millis() - _cycleStartTime;
    }
    return _dailyRuntimeMs + currentCycle;
}

unsigned long PumpController::getRuntimeMinutes() const {
    return getRuntimeToday() / 60000;
}

unsigned long PumpController::getLastOnDuration() const {
    if (!isOn() && _lastOffTime > _lastOnTime) {
        return _lastOffTime - _lastOnTime;
    }
    if (isOn() && _cycleStartTime > 0) {
        return millis() - _cycleStartTime;
    }
    return 0;
}

unsigned long PumpController::getLastOffDuration() const {
    if (isOn() && _lastOnTime > _lastOffTime) {
        return _lastOnTime - _lastOffTime;
    }
    if (!isOn() && _lastOffTime > 0) {
        return millis() - _lastOffTime;
    }
    return 0;
}

void PumpController::setMinOnTime(unsigned long ms) {
    _minOnTimeMs = ms;
}

void PumpController::setMinOffTime(unsigned long ms) {
    _minOffTimeMs = ms;
}

void PumpController::resetDailyRuntime() const {
    _dailyRuntimeMs = 0;
    _lastDailyReset = millis();
    log_i("Pump '%s' daily runtime reset", _name);
}

void PumpController::updateDailyReset() const {
    // Reset daily runtime at midnight
    unsigned long now = millis();
    unsigned long msSinceMidnight = now % 86400000UL;
    unsigned long msSinceReset = now - _lastDailyReset;

    // If more than 24h has passed or current time is before last reset (wrap), reset
    if (msSinceReset > 86400000UL) {
        resetDailyRuntime();
    }

    // Check if midnight just passed
    static unsigned long lastCheck = now;
    if (lastCheck > now) lastCheck = now;  // millis() wrap
    lastCheck = now;
}

String PumpController::getStateJSON() const {
    StaticJsonDocument<256> doc;
    doc["name"] = _name;
    doc["relay"] = _relayChannel;
    doc["on"] = isOn();
    doc["runtime_today_min"] = getRuntimeMinutes();
    doc["last_on_duration_ms"] = getLastOnDuration();
    doc["last_off_duration_ms"] = getLastOffDuration();
    String out;
    serializeJson(doc, out);
    return out;
}