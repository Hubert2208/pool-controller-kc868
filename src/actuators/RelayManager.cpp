#include "RelayManager.h"

RelayManager::RelayManager()
    : _state(0xFF)   // all bits 1 = all relays OFF (active-LOW)
    , _initialized(false)
{
}

void RelayManager::begin() {
    Wire.beginTransmission(KC868_A8_PCF8574_ADDR);
    Wire.write(0xFF);  // all relays off
    Wire.endTransmission();
    _state = 0xFF;
    _initialized = true;
    log_i("Relay manager initialized (PCF8574 @ 0x%02X, %d relays, active-LOW)",
          KC868_A8_PCF8574_ADDR, KC868_A8_RELAY_COUNT);
}

void RelayManager::writePCF8574() {
    Wire.beginTransmission(KC868_A8_PCF8574_ADDR);
    Wire.write(_state);
    Wire.endTransmission();
}

bool RelayManager::setRelay(uint8_t channel, bool state) {
    if (channel >= KC868_A8_RELAY_COUNT) {
        log_e("Invalid relay channel: %d", channel);
        return false;
    }

    if (state) {
        // ON: clear the bit (active-LOW → 0 = relay energized)
        _state &= ~(1 << channel);
    } else {
        // OFF: set the bit (1 = relay de-energized)
        _state |= (1 << channel);
    }

    writePCF8574();

    log_i("Relay %d: %s (PCF8574 byte=0x%02X)", channel, state ? "ON" : "OFF", _state);
    return true;
}

bool RelayManager::toggleRelay(uint8_t channel) {
    if (channel >= KC868_A8_RELAY_COUNT) return false;
    bool currentState = !(_state & (1 << channel));  // bit=0 → ON
    return setRelay(channel, !currentState);
}

bool RelayManager::getRelayState(uint8_t channel) const {
    if (channel >= KC868_A8_RELAY_COUNT) return false;
    // bit is 0 when relay is ON (active-LOW)
    return !(_state & (1 << channel));
}

void RelayManager::allOff() {
    _state = 0xFF;  // all bits 1 = all off
    writePCF8574();
    log_i("All relays turned off (PCF8574)");
}
