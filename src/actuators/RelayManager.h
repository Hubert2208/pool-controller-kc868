#ifndef RELAY_MANAGER_H
#define RELAY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>

#define KC868_A8_RELAY_COUNT 8
#define KC868_A8_PCF8574_ADDR 0x24  // PCF8574 I2C expander for relay control

/**
 * Relay Manager for KC868-A8.
 *
 * The KC868-A8 uses a PCF8574 I2C I/O expander at address 0x24 to
 * control its 8 relays (not direct GPIO pins!). The relay driver
 * circuit is active-LOW: writing 0 to a PCF8574 pin turns the relay ON,
 * writing 1 turns it OFF. This matches the "inverted: true" setting in
 * Kincony's official ESPHome YAML.
 */
class RelayManager {
public:
    RelayManager();
    ~RelayManager() = default;

    // Initialize PCF8574, all relays off
    void begin();

    // Set relay state (true=ON, false=OFF)
    bool setRelay(uint8_t channel, bool state);

    // Toggle relay state
    bool toggleRelay(uint8_t channel);

    // Get relay state (true=ON, false=OFF)
    bool getRelayState(uint8_t channel) const;

    // Turn all relays off
    void allOff();

    // Number of relays
    uint8_t getRelayCount() const { return KC868_A8_RELAY_COUNT; }

private:
    // Write current state byte to PCF8574 over I2C
    void writePCF8574();

    uint8_t _state;      // current output byte (bit=0 → relay ON)
    bool _initialized;
};

#endif // RELAY_MANAGER_H
