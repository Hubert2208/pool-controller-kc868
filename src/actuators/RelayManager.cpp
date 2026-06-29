#include "RelayManager.h"

RelayManager::RelayManager()
    : _initialized(false)
{
    for (int i = 0; i < KC868_A8_RELAY_COUNT; i++) {
        _states[i] = false;
    }
}

void RelayManager::begin() {
    for (int i = 0; i < KC868_A8_RELAY_COUNT; i++) {
        uint8_t pin = KC868_A8_RELAY_PINS[i];
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);  // Relays are active HIGH on KC868-A8
        _states[i] = false;
    }
    _initialized = true;
    log_i("Relay manager initialized (%d relays)", KC868_A8_RELAY_COUNT);
}

bool RelayManager::setRelay(uint8_t channel, bool state) {
    if (channel >= KC868_A8_RELAY_COUNT) {
        log_e("Invalid relay channel: %d", channel);
        return false;
    }

    uint8_t pin = KC868_A8_RELAY_PINS[channel];
    digitalWrite(pin, state ? HIGH : LOW);
    _states[channel] = state;

    log_i("Relay %d (pin %d): %s", channel, pin, state ? "ON" : "OFF");
    return true;
}

bool RelayManager::toggleRelay(uint8_t channel) {
    if (channel >= KC868_A8_RELAY_COUNT) return false;
    bool newState = !_states[channel];
    return setRelay(channel, newState);
}

bool RelayManager::getRelayState(uint8_t channel) const {
    if (channel >= KC868_A8_RELAY_COUNT) return false;
    return _states[channel];
}

void RelayManager::allOff() {
    for (int i = 0; i < KC868_A8_RELAY_COUNT; i++) {
        setRelay(i, false);
    }
    log_i("All relays turned off");
}

uint8_t RelayManager::getRelayPin(uint8_t channel) const {
    if (channel >= KC868_A8_RELAY_COUNT) return 0;
    return KC868_A8_RELAY_PINS[channel];
}