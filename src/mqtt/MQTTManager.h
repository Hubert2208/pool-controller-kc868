#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include "../config/ConfigManager.h"

class MQTTManager {
public:
    MQTTManager(ConfigManager& config);
    ~MQTTManager() = default;

    void begin();
    void loop();
    bool publish(const char* topicSuffix, const String& payload, bool retained = false);
    void publishState(const String& sensorStates, const String& chemistryState,
                      const String& filterState, const String& pumpStates);
    void publishDiscovery();
    void publishOnline();
    void publishOffline();
    bool isConnected() { return _client.connected(); }
    void setCommandCallback(void (*callback)(const char* topic, const String& payload));

private:
    ConfigManager& _config;
    WiFiClient _wifiClient;
    PubSubClient _client;
    String _baseTopic;
    String _lwtTopic;

    unsigned long _lastReconnectAttempt;
    unsigned long _lastPublishTime;
    int _reconnectDelay;
    bool _discoveryPublished;

    bool connect();
    void subscribeCommands();
    static void mqttCallback(char* topic, byte* payload, unsigned int length);

    void publishSensorDiscovery(const char* deviceClass, const char* name,
                                 const char* unit, const char* topic,
                                 const char* icon);
    void publishBinarySensorDiscovery(const char* deviceClass, const char* name,
                                       const char* topic, const char* icon);
    void publishSwitchDiscovery(const char* name, const char* commandTopic,
                                 const char* stateTopic, const char* icon);

    static MQTTManager* _instance;
    void (*_commandCallback)(const char* topic, const String& payload);
    void handleMQTTMessage(char* topic, const String& payload);
};

#endif // MQTT_MANAGER_H
