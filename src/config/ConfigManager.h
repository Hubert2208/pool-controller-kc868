#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#define CONFIG_FILE "/config.json"
#define CONFIG_JSON_SIZE 4096
#define MAX_RELAYS 8
#define CONFIG_VERSION 6  // minOnTimeSec/minOffTimeSec added to FilterPumpConfig; pump config unified
#define WIFI_HOSTNAME_MAX 64
#define MQTT_TOPIC_MAX 128
#define STRING_BUF_SIZE 256

// ── Defaults: ConfigDefaults.h ist die EINZIGE Datei für Werkseinstellungen.
//    Struct-Initialisierer hier sind nur Fallback — in ConfigDefaults.h ändern!

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

struct PIDParams {
    float kp = 1.0;
    float ki = 0.1;
    float kd = 0.05;
    float setpoint = 7.2;
    float outputMin = 0.0;
    float outputMax = 100.0;
    int minOnTimeSec = 10;
    int minOffTimeSec = 60;
    bool reverseAction = false;

    String toJson() const;
    static PIDParams fromJson(JsonVariantConst json);
};

struct SensorConfig {
    bool enabled = true;
    bool simulate = false;
    int updateIntervalMs = 2000;
    float simMin = 0.0;
    float simMax = 100.0;
    float simDriftPerHour = 0.5;

    String toJson() const;
    static SensorConfig fromJson(JsonVariantConst json);
};

struct WifiConfig {
    String ssid = DEFAULT_WIFI_SSID;
    String password = DEFAULT_WIFI_PASSWORD;
    String hostname = "poolcontroller";
    bool fallbackAP = false;
    String apSSID = "PoolController-AP";
    String apPassword = "12345678";

    String toJson() const;
    static WifiConfig fromJson(JsonVariantConst json);
};

struct MqttConfig {
    String broker = DEFAULT_MQTT_BROKER;
    int port = 1883;
    String clientId = "poolcontroller";
    String username = DEFAULT_MQTT_USERNAME;
    String password = DEFAULT_MQTT_PASSWORD;
    String baseTopic = "pool";
    int keepAliveSec = 60;

    String toJson() const;
    static MqttConfig fromJson(JsonVariantConst json);
};

struct PumpConfig {
    int relayChannel = 0;
    int minOnTimeSec = 30;   // Taktschutz: minimale Einschaltdauer
    int minOffTimeSec = 120; // Taktschutz: minimale Ruhezeit
    float maxDailyRuntimeMin = 1440.0;  // 24h = de facto unlimited

    String toJson() const;
    static PumpConfig fromJson(JsonVariantConst json);
};

struct FilterPumpConfig {
    int relayChannel = 0;
    String windowStart = "07:00";
    String windowEnd = "21:00";
    int minCycleMinutes = 60;
    int maxCycleMinutes = 480;
    float maxDailyRuntimeMin = 1440.0;  // 24h = de facto unlimited
    int minOnTimeSec = 60;   // Taktschutz: minimale Einschaltdauer
    int minOffTimeSec = 300; // Taktschutz: minimale Ruhezeit (5 min)

    String toJson() const;
    static FilterPumpConfig fromJson(JsonVariantConst json);
};

struct RelayConfig {
    int channel = 0;
    String name = "";
    bool normallyOpen = true;
    int maxOnTimeSec = 0;

    String toJson() const;
    static RelayConfig fromJson(JsonVariantConst json);
};

struct AppConfig {
    int configVersion = CONFIG_VERSION;
    WifiConfig wifi;
    MqttConfig mqtt;
    PIDParams phPID;
    PIDParams chlorinePID;
    SensorConfig phSensor;
    SensorConfig orpSensor;
    SensorConfig tempAirSensor;
    SensorConfig tempWaterSensor;
    SensorConfig pressureSensor;
    PumpConfig phPump;
    PumpConfig chlorinePump;
    FilterPumpConfig filterPump;
    RelayConfig relays[MAX_RELAYS];
    int relayCount = 0;
    int logLevel = 1;
    int loopDelayMs = 100;

    String toJson() const;
    static AppConfig fromJson(JsonVariantConst json);
};

class ConfigManager {
public:
    ConfigManager();
    bool begin();
    AppConfig& get();
    bool save();
    bool updateFromJson(const String& json);
    String toJson();
    void print();

private:
    AppConfig _config;
    void setDefaults();
    bool loadFromLittleFS();
    bool saveToLittleFS();
};

#endif // CONFIG_MANAGER_H
