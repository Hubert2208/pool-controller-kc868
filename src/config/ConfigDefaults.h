#ifndef CONFIG_DEFAULTS_H
#define CONFIG_DEFAULTS_H

#include "ConfigManager.h"

// ─────────────────────────────────────────────────────────────────────────────
//  CONFIG DEFAULTS — SINGLE SOURCE OF TRUTH
//  ─────────────────────────────────────────────────────────────────────────────
//  JEDER konfigurierbare Default-Wert wird HIER gesetzt.
//  Diese Datei ist die EINZIGE Stelle, die du bearbeiten musst, um
//  Werkseinstellungen zu ändern.
//
//  Wie die Defaults aufs Board kommen:
//   1. Beim ersten Boot (kein /config.json auf LittleFS)
//   2. Bei CONFIG_VERSION-Änderung (alte Config wird verworfen)
//   3. Nach LittleFS-Formatierung
//
//  CONFIG_VERSION in ConfigManager.h erhöhen, wenn du hier Felder
//  hinzufügst/entfernst → bestehende Boards bekommen dann die neuen
//  Defaults beim nächsten Boot.
// ─────────────────────────────────────────────────────────────────────────────

// Include local secrets if available (not committed to git)
#if __has_include("secrets.h")
  #include "secrets.h"
  #define DEFAULT_WIFI_SSID     SECRET_WIFI_SSID
  #define DEFAULT_WIFI_PASSWORD SECRET_WIFI_PASSWORD
  #define DEFAULT_MQTT_BROKER   SECRET_MQTT_BROKER
  #define DEFAULT_MQTT_USERNAME SECRET_MQTT_USERNAME
  #define DEFAULT_MQTT_PASSWORD SECRET_MQTT_PASSWORD
#else
  #define DEFAULT_WIFI_SSID     "Mayer2"
  #define DEFAULT_WIFI_PASSWORD "Moritz26tOR"
  #define DEFAULT_MQTT_BROKER   "192.168.178.223"
  #define DEFAULT_MQTT_USERNAME "mqttuser"
  #define DEFAULT_MQTT_PASSWORD "mqttuser"
#endif

namespace SetDefaults {

inline void apply(AppConfig& cfg) {

    // ═══════════════════════════════════════════════════════════════════
    //  CONFIG VERSION
    // ═══════════════════════════════════════════════════════════════════
    cfg.configVersion = CONFIG_VERSION;
    cfg.logLevel = 1;
    cfg.loopDelayMs = 100;

    // ═══════════════════════════════════════════════════════════════════
    //  WIFI
    // ═══════════════════════════════════════════════════════════════════
    cfg.wifi.ssid        = DEFAULT_WIFI_SSID;
    cfg.wifi.password    = DEFAULT_WIFI_PASSWORD;
    cfg.wifi.hostname    = "poolcontroller";
    cfg.wifi.fallbackAP  = false;
    cfg.wifi.apSSID      = "PoolController-AP";
    cfg.wifi.apPassword  = "12345678";

    // ═══════════════════════════════════════════════════════════════════
    //  MQTT
    // ═══════════════════════════════════════════════════════════════════
    cfg.mqtt.broker      = DEFAULT_MQTT_BROKER;
    cfg.mqtt.port        = 1883;
    cfg.mqtt.clientId    = "poolcontroller";
    cfg.mqtt.username    = DEFAULT_MQTT_USERNAME;
    cfg.mqtt.password    = DEFAULT_MQTT_PASSWORD;
    cfg.mqtt.baseTopic   = "pool";
    cfg.mqtt.keepAliveSec = 60;

    // ═══════════════════════════════════════════════════════════════════
    //  RELAYS (8x KC868-A8)
    // ═══════════════════════════════════════════════════════════════════
    const char* relayNames[MAX_RELAYS] = {
        "Filter Pumpe", "pH Pumpe", "Chlor Pumpe",
        "Relay 4", "Relay 5", "Relay 6", "Relay 7", "Relay 8"
    };
    for (int i = 0; i < MAX_RELAYS; i++) {
        cfg.relays[i].channel      = i;
        cfg.relays[i].name         = relayNames[i];
        cfg.relays[i].normallyOpen = true;
        cfg.relays[i].maxOnTimeSec = 0;
    }
    cfg.relayCount = MAX_RELAYS;

    // ═══════════════════════════════════════════════════════════════════
    //  PUMPEN — Kanal-Zuordnung & Takt-Schutz
    // ═══════════════════════════════════════════════════════════════════
    //  minOnTimeSec  = Mindest-Einschaltdauer (verhindert Flattern)
    //  minOffTimeSec = Mindest-Ruhezeit zwischen Zyklen (verhindert Dauertakten)

    // ── pH Pumpe (Relais 1) ──────────────────────────────────────────
    cfg.phPump.relayChannel        = 1;
    cfg.phPump.minOnTimeSec        = 30;    // Taktschutz: min. Ein-Zeit
    cfg.phPump.minOffTimeSec       = 120;   // Taktschutz: min. Pause
    cfg.phPump.maxDailyRuntimeMin  = 120;   // Überdosierungsschutz: 2h/Tag

    // ── Chlor Pumpe (Relais 2) ───────────────────────────────────────
    cfg.chlorinePump.relayChannel        = 2;
    cfg.chlorinePump.minOnTimeSec        = 30;
    cfg.chlorinePump.minOffTimeSec       = 120;
    cfg.chlorinePump.maxDailyRuntimeMin  = 240;  // 4h/Tag

    // ── Filter Pumpe (Relais 0) ──────────────────────────────────────
    cfg.filterPump.relayChannel       = 0;
    cfg.filterPump.minOnTimeSec       = 60;    // min. 1 min laufen (Motorschutz)
    cfg.filterPump.minOffTimeSec      = 300;   // min. 5 min Pause (Druckabbau)
    cfg.filterPump.tempSlope          = 8.0;    // min/Tag pro °C Wassertemp
    cfg.filterPump.tempIntercept      = -40.0;  // Basis-Offset
    cfg.filterPump.windowStart        = "07:00";
    cfg.filterPump.windowEnd          = "21:00";
    cfg.filterPump.minCycleMinutes    = 60;     // kürzester Zyklus
    cfg.filterPump.maxCycleMinutes    = 480;    // längster Zyklus
    cfg.filterPump.maxDailyRuntimeMin = 720;    // Tageslimit: 12h

    // ═══════════════════════════════════════════════════════════════════
    //  PID-REGLER
    // ═══════════════════════════════════════════════════════════════════

    // pH PID: REVERSE acting — pH-Minus-Säure SENKT den pH-Wert
    // Wenn pH > setpoint → Output positiv → Pumpe EIN
    cfg.phPID.kp            = 1.2;
    cfg.phPID.ki            = 0.08;
    cfg.phPID.kd            = 0.04;
    cfg.phPID.setpoint      = 7.2;
    cfg.phPID.outputMin     = 0.0;
    cfg.phPID.outputMax     = 100.0;
    cfg.phPID.minOnTimeSec  = 15;
    cfg.phPID.minOffTimeSec = 60;
    cfg.phPID.reverseAction = true;

    // Chlor PID: DIRECT acting — Chlor HEBT den ORP-Wert
    // Wenn ORP < setpoint → Output positiv → Pumpe EIN
    cfg.chlorinePID.kp            = 0.8;
    cfg.chlorinePID.ki            = 0.05;
    cfg.chlorinePID.kd            = 0.02;
    cfg.chlorinePID.setpoint      = 650.0;
    cfg.chlorinePID.outputMin     = 0.0;
    cfg.chlorinePID.outputMax     = 100.0;
    cfg.chlorinePID.minOnTimeSec  = 30;
    cfg.chlorinePID.minOffTimeSec = 120;
    // reverseAction defaults to false (direct acting) for chlorine

    // ═══════════════════════════════════════════════════════════════════
    //  SENSOREN
    // ═══════════════════════════════════════════════════════════════════

    // pH-Sensor (EZO pH, I²C)
    cfg.phSensor.enabled          = true;
    cfg.phSensor.simulate         = false;
    cfg.phSensor.updateIntervalMs = 2000;
    cfg.phSensor.simMin           = 6.8;
    cfg.phSensor.simMax           = 7.6;
    cfg.phSensor.simDriftPerHour  = 0.05;

    // ORP-Sensor (EZO ORP, I²C)
    cfg.orpSensor.enabled          = true;
    cfg.orpSensor.simulate         = false;
    cfg.orpSensor.updateIntervalMs = 2000;
    cfg.orpSensor.simMin           = 200.0;
    cfg.orpSensor.simMax           = 800.0;
    cfg.orpSensor.simDriftPerHour  = 10.0;

    // Lufttemperatur (DS18B20, OneWire)
    cfg.tempAirSensor.enabled          = true;
    cfg.tempAirSensor.simulate         = false;
    cfg.tempAirSensor.updateIntervalMs = 5000;
    cfg.tempAirSensor.simMin           = 10.0;
    cfg.tempAirSensor.simMax           = 40.0;
    cfg.tempAirSensor.simDriftPerHour  = 0.0;

    // Wassertemperatur (DS18B20, OneWire)
    cfg.tempWaterSensor.enabled          = true;
    cfg.tempWaterSensor.simulate         = false;
    cfg.tempWaterSensor.updateIntervalMs = 5000;
    cfg.tempWaterSensor.simMin           = 5.0;
    cfg.tempWaterSensor.simMax           = 35.0;
    cfg.tempWaterSensor.simDriftPerHour  = 0.0;

    // Drucksensor (analog, ADS1115)
    cfg.pressureSensor.enabled          = true;
    cfg.pressureSensor.simulate         = false;
    cfg.pressureSensor.updateIntervalMs = 5000;
    cfg.pressureSensor.simMin           = 0.0;
    cfg.pressureSensor.simMax           = 2.5;
    cfg.pressureSensor.simDriftPerHour  = 0.1;
}

} // namespace SetDefaults

#endif
