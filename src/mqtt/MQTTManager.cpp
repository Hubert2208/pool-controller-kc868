#include "MQTTManager.h"

MQTTManager* MQTTManager::_instance = nullptr;

#define MQTT_RECONNECT_BASE_DELAY 1000
#define MQTT_RECONNECT_MAX_DELAY 30000

MQTTManager::MQTTManager(ConfigManager& config)
    : _config(config)
    , _client(_wifiClient)
    , _lastReconnectAttempt(0)
    , _reconnectDelay(MQTT_RECONNECT_BASE_DELAY)
    , _discoveryPublished(false)
    , _commandCallback(nullptr)
{
    _instance = this;
}

void MQTTManager::begin() {
    AppConfig& cfg = _config.get();
    _baseTopic = cfg.mqtt.baseTopic;
    _lwtTopic = _baseTopic + "/status/LWT";
    _client.setServer(cfg.mqtt.broker.c_str(), cfg.mqtt.port);
    _client.setCallback(mqttCallback);
    _client.setBufferSize(1024);
    log_i("MQTT: broker=%s:%d, base=%s", cfg.mqtt.broker.c_str(), cfg.mqtt.port, _baseTopic.c_str());
}

void MQTTManager::loop() {
    if (!_client.connected()) {
        unsigned long now = millis();
        if (now - _lastReconnectAttempt >= (unsigned long)_reconnectDelay) {
            _lastReconnectAttempt = now;
            if (connect()) {
                _reconnectDelay = MQTT_RECONNECT_BASE_DELAY;
                _discoveryPublished = false;
            } else {
                _reconnectDelay = min((int)(_reconnectDelay * 1.5f), MQTT_RECONNECT_MAX_DELAY);
            }
        }
    } else {
        _client.loop();
    }
}

bool MQTTManager::connect() {
    AppConfig& cfg = _config.get();
    StaticJsonDocument<64> lwtDoc;
    lwtDoc["state"] = "offline";
    lwtDoc["client"] = cfg.mqtt.clientId.c_str();
    String lwtPayload; serializeJson(lwtDoc, lwtPayload);
    bool connected = false;
    if (cfg.mqtt.username.length() > 0)
        connected = _client.connect(cfg.mqtt.clientId.c_str(), cfg.mqtt.username.c_str(), cfg.mqtt.password.c_str(), _lwtTopic.c_str(), 1, true, lwtPayload.c_str());
    else
        connected = _client.connect(cfg.mqtt.clientId.c_str(), _lwtTopic.c_str(), 1, true, lwtPayload.c_str());
    if (connected) { log_i("MQTT connected"); publishOnline(); subscribeCommands(); publishDiscovery(); return true; }
    log_e("MQTT connect failed, rc=%d", _client.state());
    return false;
}

void MQTTManager::subscribeCommands() {
    String cmd = _baseTopic + "/command/#"; _client.subscribe(cmd.c_str(), 1);
    String cfg = _baseTopic + "/config/set"; _client.subscribe(cfg.c_str(), 1);
}

bool MQTTManager::publish(const char* topicSuffix, const String& payload, bool retained) {
    if (!_client.connected()) return false;
    String fullTopic = _baseTopic + "/" + String(topicSuffix);
    return _client.publish(fullTopic.c_str(), payload.c_str(), retained);
}

void MQTTManager::publishState(const String& sensorStates, const String& chemistryState,
                                const String& filterState, const String& pumpStates) {
    if (!_client.connected()) return;
    publish("sensors", sensorStates, false);
    publish("chemistry", chemistryState, false);
    publish("filter", filterState, false);
    publish("pumps", pumpStates, false);
}

void MQTTManager::publishOnline() {
    StaticJsonDocument<64> doc;
    doc["state"] = "online"; doc["client"] = _config.get().mqtt.clientId.c_str(); doc["version"] = "1.0.0";
    String payload; serializeJson(doc, payload);
    publish("status/LWT", payload, true);
}

void MQTTManager::publishOffline() {
    StaticJsonDocument<64> doc; doc["state"] = "offline";
    String payload; serializeJson(doc, payload);
    publish("status/LWT", payload, true);
}

void MQTTManager::publishDiscovery() {
    if (_discoveryPublished) return;
    AppConfig& cfg = _config.get();
    StaticJsonDocument<256> deviceDoc;
    deviceDoc["identifiers"][0] = cfg.mqtt.clientId.c_str();
    deviceDoc["name"] = "Pool Controller";
    deviceDoc["model"] = "KC868-A8";
    deviceDoc["manufacturer"] = "Kincony";
    deviceDoc["sw_version"] = "1.0.0";
    String deviceStr; serializeJson(deviceDoc, deviceStr);
    auto disco = [&](const char* type, const char* objId, const String& cfgJson) {
        String topic = "homeassistant/" + String(type) + "/" + cfg.mqtt.clientId + "/" + objId + "/config";
        _client.publish(topic.c_str(), cfgJson.c_str(), true);
    };
    disco("sensor","ph","{\"device_class\":\"pH\",\"name\":\"Pool pH\",\"unit_of_measurement\":\"pH\",\"state_topic\":\""+_baseTopic+"/ph_raw\",\"device\":"+deviceStr+",\"unique_id\":\""+cfg.mqtt.clientId+"_ph\"}");
    disco("sensor","orp","{\"device_class\":\"voltage\",\"name\":\"Pool ORP\",\"unit_of_measurement\":\"mV\",\"state_topic\":\""+_baseTopic+"/orp_raw\",\"device\":"+deviceStr+",\"unique_id\":\""+cfg.mqtt.clientId+"_orp\"}");
    disco("sensor","water_temp","{\"device_class\":\"temperature\",\"name\":\"Water Temp\",\"unit_of_measurement\":\"°C\",\"state_topic\":\""+_baseTopic+"/sensors\",\"value_template\":\"{{ value_json.water_temperature.value }}\",\"device\":"+deviceStr+",\"unique_id\":\""+cfg.mqtt.clientId+"_wt\"}");
    disco("sensor","air_temp","{\"device_class\":\"temperature\",\"name\":\"Air Temp\",\"unit_of_measurement\":\"°C\",\"state_topic\":\""+_baseTopic+"/sensors\",\"value_template\":\"{{ value_json.air_temperature.value }}\",\"device\":"+deviceStr+",\"unique_id\":\""+cfg.mqtt.clientId+"_at\"}");
    _discoveryPublished = true;
    log_i("HA discovery published");
}

void MQTTManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance) { String s; for (unsigned int i=0;i<length;i++) s+=(char)payload[i]; _instance->handleMQTTMessage(topic, s); }
}

void MQTTManager::handleMQTTMessage(char* topic, const String& payload) {
    log_i("MQTT message: %s -> %s", topic, payload.c_str());
    if (_commandCallback) _commandCallback(topic, payload);
}

void MQTTManager::setCommandCallback(void (*callback)(const char* topic, const String& payload)) {
    _commandCallback = callback;
}
