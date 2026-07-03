#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#define CONFIG_FILE "/config.json"
#define CONFIG_JSON_SIZE 4096
#define MAX_RELAYS 8
#define CONFIG_VERSION 3  // bump to force config regeneration on breaking changes
#define WIFI_HOSTNAME_MAX 64
#define MQTT_TOPIC_MAX 128
#define STRING_BUF_SIZE 256

struct PIDParams {
    float kp = 1.0;
    float ki = 0.1;
    float kd = 0.05;
    float setpoint = 7.2;
    float outputMin = 0.0;
    float outputMax = 100.0;
    int minOnTimeSec = 10;
    int minOffTimeSec = 60;

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
    String ssid = "Mayer2";
    String password = "Moritz26tOR";
    String hostname = "poolcontroller";
    bool fallbackAP = false;
    String apSSID = "PoolController-AP";
    String apPassword = "12345678";

    String toJson() const;
    static WifiConfig fromJson(JsonVariantConst json);
};

struct MqttConfig {
    String broker = "192.168.178.223";
    int port = 1883;
    String clientId = "poolcontroller";
    String username = "";
    String password = "";
    String baseTopic = "pool";
    int keepAliveSec = 60;

    String toJson() const;
    static MqttConfig fromJson(JsonVariantConst json);
};

struct PumpConfig {
    int relayChannel = 0;
    int minOnTimeSec = 30;
    int minOffTimeSec = 120;
    float maxDailyRuntimeMin = 1440.0;

    String toJson() const;
    static PumpConfig fromJson(JsonVariantConst json);
};

struct FilterPumpConfig {
    int relayChannel = 0;
    float tempSlope = 8.0;    // warmer water → more filtration (min/day per °C)
    float tempIntercept = -40.0; // baseline offset
    String windowStart = "07:00";
    String windowEnd = "21:00";
    int minCycleMinutes = 60;
    int maxCycleMinutes = 480;

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
    int relayCount = 8;
    int logLevel = 1;
    int loopDelayMs = 100;

    String toJson() const;
    static AppConfig fromJson(JsonVariantConst json);
};

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    bool begin();
    void save();
    AppConfig& get() { return _config; }
    void print();

private:
    AppConfig _config;
    bool _initialized;

    void loadDefaults();
    bool loadFromLittleFS();
    bool saveToLittleFS();
};

#endif // CONFIG_MANAGER_H
