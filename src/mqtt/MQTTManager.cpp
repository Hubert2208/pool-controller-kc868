#include "MQTTManager.h"

MQTTManager* MQTTManager::_instance = nullptr;

#define MQTT_RECONNECT_BASE_DELAY 1000
#define MQTT_RECONNECT_MAX_DELAY 30000

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
    log_i("MQTT: broker=%s:%d, base=%s", cfg.mqtt.broker.c_str(), cfg.mqtt.port, _baseTopic.c_str());
}

void MQTTManager::loop() {
    if (!_client.connected()) {
        unsigned long now = millis();
        if (now - _lastReconnectAttempt >= (unsigned long)_reconnectDelay) {
            _lastReconnectAttempt = now;
            if (connect()) { _reconnectDelay = MQTT_RECONNECT_BASE_DELAY; _discoveryPublished = false; }
            else { _reconnectDelay = min((int)(_reconnectDelay * 1.5f), MQTT_RECONNECT_MAX_DELAY); }
        }
    } else { _client.loop(); }
}

bool MQTTManager::connect() {
    AppConfig& cfg = _config.get();
    StaticJsonDocument<64> lwtDoc;
    lwtDoc["state"] = "offline"; lwtDoc["client"] = cfg.mqtt.clientId.c_str();
    String lwtPayload; serializeJson(lwtDoc, lwtPayload);
    bool ok = false;
    if (cfg.mqtt.username.length() > 0)
        ok = _client.connect(cfg.mqtt.clientId.c_str(), cfg.mqtt.username.c_str(), cfg.mqtt.password.c_str(), _lwtTopic.c_str(), 1, true, lwtPayload.c_str());
    else
        ok = _client.connect(cfg.mqtt.clientId.c_str(), _lwtTopic.c_str(), 1, true, lwtPayload.c_str());
    if (ok) { log_i("MQTT connected"); publishOnline(); subscribeCommands(); publishDiscovery(); return true; }
    return false;
}

void MQTTManager::subscribeCommands() {
    _client.subscribe((_baseTopic + "/command/#").c_str(), 1);
    _client.subscribe((_baseTopic + "/config/set").c_str(), 1);
}

bool MQTTManager::publish(const char* topicSuffix, const String& payload, bool retained) {
    if (!_client.connected()) return false;
    return _client.publish((_baseTopic + "/" + topicSuffix).c_str(), payload.c_str(), retained);
}

void MQTTManager::publishOnline() {
    StaticJsonDocument<64> doc;
    doc["state"] = "online"; doc["client"] = _config.get().mqtt.clientId.c_str(); doc["version"] = "1.1.0";
    String payload; serializeJson(doc, payload);
    publish("status/LWT", payload, true);
}

void MQTTManager::publishDiscovery() {
    if (_discoveryPublished) return;
    AppConfig& cfg = _config.get();
    StaticJsonDocument<256> dev;
    dev["identifiers"][0] = cfg.mqtt.clientId.c_str();
    dev["name"] = "Pool Controller"; dev["model"] = "KC868-A8"; dev["manufacturer"] = "Kincony"; dev["sw_version"] = "1.1.0";
    String d; serializeJson(dev, d);
    auto dt = [&](const char* t, const char* id) -> String {
        return "homeassistant/" + String(t) + "/" + cfg.mqtt.clientId + "/" + id + "/config";
    };
    char tb[256]; String p;

    // Core sensors (5)
    snprintf(tb,sizeof(tb),"%s",dt("sensor","ph").c_str());
    p = "{\"device_class\":\"pH\",\"name\":\"Pool pH\",\"state_class\":\"measurement\",\"state_topic\":\""+_baseTopic+"/ph\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_ph\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("sensor","orp").c_str());
    p = "{\"device_class\":\"voltage\",\"name\":\"Pool ORP\",\"unit_of_measurement\":\"mV\",\"state_topic\":\""+_baseTopic+"/sensors\",\"value_template\":\"{{ value_json.orp.value }}\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_orp\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("sensor","water_temp").c_str());
    p = "{\"device_class\":\"temperature\",\"name\":\"Pool Water Temperature\",\"unit_of_measurement\":\"°C\",\"state_topic\":\""+_baseTopic+"/sensors\",\"value_template\":\"{{ value_json.water_temperature.value }}\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_wt\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("sensor","air_temp").c_str());
    p = "{\"device_class\":\"temperature\",\"name\":\"Air Temperature\",\"unit_of_measurement\":\"°C\",\"state_topic\":\""+_baseTopic+"/sensors\",\"value_template\":\"{{ value_json.air_temperature.value }}\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_at\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("sensor","filter_pressure").c_str());
    p = "{\"device_class\":\"pressure\",\"name\":\"Filter Pressure\",\"unit_of_measurement\":\"bar\",\"state_topic\":\""+_baseTopic+"/sensors\",\"value_template\":\"{{ value_json.filter_pressure.value }}\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_fp\"}";
    _client.publish(tb,p.c_str(),true);

    // Chemistry (4)
    snprintf(tb,sizeof(tb),"%s",dt("sensor","ph_pid_output").c_str());
    p = "{\"name\":\"pH PID Output\",\"unit_of_measurement\":\"%\",\"state_topic\":\""+_baseTopic+"/chemistry\",\"value_template\":\"{{ value_json.ph.pid_output }}\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_phpo\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("sensor","cl_pid_output").c_str());
    p = "{\"name\":\"Chlorine PID Output\",\"unit_of_measurement\":\"%\",\"state_topic\":\""+_baseTopic+"/chemistry\",\"value_template\":\"{{ value_json.chlorine.pid_output }}\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_clpo\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("switch","ph_enable").c_str());
    p = "{\"name\":\"pH Control\",\"state_topic\":\""+_baseTopic+"/chemistry\",\"value_template\":\"{{ value_json.ph.enabled }}\",\"command_topic\":\""+_baseTopic+"/command/ph_set_enabled\",\"payload_on\":\"true\",\"payload_off\":\"false\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_phen\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("switch","cl_enable").c_str());
    p = "{\"name\":\"Chlorine Control\",\"state_topic\":\""+_baseTopic+"/chemistry\",\"value_template\":\"{{ value_json.chlorine.enabled }}\",\"command_topic\":\""+_baseTopic+"/command/cl_set_enabled\",\"payload_on\":\"true\",\"payload_off\":\"false\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_clen\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("number","ph_setpoint").c_str());
    p = "{\"name\":\"pH Setpoint\",\"unit_of_measurement\":\"pH\",\"state_topic\":\""+_baseTopic+"/chemistry\",\"value_template\":\"{{ value_json.ph.setpoint }}\",\"command_topic\":\""+_baseTopic+"/command/ph_setpoint\",\"min\":6.0,\"max\":8.0,\"step\":0.1,\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_phsp\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("number","orp_setpoint").c_str());
    p = "{\"name\":\"ORP Setpoint\",\"unit_of_measurement\":\"mV\",\"state_topic\":\""+_baseTopic+"/chemistry\",\"value_template\":\"{{ value_json.chlorine.setpoint }}\",\"command_topic\":\""+_baseTopic+"/command/orp_setpoint\",\"min\":200,\"max\":900,\"step\":10,\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_orsp\"}";
    _client.publish(tb,p.c_str(),true);

    // Filter (1)
    snprintf(tb,sizeof(tb),"%s",dt("sensor","filter_required").c_str());
    p = "{\"name\":\"Required Filter Runtime\",\"unit_of_measurement\":\"min\",\"state_topic\":\""+_baseTopic+"/filter\",\"value_template\":\"{{ value_json.required_runtime_min }}\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_frt\"}";
    _client.publish(tb,p.c_str(),true);

    // Pump runtime (3)
    snprintf(tb,sizeof(tb),"%s",dt("sensor","ph_pump_runtime").c_str());
    p = "{\"name\":\"pH Pump Runtime Today\",\"unit_of_measurement\":\"min\",\"state_topic\":\""+_baseTopic+"/pumps\",\"value_template\":\"{{ value_json.ph_pump.runtime_today_min }}\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_phrt\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("sensor","cl_pump_runtime").c_str());
    p = "{\"name\":\"Chlorine Pump Runtime Today\",\"unit_of_measurement\":\"min\",\"state_topic\":\""+_baseTopic+"/pumps\",\"value_template\":\"{{ value_json.chlorine_pump.runtime_today_min }}\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_clrt\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("sensor","filter_pump_runtime").c_str());
    p = "{\"name\":\"Filter Pump Runtime Today\",\"unit_of_measurement\":\"min\",\"state_topic\":\""+_baseTopic+"/pumps\",\"value_template\":\"{{ value_json.filter_pump.runtime_today_min }}\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_firt\"}";
    _client.publish(tb,p.c_str(),true);

    // Pump timing: min on/off (6)
    snprintf(tb,sizeof(tb),"%s",dt("number","ph_pump_min_on").c_str());
    p = "{\"name\":\"pH Pump Min ON\",\"unit_of_measurement\":\"s\",\"state_topic\":\""+_baseTopic+"/pump_config\",\"value_template\":\"{{ value_json.ph.minOn }}\",\"command_topic\":\""+_baseTopic+"/command/ph_pump_min_on\",\"min\":1,\"max\":3600,\"step\":1,\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_phmno\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("number","ph_pump_min_off").c_str());
    p = "{\"name\":\"pH Pump Min OFF\",\"unit_of_measurement\":\"s\",\"state_topic\":\""+_baseTopic+"/pump_config\",\"value_template\":\"{{ value_json.ph.minOff }}\",\"command_topic\":\""+_baseTopic+"/command/ph_pump_min_off\",\"min\":1,\"max\":7200,\"step\":1,\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_phmfo\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("number","cl_pump_min_on").c_str());
    p = "{\"name\":\"Chlorine Pump Min ON\",\"unit_of_measurement\":\"s\",\"state_topic\":\""+_baseTopic+"/pump_config\",\"value_template\":\"{{ value_json.chlorine.minOn }}\",\"command_topic\":\""+_baseTopic+"/command/cl_pump_min_on\",\"min\":1,\"max\":3600,\"step\":1,\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_clmno\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("number","cl_pump_min_off").c_str());
    p = "{\"name\":\"Chlorine Pump Min OFF\",\"unit_of_measurement\":\"s\",\"state_topic\":\""+_baseTopic+"/pump_config\",\"value_template\":\"{{ value_json.chlorine.minOff }}\",\"command_topic\":\""+_baseTopic+"/command/cl_pump_min_off\",\"min\":1,\"max\":7200,\"step\":1,\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_clmfo\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("number","filter_pump_min_on").c_str());
    p = "{\"name\":\"Filter Pump Min ON\",\"unit_of_measurement\":\"s\",\"state_topic\":\""+_baseTopic+"/pump_config\",\"value_template\":\"{{ value_json.filter.minOn }}\",\"command_topic\":\""+_baseTopic+"/command/filter_pump_min_on\",\"min\":1,\"max\":3600,\"step\":1,\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_fimno\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("number","filter_pump_min_off").c_str());
    p = "{\"name\":\"Filter Pump Min OFF\",\"unit_of_measurement\":\"s\",\"state_topic\":\""+_baseTopic+"/pump_config\",\"value_template\":\"{{ value_json.filter.minOff }}\",\"command_topic\":\""+_baseTopic+"/command/filter_pump_min_off\",\"min\":1,\"max\":7200,\"step\":1,\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_fimfo\"}";
    _client.publish(tb,p.c_str(),true);

    // Daily limits (3)
    snprintf(tb,sizeof(tb),"%s",dt("number","ph_pump_max_day").c_str());
    p = "{\"name\":\"pH Pump Max Daily\",\"unit_of_measurement\":\"min\",\"state_topic\":\""+_baseTopic+"/pump_config\",\"value_template\":\"{{ value_json.ph.maxDailyMin }}\",\"command_topic\":\""+_baseTopic+"/command/ph_pump_max_day\",\"min\":1,\"max\":1440,\"step\":1,\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_phmdr\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("number","cl_pump_max_day").c_str());
    p = "{\"name\":\"Chlorine Pump Max Daily\",\"unit_of_measurement\":\"min\",\"state_topic\":\""+_baseTopic+"/pump_config\",\"value_template\":\"{{ value_json.chlorine.maxDailyMin }}\",\"command_topic\":\""+_baseTopic+"/command/cl_pump_max_day\",\"min\":1,\"max\":1440,\"step\":1,\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_clmdr\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("number","filter_pump_max_day").c_str());
    p = "{\"name\":\"Filter Pump Max Daily\",\"unit_of_measurement\":\"min\",\"state_topic\":\""+_baseTopic+"/pump_config\",\"value_template\":\"{{ value_json.filter.maxDailyMin }}\",\"command_topic\":\""+_baseTopic+"/command/filter_pump_max_day\",\"min\":1,\"max\":1440,\"step\":1,\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_fimdr\"}";
    _client.publish(tb,p.c_str(),true);

    // Filter pre-run delay (1)
    snprintf(tb,sizeof(tb),"%s",dt("number","filter_prerun_delay").c_str());
    p = "{\"name\":\"Filter Pre-Run Delay\",\"unit_of_measurement\":\"min\",\"state_topic\":\""+_baseTopic+"/pump_config\",\"value_template\":\"{{ value_json.filter.preRunDelay }}\",\"command_topic\":\""+_baseTopic+"/command/filter_prerun_delay\",\"min\":1,\"max\":60,\"step\":1,\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_fprd\"}";
    _client.publish(tb,p.c_str(),true);

    // Calibration age sensors (2)
    snprintf(tb,sizeof(tb),"%s",dt("sensor","ph_calibration_days").c_str());
    p = "{\"name\":\"pH Calibration Age\",\"unit_of_measurement\":\"days\",\"state_class\":\"measurement\",\"state_topic\":\""+_baseTopic+"/calibration\",\"value_template\":\"{{ ((as_timestamp(now()) - value_json.ph.calibratedAt) / 86400) | round(0) if value_json.ph.calibratedAt is defined else -1 }}\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_phcd\"}";
    _client.publish(tb,p.c_str(),true);

    snprintf(tb,sizeof(tb),"%s",dt("sensor","orp_calibration_days").c_str());
    p = "{\"name\":\"ORP Calibration Age\",\"unit_of_measurement\":\"days\",\"state_class\":\"measurement\",\"state_topic\":\""+_baseTopic+"/calibration\",\"value_template\":\"{{ ((as_timestamp(now()) - value_json.orp.calibratedAt) / 86400) | round(0) if value_json.orp.calibratedAt is defined else -1 }}\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_orcd\"}";
    _client.publish(tb,p.c_str(),true);

    _discoveryPublished = true;
    log_i("HA discovery published (27 entities)");
}

void MQTTManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance) {
        String s = ""; for (unsigned int i = 0; i < length; i++) s += (char)payload[i];
        _instance->handleMQTTMessage(topic, s);
    }
}

void MQTTManager::handleMQTTMessage(char* topic, const String& payload) {
    if (_commandCallback) _commandCallback(topic, payload);
}

void MQTTManager::setCommandCallback(void (*cb)(const char*,const String&)) { _commandCallback = cb; }
