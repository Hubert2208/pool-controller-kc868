#include "MQTTManager.h"

MQTTManager* MQTTManager::_instance = nullptr;

#define MQTT_RECONNECT_BASE_DELAY 1000
#define MQTT_RECONNECT_MAX_DELAY 30000
#define MQTT_PUBLISH_INTERVAL_MS 30000

MQTTManager::MQTTManager(ConfigManager& config)
    : _config(config)
    , _client(_wifiClient)
    , _lastReconnectAttempt(0)
    , _lastPublishTime(0)
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

    log_i("MQTT: broker=%s:%d, base=%s, client=%s",
          cfg.mqtt.broker.c_str(), cfg.mqtt.port,
          _baseTopic.c_str(), cfg.mqtt.clientId.c_str());
}

void MQTTManager::loop() {
    if (!_client.connected()) {
        unsigned long now = millis();
        unsigned long delaySinceAttempt = now - _lastReconnectAttempt;

        if (delaySinceAttempt >= (unsigned long)_reconnectDelay) {
            _lastReconnectAttempt = now;
            if (connect()) {
                _reconnectDelay = MQTT_RECONNECT_BASE_DELAY;
                _discoveryPublished = false;
            } else {
                _reconnectDelay = min((int)(_reconnectDelay * 1.5f), MQTT_RECONNECT_MAX_DELAY);
                log_w("MQTT reconnect failed, next in %d ms", _reconnectDelay);
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
    String lwtPayload;
    serializeJson(lwtDoc, lwtPayload);

    bool connected = false;
    if (cfg.mqtt.username.length() > 0) {
        connected = _client.connect(
            cfg.mqtt.clientId.c_str(),
            cfg.mqtt.username.c_str(),
            cfg.mqtt.password.c_str(),
            _lwtTopic.c_str(),
            1,
            true,
            lwtPayload.c_str()
        );
    } else {
        connected = _client.connect(
            cfg.mqtt.clientId.c_str(),
            _lwtTopic.c_str(),
            1,
            true,
            lwtPayload.c_str()
        );
    }

    if (connected) {
        log_i("MQTT connected to %s:%d", cfg.mqtt.broker.c_str(), cfg.mqtt.port);
        publishOnline();
        subscribeCommands();
        publishDiscovery();
        return true;
    }

    log_e("MQTT connect failed, rc=%d", _client.state());
    return false;
}

void MQTTManager::subscribeCommands() {
    AppConfig& cfg = _config.get();

    String cmdTopic = _baseTopic + "/command/#";
    if (_client.subscribe(cmdTopic.c_str(), 1)) {
        log_i("MQTT subscribed: %s", cmdTopic.c_str());
    }

    String cfgTopic = _baseTopic + "/config/set";
    if (_client.subscribe(cfgTopic.c_str(), 1)) {
        log_i("MQTT subscribed: %s", cfgTopic.c_str());
    }
}

bool MQTTManager::publish(const char* topicSuffix, const String& payload, bool retained) {
    if (!_client.connected()) return false;

    String fullTopic = _baseTopic + "/" + topicSuffix;
    bool ok = _client.publish(fullTopic.c_str(), payload.c_str(), retained);
    if (!ok) {
        log_w("MQTT publish failed: %s", fullTopic.c_str());
    }
    return ok;
}

void MQTTManager::publishState(const String& sensorStates, const String& chemistryState,
                                const String& filterState, const String& pumpStates) {
    unsigned long now = millis();
    if (now - _lastPublishTime < MQTT_PUBLISH_INTERVAL_MS) return;
    _lastPublishTime = now;

    if (!_client.connected()) return;

    publish("sensors", sensorStates, false);
    publish("chemistry", chemistryState, false);
    publish("filter", filterState, false);
    publish("pumps", pumpStates, false);
}

void MQTTManager::publishOnline() {
    StaticJsonDocument<64> doc;
    doc["state"] = "online";
    doc["client"] = _config.get().mqtt.clientId.c_str();
    doc["version"] = "1.0.0";
    String payload;
    serializeJson(doc, payload);
    publish("status/LWT", payload, true);
}

void MQTTManager::publishOffline() {
    StaticJsonDocument<64> doc;
    doc["state"] = "offline";
    String payload;
    serializeJson(doc, payload);
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
    String deviceStr;
    serializeJson(deviceDoc, deviceStr);

    auto discoveryTopic = [&](const char* type, const char* objId) -> String {
        return "homeassistant/" + String(type) + "/" + cfg.mqtt.clientId + "/" + objId + "/config";
    };

    char topicBuf[256];
    String payload;

    // ── Sensor entities ──

    // Pool pH — dedicated raw-numeric topic (no JSON parsing)
    snprintf(topicBuf, sizeof(topicBuf), "%s", discoveryTopic("sensor", "ph").c_str());
    payload = "{\"device_class\":\"pH\",\"name\":\"Pool pH\",\"unit_of_measurement\":\"pH\","
              "\"state_topic\":\"" + _baseTopic + "/ph\","
              "\"device\":" + deviceStr + ",\"unique_id\":\"" + cfg.mqtt.clientId + "_ph\"}";
    _client.publish(topicBuf, payload.c_str(), true);

    // ── Composite JSON sensors on pool-controller/sensors ──
    // JSON format from SensorManager::getAllStateJSON():
    //   {"orp":{"value":400,...},"water_temperature":{"value":25,...},
    //    "air_temperature":{"value":22,...},"filter_pressure":{"value":1.2,"needs_backwash":false,...}}

    snprintf(topicBuf, sizeof(topicBuf), "%s", discoveryTopic("sensor", "orp").c_str());
    payload = "{\"device_class\":\"voltage\",\"name\":\"Pool ORP\",\"unit_of_measurement\":\"mV\","
              "\"state_topic\":\"" + _baseTopic + "/sensors\",\"value_template\":\"{{ value_json.orp.value }}\","
              "\"device\":" + deviceStr + ",\"unique_id\":\"" + cfg.mqtt.clientId + "_orp\"}";
    _client.publish(topicBuf, payload.c_str(), true);

    snprintf(topicBuf, sizeof(topicBuf), "%s", discoveryTopic("sensor", "water_temp").c_str());
    payload = "{\"device_class\":\"temperature\",\"name\":\"Pool Water Temperature\","
              "\"unit_of_measurement\":\"°C\","
              "\"state_topic\":\"" + _baseTopic + "/sensors\","
              "\"value_template\":\"{{ value_json.water_temperature.value }}\","
              "\"device\":" + deviceStr + ",\"unique_id\":\"" + cfg.mqtt.clientId + "_wt\"}";
    _client.publish(topicBuf, payload.c_str(), true);

    snprintf(topicBuf, sizeof(topicBuf), "%s", discoveryTopic("sensor", "air_temp").c_str());
    payload = "{\"device_class\":\"temperature\",\"name\":\"Air Temperature\",\"unit_of_measurement\":\"°C\","
              "\"state_topic\":\"" + _baseTopic + "/sensors\","
              "\"value_template\":\"{{ value_json.air_temperature.value }}\","
              "\"device\":" + deviceStr + ",\"unique_id\":\"" + cfg.mqtt.clientId + "_at\"}";
    _client.publish(topicBuf, payload.c_str(), true);

    // NOTE: No humidity sensor — SensorManager has no humidity getter.

    snprintf(topicBuf, sizeof(topicBuf), "%s", discoveryTopic("sensor", "filter_pressure").c_str());
    payload = "{\"device_class\":\"pressure\",\"name\":\"Filter Pressure\",\"unit_of_measurement\":\"bar\","
              "\"state_topic\":\"" + _baseTopic + "/sensors\","
              "\"value_template\":\"{{ value_json.filter_pressure.value }}\","
              "\"device\":" + deviceStr + ",\"unique_id\":\"" + cfg.mqtt.clientId + "_fp\"}";
    _client.publish(topicBuf, payload.c_str(), true);

    snprintf(topicBuf, sizeof(topicBuf), "%s", discoveryTopic("binary_sensor", "needs_backwash").c_str());
    payload = "{\"device_class\":\"problem\",\"name\":\"Needs Backwash\","
              "\"state_topic\":\"" + _baseTopic + "/sensors\","
              "\"value_template\":\"{{ value_json.filter_pressure.needs_backwash }}\","
              "\"device\":" + deviceStr + ",\"unique_id\":\"" + cfg.mqtt.clientId + "_bw\"}";
    _client.publish(topicBuf, payload.c_str(), true);

    // ── Chemistry entities on pool-controller/chemistry ──
    // JSON format from PoolChemistryController::getStateJSON():
    //   {"ph":{"enabled":true,"setpoint":7.2,"pid_output":45.5,...},
    //    "chlorine":{"enabled":true,"setpoint":650,"pid_output":60.0,...}}

    snprintf(topicBuf, sizeof(topicBuf), "%s", discoveryTopic("sensor", "ph_pid_output").c_str());
    payload = "{\"name\":\"pH PID Output\",\"unit_of_measurement\":\"%\","
              "\"state_topic\":\"" + _baseTopic + "/chemistry\","
              "\"value_template\":\"{{ value_json.ph.pid_output }}\","
              "\"device\":" + deviceStr + ",\"unique_id\":\"" + cfg.mqtt.clientId + "_phpo\"}";
    _client.publish(topicBuf, payload.c_str(), true);

    snprintf(topicBuf, sizeof(topicBuf), "%s", discoveryTopic("sensor", "chlorine_pid_output").c_str());
    payload = "{\"name\":\"Chlorine PID Output\",\"unit_of_measurement\":\"%\","
              "\"state_topic\":\"" + _baseTopic + "/chemistry\","
              "\"value_template\":\"{{ value_json.chlorine.pid_output }}\","
              "\"device\":" + deviceStr + ",\"unique_id\":\"" + cfg.mqtt.clientId + "_clpo\"}";
    _client.publish(topicBuf, payload.c_str(), true);

    snprintf(topicBuf, sizeof(topicBuf), "%s", discoveryTopic("switch", "ph_enable").c_str());
    payload = "{\"name\":\"pH Control\","
              "\"state_topic\":\"" + _baseTopic + "/chemistry\","
              "\"value_template\":\"{{ value_json.ph.enabled }}\","
              "\"command_topic\":\"" + _baseTopic + "/command/ph_set_enabled\","
              "\"payload_on\":\"true\",\"payload_off\":\"false\","
              "\"device\":" + deviceStr + ",\"unique_id\":\"" + cfg.mqtt.clientId + "_phen\"}";
    _client.publish(topicBuf, payload.c_str(), true);

    snprintf(topicBuf, sizeof(topicBuf), "%s", discoveryTopic("switch", "chlorine_enable").c_str());
    payload = "{\"name\":\"Chlorine Control\","
              "\"state_topic\":\"" + _baseTopic + "/chemistry\","
              "\"value_template\":\"{{ value_json.chlorine.enabled }}\","
              "\"command_topic\":\"" + _baseTopic + "/command/cl_set_enabled\","
              "\"payload_on\":\"true\",\"payload_off\":\"false\","
              "\"device\":" + deviceStr + ",\"unique_id\":\"" + cfg.mqtt.clientId + "_clen\"}";
    _client.publish(topicBuf, payload.c_str(), true);

    snprintf(topicBuf, sizeof(topicBuf), "%s", discoveryTopic("number", "ph_setpoint").c_str());
    payload = "{\"name\":\"pH Setpoint\",\"unit_of_measurement\":\"pH\","
              "\"state_topic\":\"" + _baseTopic + "/chemistry\","
              "\"value_template\":\"{{ value_json.ph.setpoint }}\","
              "\"command_topic\":\"" + _baseTopic + "/command/ph_setpoint\","
              "\"min\":6.0,\"max\":8.0,\"step\":0.1,"
              "\"device\":" + deviceStr + ",\"unique_id\":\"" + cfg.mqtt.clientId + "_phsp\"}";
    _client.publish(topicBuf, payload.c_str(), true);

    snprintf(topicBuf, sizeof(topicBuf), "%s", discoveryTopic("number", "orp_setpoint").c_str());
    payload = "{\"name\":\"ORP Setpoint\",\"unit_of_measurement\":\"mV\","
              "\"state_topic\":\"" + _baseTopic + "/chemistry\","
              "\"value_template\":\"{{ value_json.chlorine.setpoint }}\","
              "\"command_topic\":\"" + _baseTopic + "/command/orp_setpoint\","
              "\"min\":200,\"max\":900,\"step\":10,"
              "\"device\":" + deviceStr + ",\"unique_id\":\"" + cfg.mqtt.clientId + "_orsp\"}";
    _client.publish(topicBuf, payload.c_str(), true);

    // ── Filter entities on pool-controller/filter ──
    // JSON format from FilterPumpLogic::getStateJSON():
    //   {"enabled":true,"required_runtime_min":600,"pump_on":true,...}

    snprintf(topicBuf, sizeof(topicBuf), "%s", discoveryTopic("sensor", "filter_required").c_str());
    payload = "{\"name\":\"Required Filter Runtime\",\"unit_of_measurement\":\"min\","
              "\"state_topic\":\"" + _baseTopic + "/filter\","
              "\"value_template\":\"{{ value_json.required_runtime_min }}\","
              "\"device\":" + deviceStr + ",\"unique_id\":\"" + cfg.mqtt.clientId + "_frt\"}";
    _client.publish(topicBuf, payload.c_str(), true);

    // ── Pump entities on pool-controller/pumps ──
    // JSON format from loop() pumpDoc:
    //   {"ph_pump":{"name":"pH Pump","on":false,"runtime_today_min":12,...},
    //    "chlorine_pump":{...},"filter_pump":{...}}

    snprintf(topicBuf, sizeof(topicBuf), "%s", discoveryTopic("sensor", "ph_pump_runtime").c_str());
    payload = "{\"name\":\"pH Pump Runtime Today\",\"unit_of_measurement\":\"min\","
              "\"state_topic\":\"" + _baseTopic + "/pumps\","
              "\"value_template\":\"{{ value_json.ph_pump.runtime_today_min }}\","
              "\"device\":" + deviceStr + ",\"unique_id\":\"" + cfg.mqtt.clientId + "_phrt\"}";
    _client.publish(topicBuf, payload.c_str(), true);

    snprintf(topicBuf, sizeof(topicBuf), "%s", discoveryTopic("sensor", "chlorine_pump_runtime").c_str());
    payload = "{\"name\":\"Chlorine Pump Runtime Today\",\"unit_of_measurement\":\"min\","
              "\"state_topic\":\"" + _baseTopic + "/pumps\","
              "\"value_template\":\"{{ value_json.chlorine_pump.runtime_today_min }}\","
              "\"device\":" + deviceStr + ",\"unique_id\":\"" + cfg.mqtt.clientId + "_clrt\"}";
    _client.publish(topicBuf, payload.c_str(), true);

    snprintf(topicBuf, sizeof(topicBuf), "%s", discoveryTopic("sensor", "filter_pump_runtime").c_str());
    payload = "{\"name\":\"Filter Pump Runtime Today\",\"unit_of_measurement\":\"min\","
              "\"state_topic\":\"" + _baseTopic + "/pumps\","
              "\"value_template\":\"{{ value_json.filter_pump.runtime_today_min }}\","
              "\"device\":" + deviceStr + ",\"unique_id\":\"" + cfg.mqtt.clientId + "_firt\"}";
    _client.publish(topicBuf, payload.c_str(), true);

    _discoveryPublished = true;
    log_i("HA discovery published (16 entities)");
}

void MQTTManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance) {
        String payloadStr = "";
        for (unsigned int i = 0; i < length; i++) {
            payloadStr += (char)payload[i];
        }
        _instance->handleMQTTMessage(topic, payloadStr);
    }
}

void MQTTManager::handleMQTTMessage(char* topic, const String& payload) {
    String topicStr = String(topic);
    log_i("MQTT message: %s -> %s", topicStr.c_str(), payload.c_str());

    if (_commandCallback) {
        _commandCallback(topic, payload);
    }
}

void MQTTManager::setCommandCallback(void (*callback)(const char* topic, const String& payload)) {
    _commandCallback = callback;
}