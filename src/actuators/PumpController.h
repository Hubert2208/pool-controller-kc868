#ifndef PUMP_CONTROLLER_H
#define PUMP_CONTROLLER_H

#include <Arduino.h>
#include "RelayManager.h"

class PumpController {
public:
    static const int MAX_DEPENDENTS = 4;

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

    // Dependents: pumps that may only run when this pump is running.
    // When this pump stops, all dependents are force-stopped.
    void addDependent(PumpController* dep);
    bool hasDependentsOn() const;

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

    bool _initialized;

    // Dependent pumps (must be ON for these to run)
    PumpController* _master;             // this pump requires _master to be ON
    PumpController* _dependents[MAX_DEPENDENTS]; // these pumps require THIS pump
    int _dependentCount;

    void forceOffDependents();

    void updateDailyReset() const;
};

#endif // PUMP_CONTROLLER_H
