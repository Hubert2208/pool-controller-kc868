#ifndef RELAY_MANAGER_H
#define RELAY_MANAGER_H

#include <Arduino.h>

#define KC868_A8_RELAY_COUNT 8

// Typical Kincony KC868-A8 GPIO to relay mapping
static const uint8_t KC868_A8_RELAY_PINS[KC868_A8_RELAY_COUNT] = {
    13,  // Relay 1  (Filter Pumpe)
    14,  // Relay 2  (pH Pumpe)
    15,  // Relay 3  (Chlor Pumpe)
    16,  // Relay 4
    17,  // Relay 5
    18,  // Relay 6
    19,  // Relay 7
    21   // Relay 8
};

class RelayManager {
public:
    RelayManager();
    ~RelayManager() = default;

    // Initialize all relay GPIOs, all OFF
    void begin();

    // Set relay channel (0-7) to state (true=ON, false=OFF)
    bool setRelay(uint8_t channel, bool state);

    // Toggle relay state
    bool toggleRelay(uint8_t channel);

    // Get current state of relay channel
    bool getRelayState(uint8_t channel) const;

    // Turn all relays off
    void allOff();

    // Get number of relays
    uint8_t getRelayCount() const { return KC868_A8_RELAY_COUNT; }

    // Get GPIO pin for relay channel
    uint8_t getRelayPin(uint8_t channel) const;

private:
    bool _states[KC868_A8_RELAY_COUNT];
    bool _initialized;
};

#endif // RELAY_MANAGER_H