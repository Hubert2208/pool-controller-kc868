#ifndef CONFIG_DEFAULTS_H
#define CONFIG_DEFAULTS_H

#include "ConfigManager.h"

namespace SetDefaults {

inline void apply(AppConfig& _config) {
    // ─── WiFi defaults (from ConfigManager.h struct) ───
    _config.wifi.ssid        = "Mayer2";
    _config.wifi.password    = "Moritz26tOR";
    _config.wifi.hostname    = "poolcontroller";
    _config.wifi.fallbackAP  = false;
    _config.wifi.apSSID      = "PoolController-AP";
    _config.wifi.apPassword  = "12345678";

    // ─── MQTT defaults (from ConfigManager.h struct) ───
    _config.mqtt.broker      = "192.168.178.223";
    _config.mqtt.port        = 1883;
    _config.mqtt.clientId    = "poolcontroller";
    _config.mqtt.username    = "";
    _config.mqtt.password    = "";
    _config.mqtt.baseTopic   = "pool";
    _config.mqtt.keepAliveSec = 60;

    const char* relayNames[MAX_RELAYS] = {
        "Filter Pumpe", "pH Pumpe", "Chlor Pumpe",
        "Relay 4", "Relay 5", "Relay 6",
        "Relay 7", "Relay 8"
    };
    for (int i = 0; i < MAX_RELAYS; i++) {
        _config.relays[i].channel = i;
        _config.relays[i].name = relayNames[i];
        _config.relays[i].normallyOpen = true;
        _config.relays[i].maxOnTimeSec = 0;
    }
    _config.relayCount = MAX_RELAYS;

    _config.phPump.relayChannel       = 1;
    _config.chlorinePump.relayChannel = 2;
    _config.filterPump.relayChannel   = 0;

    _config.phSensor.simMin          = 6.8f;
    _config.phSensor.simMax          = 7.6f;
    _config.phSensor.simDriftPerHour = 0.05f;

    _config.orpSensor.simMin          = 200.0f;
    _config.orpSensor.simMax          = 800.0f;
    _config.orpSensor.simDriftPerHour = 10.0f;

    _config.tempAirSensor.simMin       = 10.0f;
    _config.tempAirSensor.simMax       = 40.0f;
    _config.tempWaterSensor.simMin     = 5.0f;
    _config.tempWaterSensor.simMax     = 35.0f;

    _config.pressureSensor.simMin          = 0.0f;
    _config.pressureSensor.simMax          = 2.5f;
    _config.pressureSensor.simDriftPerHour = 0.1f;

    // pH PID: REVERSE acting — adding pH-Minus DECREASES pH
    // When pH > setpoint → output should be positive → pump ON
    _config.phPID.kp            = 1.2f;
    _config.phPID.ki            = 0.08f;
    _config.phPID.kd            = 0.04f;
    _config.phPID.setpoint      = 7.2f;
    _config.phPID.minOnTimeSec  = 15;
    _config.phPID.minOffTimeSec = 60;
    _config.phPID.reverseAction = true;

    // Chlorine PID: DIRECT acting — adding chlorine INCREASES ORP
    // When ORP < setpoint → output should be positive → pump ON
    _config.chlorinePID.kp            = 0.8f;
    _config.chlorinePID.ki            = 0.05f;
    _config.chlorinePID.kd            = 0.02f;
    _config.chlorinePID.setpoint      = 650.0f;
    _config.chlorinePID.minOnTimeSec  = 30;
    _config.chlorinePID.minOffTimeSec = 120;
    // reverseAction defaults to false (direct acting) for chlorine
}

} // namespace SetDefaults

#endif
