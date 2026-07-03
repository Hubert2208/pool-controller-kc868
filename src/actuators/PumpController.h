#ifndef PUMP_CONTROLLER_H
#define PUMP_CONTROLLER_H

#include <Arduino.h>
#include "RelayManager.h"

class PumpController {
public:
    PumpController(RelayManager& relayManager, uint8_t relayChannel, const char* name);

    void begin();

    bool turnOn();
    bool turnOff();
    bool forceOff();
    bool isOn() const;

    unsigned long getRuntimeToday() const;
    unsigned long getRuntimeMinutes() const;
    unsigned long getLastOnDuration() const;
    unsigned long getLastOffDuration() const;

    void setMinOnTime(unsigned long ms);
    void setMinOffTime(unsigned long ms);
    void resetDailyRuntime() const;

    // Interlock: this pump may only start when _interlock pump is running
    void setInterlock(PumpController* interlock) { _interlock = interlock; }
    PumpController* getInterlock() const { return _interlock; }

    const char* getName() const { return _name; }
    uint8_t getRelayChannel() const { return _relayChannel; }

    String getStateJSON() const;

private:
    RelayManager& _relayManager;
    uint8_t _relayChannel;
    char _name[24];

    unsigned long _minOnTimeMs;
    unsigned long _minOffTimeMs;
    unsigned long _lastOnTime;
    unsigned long _lastOffTime;
    unsigned long _cycleStartTime;
    mutable unsigned long _dailyRuntimeMs;
    mutable unsigned long _lastDailyReset;

    PumpController* _interlock;   // dependency pump (must be ON to allow start)
    bool _initialized;

    void updateDailyReset() const;
};

#endif // PUMP_CONTROLLER_H
