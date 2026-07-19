#include "PumpController.h"
#include <ArduinoJson.h>
#include <time.h>

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
    , _lastResetDay(-1)
    , _initialized(false)
    , _master(nullptr)
    , _dependentCount(0)
{
    strncpy(_name, name, sizeof(_name) - 1);
    _name[sizeof(_name) - 1] = '\0';
}

void PumpController::begin() {
    _lastOnTime = 0;
    _lastOffTime = millis();
    _cycleStartTime = 0;
    _dailyRuntimeMs = 0;
    _lastDailyReset = millis();
    _initialized = true;

    log_i("Pump '%s' initialized on relay %d", _name, _relayChannel);
}

bool PumpController::turnOn() {
    if (!_initialized) return false;

    // Interlock: this pump requires master to be running
    if (_master && !_master->isOn()) {
        log_w("Pump '%s' interlock blocked: '%s' not running", _name, _master->getName());
        return false;
    }

    unsigned long now = millis();
    unsigned long offDuration = now - _lastOffTime;

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

    if (onDuration > 0 && onDuration < (unsigned long)_minOnTimeMs) {
        unsigned long remaining = _minOnTimeMs - onDuration;
        log_w("Pump '%s' min on time not met: %lu ms remaining", _name, remaining);
        return false;
    }

    if (_relayManager.setRelay(_relayChannel, false)) {
        _dailyRuntimeMs += onDuration;
        _lastOffTime = now;
        _cycleStartTime = 0;
        forceOffDependents();
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
        forceOffDependents();
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

void PumpController::addDependent(PumpController* dep) {
    if (_dependentCount < MAX_DEPENDENTS) {
        _dependents[_dependentCount++] = dep;
        dep->_master = this;
        log_i("Pump '%s' added dependent '%s'", _name, dep->getName());
    }
}

bool PumpController::hasDependentsOn() const {
    for (int i = 0; i < _dependentCount; i++) {
        if (_dependents[i]->isOn()) return true;
    }
    return false;
}

void PumpController::forceOffDependents() {
    for (int i = 0; i < _dependentCount; i++) {
        if (_dependents[i]->isOn()) {
            log_i("Pump '%s' force-stopping dependent '%s'", _name, _dependents[i]->getName());
            _dependents[i]->forceOff();
        }
    }
}

void PumpController::resetDailyRuntime() const {
    _dailyRuntimeMs = 0;
    _lastDailyReset = millis();
    log_i("Pump '%s' daily runtime reset", _name);
}

void PumpController::updateDailyReset() const {
    time_t t = time(nullptr);
    if (t > 100000) {
        // NTP synced: use calendar day to detect midnight crossing
        struct tm* ti = localtime(&t);
        if (_lastResetDay < 0) _lastResetDay = ti->tm_yday;
        if (ti->tm_yday != _lastResetDay) {
            resetDailyRuntime();
            _lastResetDay = ti->tm_yday;
        }
    } else {
        // No NTP: reset every 24h of uptime
        unsigned long now = millis();
        if (now - _lastDailyReset > 86400000UL) {
            resetDailyRuntime();
        }
    }
}
