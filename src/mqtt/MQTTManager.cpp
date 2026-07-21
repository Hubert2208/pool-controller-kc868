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
    log_i("MQTT: broker=%s:%d, base=%s, client=%s", cfg.mqtt.broker.c_str(), cfg.mqtt.port, _baseTopic.c_str(), cfg.mqtt.clientId.c_str());
}

void MQTTManager::loop() {
    if (!_client.connected()) {
        unsigned long now = millis();
        unsigned long delaySinceAttempt = now - _lastReconnectAttempt;
        if (delaySinceAttempt >= (unsigned long)_reconnectDelay) {
            _lastReconnectAttempt = now;
            if (connect()) { _reconnectDelay = MQTT_RECONNECT_BASE_DELAY; _discoveryPublished = false; }
            else { _reconnectDelay = min((int)(_reconnectDelay * 1.5f), MQTT_RECONNECT_MAX_DELAY); log_w("MQTT reconnect failed"); }
        }
    } else { _client.loop(); }
}

bool MQTTManager::connect() {
    AppConfig& cfg = _config.get();
    StaticJsonDocument<64> lwtDoc;
    lwtDoc["state"] = "offline"; lwtDoc["client"] = cfg.mqtt.clientId.c_str();
    String lwtPayload; serializeJson(lwtDoc, lwtPayload);
    bool connected = false;
    if (cfg.mqtt.username.length() > 0) {
        connected = _client.connect(cfg.mqtt.clientId.c_str(), cfg.mqtt.username.c_str(), cfg.mqtt.password.c_str(), _lwtTopic.c_str(), 1, true, lwtPayload.c_str());
    } else {
        connected = _client.connect(cfg.mqtt.clientId.c_str(), _lwtTopic.c_str(), 1, true, lwtPayload.c_str());
    }
    if (connected) { log_i("MQTT connected"); publishOnline(); subscribeCommands(); publishDiscovery(); return true; }
    log_e("MQTT connect failed, rc=%d", _client.state()); return false;
}

void MQTTManager::subscribeCommands() {
    String cmdTopic = _baseTopic + "/command/#";
    _client.subscribe(cmdTopic.c_str(), 1);
    _client.subscribe((_baseTopic + "/config/set").c_str(), 1);
}

bool MQTTManager::publish(const char* topicSuffix, const String& payload, bool retained) {
    if (!_client.connected()) return false;
    String fullTopic = _baseTopic + "/" + topicSuffix;
    return _client.publish(fullTopic.c_str(), payload.c_str(), retained);
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

    // Core sensors
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

    // Chemistry
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

    // Filter
    snprintf(tb,sizeof(tb),"%s",dt("sensor","filter_required").c_str());
    p = "{\"name\":\"Required Filter Runtime\",\"unit_of_measurement\":\"min\",\"state_topic\":\""+_baseTopic+"/filter\",\"value_template\":\"{{ value_json.required_runtime_min }}\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_frt\"}";
    _client.publish(tb,p.c_str(),true);

    // Pump runtime
    for (auto [suffix, name, uid] : {std::tuple{"ph_pump_runtime","pH Pump Runtime","_phrt"},{"cl_pump_runtime","Chlorine Pump Runtime","_clrt"},{"filter_pump_runtime","Filter Pump Runtime","_firt"}}) {
        snprintf(tb,sizeof(tb),"%s",dt("sensor",suffix).c_str());
        p = "{\"name\":\""+String(name)+"\",\"unit_of_measurement\":\"min\",\"state_topic\":\""+_baseTopic+"/pumps\",\"value_template\":\"{{ value_json."+String(suffix).substring(0,strlen(suffix)-8)+"_pump.runtime_today_min }}\",\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+uid+"\"}";
        _client.publish(tb,p.c_str(),true);
    }

    // Pump timing + daily limits + pre-run delay
    for (auto [s,f,u,lo,hi] : {std::tuple{"Min ON","minOn",1,3600},{"Min OFF","minOff",1,7200},{"Max Daily","maxDailyMin",1,1440}}) {
        for (auto [prefix,name,uid] : {std::tuple{"ph_","pH","_ph"},{"cl_","Chlorine","_cl"},{"filter_","Filter","_fi"}}) {
            snprintf(tb,sizeof(tb),"%s",dt("number",String(prefix)+"pump_"+String(f=="Min ON"?"min_on":f=="Min OFF"?"min_off":"max_day")).c_str());
            p = "{\"name\":\""+String(name)+" Pump "+String(s)+"\",\"unit_of_measurement\":\""+String(f=="maxDailyMin"?"min":"s")+"\",\"state_topic\":\""+_baseTopic+"/pump_config\",\"value_template\":\"{{ value_json."+String(prefix.substring(0,prefix.length()-1))+"."+String(f)+"}}\",\"command_topic\":\""+_baseTopic+"/command/"+String(prefix)+"pump_"+String(f=="minOn"?"min_on":f=="minOff"?"min_off":"max_day")+"\",\"min\":"+String(lo)+",\"max\":"+String(hi)+",\"step\":1,\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+uid+"\"}";
            _client.publish(tb,p.c_str(),true);
        }
    }

    // Filter pre-run delay
    snprintf(tb,sizeof(tb),"%s",dt("number","filter_prerun_delay").c_str());
    p = "{\"name\":\"Filter Pre-Run Delay\",\"unit_of_measurement\":\"min\",\"state_topic\":\""+_baseTopic+"/pump_config\",\"value_template\":\"{{ value_json.filter.preRunDelay }}\",\"command_topic\":\""+_baseTopic+"/command/filter_prerun_delay\",\"min\":1,\"max\":60,\"step\":1,\"device\":"+d+",\"unique_id\":\""+cfg.mqtt.clientId+"_fprd\"}";
    _client.publish(tb,p.c_str(),true);

    // ── Calibration sensors ──
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
