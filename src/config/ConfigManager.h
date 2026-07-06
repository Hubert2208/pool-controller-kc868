#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#define CONFIG_FILE "/config.json"
#define CONFIG_JSON_SIZE 4096
#define MAX_RELAYS 8
#define CONFIG_VERSION 4  // added reverseAction to PIDParams
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
    bool reverseAction = false;  // true = reverse acting (output↑ when PV↑), e.g. pH

    String toJson() const;
    static PIDParams fromJson(JsonVariantConst json);
};