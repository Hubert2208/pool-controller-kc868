#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

#define CONFIG_FILE "/config.json"
#define CONFIG_JSON_SIZE 4096
#define MAX_RELAYS 8
#define WIFI_HOSTNAME_MAX 64
#define MQTT_TOPIC_MAX 128
#define STRING_BUF_SIZE 256

struct PIDParams {
    float kp = 1.0;
    float ki = 0.1;
    float kd = 0.05;
    float setpoint = 7.2;       // pH setpoint
    float outputMin = 0.0;
    float outputMax = 100.0;
    int minOnTimeSec = 10;
    int minOffTimeSec = 60;

    String toJson() const;
    static PIDParams fromJson(const JsonVariant& json);
};

struct SensorConfig {
    bool enabled = true;
    bool simulate = false;
    int updateIntervalMs = 2000;
    float simMin = 0.0;
    float simMax = 100.0;
    float simDriftPerHour = 0.5;

    String toJson() const;
    static SensorConfig fromJson(const JsonVariant& json);
};

struct WifiConfig {
    String ssid = "";
    String password = "";
    String hostname = "pool-controller";
    bool fallbackAP = true;
    String apSSID = "PoolController-AP";
    String apPassword = "12345678";

    String toJson() const;
    static WifiConfig fromJson(const JsonVariant& json);
};

struct MqttConfig {
    String broker = "";
    int port = 1883;
    String clientId = "pool-controller";
    String username = "";
    String password = "";
    String baseTopic = "pool";
    int keepAliveSec = 60;

    String toJson() const;
    static MqttConfig fromJson(const JsonVariant& json);
};

struct PumpConfig {
    int relayChannel = 0;
    int minOnTimeSec = 30;
    int minOffTimeSec = 120;
    float maxDailyRuntimeMin = 1440.0;

    String toJson() const;
    static PumpConfig fromJson(const JsonVariant& json);
};

struct FilterPumpConfig {
    int relayChannel = 0;
    float tempSlope = -7.5;           // minutes per °C (higher temp = less runtime)
    float tempIntercept = 300.0;      // base minutes at 0°C
    String windowStart = "07:00";
    String windowEnd = "21:00";
    int minCycleMinutes = 60;
    int maxCycleMinutes = 480;

    String toJson() const;
    static FilterPumpConfig fromJson(const JsonVariant& json);
};

struct RelayConfig {
    int channel = 0;
    String name = "";
    bool normallyOpen = true;
    int maxOnTimeSec = 0;             // 0 = unlimited

    String toJson() const;
    static RelayConfig fromJson(const JsonVariant& json);
};

struct AppConfig {
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
    static AppConfig fromJson(const JsonDocument& doc);
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
    bool loadFromSPIFFS();
    bool saveToSPIFFS();
    bool createDefaultConfig();
};

#endif // CONFIG_MANAGER_H
