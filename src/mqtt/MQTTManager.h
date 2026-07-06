#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "../config/ConfigManager.h"

class MQTTManager {
public:
    MQTTManager(ConfigManager& config);
    void begin();
    void loop();
    bool isConnected() { return _client.connected(); }

    bool publish(const char* topicSuffix, const String& payload, bool retained = false);
    void publishState(const String& sensorStates, const String& chemistryState,
                      const String& filterState, const String& pumpStates);
    void publishOnline();
    void publishOffline();
    void publishDiscovery();

    void setCommandCallback(void (*callback)(const char* topic, const String& payload));

private:
    ConfigManager& _config;
    WiFiClient _wifiClient;
    PubSubClient _client;
    String _baseTopic;
    String _lwtTopic;
    unsigned long _lastReconnectAttempt;
    int _reconnectDelay;
    bool _discoveryPublished;
    void (*_commandCallback)(const char* topic, const String& payload);

    static MQTTManager* _instance;
    static void mqttCallback(char* topic, byte* payload, unsigned int length);
    void handleMQTTMessage(char* topic, const String& payload);
    bool connect();
    void subscribeCommands();
};

#endif
