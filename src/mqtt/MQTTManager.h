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

    // Initialize MQTT client
    void begin();

    // Main loop — call every loop(), handles reconnection
    void loop();

    // Publish a sensor value to the status topic
    bool publish(const char* topicSuffix, const String& payload, bool retained = false);

    // Publish state for all subsystems
    void publishState(const String& sensorStates, const String& chemistryState,
                      const String& filterState, const String& pumpStates);

    // Publish Home Assistant auto-discovery config
    void publishDiscovery();

    // Publish will/online message
    void publishOnline();
    void publishOffline();

    // Check if connected
    bool isConnected() const { return _client.connected(); }

    // Set command callback
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

    // Internal methods
    bool connect();
    void subscribeCommands();
    static void mqttCallback(char* topic, byte* payload, unsigned int length);

    // Discovery helpers
    void publishSensorDiscovery(const char* deviceClass, const char* name,
                                 const char* unit, const char* topic,
                                 const char* icon);
    void publishBinarySensorDiscovery(const char* deviceClass, const char* name,
                                       const char* topic, const char* icon);
    void publishSwitchDiscovery(const char* name, const char* commandTopic,
                                 const char* stateTopic, const char* icon);

    // Command handler reference
    static MQTTManager* _instance;
    void (*_commandCallback)(const char* topic, const String& payload);
    void handleMQTTMessage(char* topic, byte* payload, unsigned int length);
};

#endif // MQTT_MANAGER_H