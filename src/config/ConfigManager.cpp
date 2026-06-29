#include "ConfigManager.h"

// ─── Serialization helpers ────────────────────────────────────────────

String PIDParams::toJson() const {
    StaticJsonDocument<256> doc;
    doc["kp"] = kp;
    doc["ki"] = ki;
    doc["kd"] = kd;
    doc["setpoint"] = setpoint;
    doc["outputMin"] = outputMin;
    doc["outputMax"] = outputMax;
    doc["minOnTimeSec"] = minOnTimeSec;
    doc["minOffTimeSec"] = minOffTimeSec;
    String out;
    serializeJson(doc, out);
    return out;
}

PIDParams PIDParams::fromJson(JsonVariantConst json) {
    PIDParams p;
    if (json.is<JsonObjectConst>()) {
        p.kp = json["kp"] | p.kp;
        p.ki = json["ki"] | p.ki;
        p.kd = json["kd"] | p.kd;
        p.setpoint = json["setpoint"] | p.setpoint;
        p.outputMin = json["outputMin"] | p.outputMin;
        p.outputMax = json["outputMax"] | p.outputMax;
        p.minOnTimeSec = json["minOnTimeSec"] | p.minOnTimeSec;
        p.minOffTimeSec = json["minOffTimeSec"] | p.minOffTimeSec;
    }
    return p;
}

String SensorConfig::toJson() const {
    StaticJsonDocument<256> doc;
    doc["enabled"] = enabled;
    doc["simulate"] = simulate;
    doc["updateIntervalMs"] = updateIntervalMs;
    doc["simMin"] = simMin;
    doc["simMax"] = simMax;
    doc["simDriftPerHour"] = simDriftPerHour;
    String out;
    serializeJson(doc, out);
    return out;
}

SensorConfig SensorConfig::fromJson(JsonVariantConst json) {
    SensorConfig s;
    if (json.is<JsonObjectConst>()) {
        s.enabled = json["enabled"] | s.enabled;
        s.simulate = json["simulate"] | s.simulate;
        s.updateIntervalMs = json["updateIntervalMs"] | s.updateIntervalMs;
        s.simMin = json["simMin"] | s.simMin;
        s.simMax = json["simMax"] | s.simMax;
        s.simDriftPerHour = json["simDriftPerHour"] | s.simDriftPerHour;
    }
    return s;
}

String WifiConfig::toJson() const {
    StaticJsonDocument<256> doc;
    doc["ssid"] = ssid;
    doc["password"] = "****";       // mask in output
    doc["hostname"] = hostname;
    doc["fallbackAP"] = fallbackAP;
    doc["apSSID"] = apSSID;
    doc["apPassword"] = "****";
    String out;
    serializeJson(doc, out);
    return out;
}

WifiConfig WifiConfig::fromJson(JsonVariantConst json) {
    WifiConfig w;
    if (json.is<JsonObjectConst>()) {
        if (json["ssid"].is<const char*>()) w.ssid = json["ssid"].as<const char*>();
        if (json["password"].is<const char*>()) w.password = json["password"].as<const char*>();
        if (json["hostname"].is<const char*>()) w.hostname = json["hostname"].as<const char*>();
        w.fallbackAP = json["fallbackAP"] | w.fallbackAP;
        if (json["apSSID"].is<const char*>()) w.apSSID = json["apSSID"].as<const char*>();
        if (json["apPassword"].is<const char*>()) w.apPassword = json["apPassword"].as<const char*>();
    }
    return w;
}

String MqttConfig::toJson() const {
    StaticJsonDocument<256> doc;
    doc["broker"] = broker;
    doc["port"] = port;
    doc["clientId"] = clientId;
    doc["username"] = username;
    doc["password"] = "****";
    doc["baseTopic"] = baseTopic;
    doc["keepAliveSec"] = keepAliveSec;
    String out;
    serializeJson(doc, out);
    return out;
}

MqttConfig MqttConfig::fromJson(JsonVariantConst json) {
    MqttConfig m;
    if (json.is<JsonObjectConst>()) {
        if (json["broker"].is<const char*>()) m.broker = json["broker"].as<const char*>();
        m.port = json["port"] | m.port;
        if (json["clientId"].is<const char*>()) m.clientId = json["clientId"].as<const char*>();
        if (json["username"].is<const char*>()) m.username = json["username"].as<const char*>();
        if (json["password"].is<const char*>()) m.password = json["password"].as<const char*>();
        if (json["baseTopic"].is<const char*>()) m.baseTopic = json["baseTopic"].as<const char*>();
        m.keepAliveSec = json["keepAliveSec"] | m.keepAliveSec;
    }
    return m;
}

String PumpConfig::toJson() const {
    StaticJsonDocument<256> doc;
    doc["relayChannel"] = relayChannel;
    doc["minOnTimeSec"] = minOnTimeSec;
    doc["minOffTimeSec"] = minOffTimeSec;
    doc["maxDailyRuntimeMin"] = maxDailyRuntimeMin;
    String out;
    serializeJson(doc, out);
    return out;
}

PumpConfig PumpConfig::fromJson(JsonVariantConst json) {
    PumpConfig p;
    if (json.is<JsonObjectConst>()) {
        p.relayChannel = json["relayChannel"] | p.relayChannel;
        p.minOnTimeSec = json["minOnTimeSec"] | p.minOnTimeSec;
        p.minOffTimeSec = json["minOffTimeSec"] | p.minOffTimeSec;
        p.maxDailyRuntimeMin = json["maxDailyRuntimeMin"] | p.maxDailyRuntimeMin;
    }
    return p;
}

String FilterPumpConfig::toJson() const {
    StaticJsonDocument<256> doc;
    doc["relayChannel"] = relayChannel;
    doc["tempSlope"] = tempSlope;
    doc["tempIntercept"] = tempIntercept;
    doc["windowStart"] = windowStart;
    doc["windowEnd"] = windowEnd;
    doc["minCycleMinutes"] = minCycleMinutes;
    doc["maxCycleMinutes"] = maxCycleMinutes;
    String out;
    serializeJson(doc, out);
    return out;
}

FilterPumpConfig FilterPumpConfig::fromJson(JsonVariantConst json) {
    FilterPumpConfig f;
    if (json.is<JsonObjectConst>()) {
        f.relayChannel = json["relayChannel"] | f.relayChannel;
        f.tempSlope = json["tempSlope"] | f.tempSlope;
        f.tempIntercept = json["tempIntercept"] | f.tempIntercept;
        if (json["windowStart"].is<const char*>()) f.windowStart = json["windowStart"].as<const char*>();
        if (json["windowEnd"].is<const char*>()) f.windowEnd = json["windowEnd"].as<const char*>();
        f.minCycleMinutes = json["minCycleMinutes"] | f.minCycleMinutes;
        f.maxCycleMinutes = json["maxCycleMinutes"] | f.maxCycleMinutes;
    }
    return f;
}

String RelayConfig::toJson() const {
    StaticJsonDocument<256> doc;
    doc["channel"] = channel;
    doc["name"] = name;
    doc["normallyOpen"] = normallyOpen;
    doc["maxOnTimeSec"] = maxOnTimeSec;
    String out;
    serializeJson(doc, out);
    return out;
}

RelayConfig RelayConfig::fromJson(JsonVariantConst json) {
    RelayConfig r;
    if (json.is<JsonObjectConst>()) {
        r.channel = json["channel"] | r.channel;
        if (json["name"].is<const char*>()) r.name = json["name"].as<const char*>();
        r.normallyOpen = json["normallyOpen"] | r.normallyOpen;
        r.maxOnTimeSec = json["maxOnTimeSec"] | r.maxOnTimeSec;
    }
    return r;
}

// ─── AppConfig ────────────────────────────────────────────────────────

String AppConfig::toJson() const {
    StaticJsonDocument<CONFIG_JSON_SIZE> doc;
    JsonObject wifiObj = doc.createNestedObject("wifi");
    wifiObj["ssid"] = wifi.ssid;
    wifiObj["password"] = wifi.password;
    wifiObj["hostname"] = wifi.hostname;
    wifiObj["fallbackAP"] = wifi.fallbackAP;
    wifiObj["apSSID"] = wifi.apSSID;
    wifiObj["apPassword"] = wifi.apPassword;

    JsonObject mqttObj = doc.createNestedObject("mqtt");
    mqttObj["broker"] = mqtt.broker;
    mqttObj["port"] = mqtt.port;
    mqttObj["clientId"] = mqtt.clientId;
    mqttObj["username"] = mqtt.username;
    mqttObj["password"] = mqtt.password;
    mqttObj["baseTopic"] = mqtt.baseTopic;
    mqttObj["keepAliveSec"] = mqtt.keepAliveSec;

    JsonObject phPidObj = doc.createNestedObject("phPID");
    phPidObj["kp"] = phPID.kp;
    phPidObj["ki"] = phPID.ki;
    phPidObj["kd"] = phPID.kd;
    phPidObj["setpoint"] = phPID.setpoint;
    phPidObj["outputMin"] = phPID.outputMin;
    phPidObj["outputMax"] = phPID.outputMax;
    phPidObj["minOnTimeSec"] = phPID.minOnTimeSec;
    phPidObj["minOffTimeSec"] = phPID.minOffTimeSec;

    JsonObject clPidObj = doc.createNestedObject("chlorinePID");
    clPidObj["kp"] = chlorinePID.kp;
    clPidObj["ki"] = chlorinePID.ki;
    clPidObj["kd"] = chlorinePID.kd;
    clPidObj["setpoint"] = chlorinePID.setpoint;
    clPidObj["outputMin"] = chlorinePID.outputMin;
    clPidObj["outputMax"] = chlorinePID.outputMax;
    clPidObj["minOnTimeSec"] = chlorinePID.minOnTimeSec;
    clPidObj["minOffTimeSec"] = chlorinePID.minOffTimeSec;

    JsonObject phSens = doc.createNestedObject("phSensor");
    phSens["enabled"] = phSensor.enabled;
    phSens["simulate"] = phSensor.simulate;
    phSens["updateIntervalMs"] = phSensor.updateIntervalMs;
    phSens["simMin"] = phSensor.simMin;
    phSens["simMax"] = phSensor.simMax;
    phSens["simDriftPerHour"] = phSensor.simDriftPerHour;

    JsonObject orpSens = doc.createNestedObject("orpSensor");
    orpSens["enabled"] = orpSensor.enabled;
    orpSens["simulate"] = orpSensor.simulate;
    orpSens["updateIntervalMs"] = orpSensor.updateIntervalMs;
    orpSens["simMin"] = orpSensor.simMin;
    orpSens["simMax"] = orpSensor.simMax;
    orpSens["simDriftPerHour"] = orpSensor.simDriftPerHour;

    JsonObject taSens = doc.createNestedObject("tempAirSensor");
    taSens["enabled"] = tempAirSensor.enabled;
    taSens["simulate"] = tempAirSensor.simulate;
    taSens["updateIntervalMs"] = tempAirSensor.updateIntervalMs;
    taSens["simMin"] = tempAirSensor.simMin;
    taSens["simMax"] = tempAirSensor.simMax;
    taSens["simDriftPerHour"] = tempAirSensor.simDriftPerHour;

    JsonObject twSens = doc.createNestedObject("tempWaterSensor");
    twSens["enabled"] = tempWaterSensor.enabled;
    twSens["simulate"] = tempWaterSensor.simulate;
    twSens["updateIntervalMs"] = tempWaterSensor.updateIntervalMs;
    twSens["simMin"] = tempWaterSensor.simMin;
    twSens["simMax"] = tempWaterSensor.simMax;
    twSens["simDriftPerHour"] = tempWaterSensor.simDriftPerHour;

    JsonObject presSens = doc.createNestedObject("pressureSensor");
    presSens["enabled"] = pressureSensor.enabled;
    presSens["simulate"] = pressureSensor.simulate;
    presSens["updateIntervalMs"] = pressureSensor.updateIntervalMs;
    presSens["simMin"] = pressureSensor.simMin;
    presSens["simMax"] = pressureSensor.simMax;
    presSens["simDriftPerHour"] = pressureSensor.simDriftPerHour;

    JsonObject phpump = doc.createNestedObject("phPump");
    phpump["relayChannel"] = phPump.relayChannel;
    phpump["minOnTimeSec"] = phPump.minOnTimeSec;
    phpump["minOffTimeSec"] = phPump.minOffTimeSec;
    phpump["maxDailyRuntimeMin"] = phPump.maxDailyRuntimeMin;

    JsonObject clpump = doc.createNestedObject("chlorinePump");
    clpump["relayChannel"] = chlorinePump.relayChannel;
    clpump["minOnTimeSec"] = chlorinePump.minOnTimeSec;
    clpump["minOffTimeSec"] = chlorinePump.minOffTimeSec;
    clpump["maxDailyRuntimeMin"] = chlorinePump.maxDailyRuntimeMin;

    JsonObject fp = doc.createNestedObject("filterPump");
    fp["relayChannel"] = filterPump.relayChannel;
    fp["tempSlope"] = filterPump.tempSlope;
    fp["tempIntercept"] = filterPump.tempIntercept;
    fp["windowStart"] = filterPump.windowStart;
    fp["windowEnd"] = filterPump.windowEnd;
    fp["minCycleMinutes"] = filterPump.minCycleMinutes;
    fp["maxCycleMinutes"] = filterPump.maxCycleMinutes;

    JsonArray relaysArr = doc.createNestedArray("relays");
    for (int i = 0; i < relayCount; i++) {
        JsonObject r = relaysArr.createNestedObject();
        r["channel"] = relays[i].channel;
        r["name"] = relays[i].name;
        r["normallyOpen"] = relays[i].normallyOpen;
        r["maxOnTimeSec"] = relays[i].maxOnTimeSec;
    }

    doc["relayCount"] = relayCount;
    doc["logLevel"] = logLevel;
    doc["loopDelayMs"] = loopDelayMs;

    String out;
    serializeJsonPretty(doc, out);
    return out;
}

AppConfig AppConfig::fromJson(const JsonDocument& doc) {
    AppConfig cfg;
    cfg.wifi = WifiConfig::fromJson(doc["wifi"]);
    cfg.mqtt = MqttConfig::fromJson(doc["mqtt"]);
    cfg.phPID = PIDParams::fromJson(doc["phPID"]);
    cfg.chlorinePID = PIDParams::fromJson(doc["chlorinePID"]);
    cfg.phSensor = SensorConfig::fromJson(doc["phSensor"]);
    cfg.orpSensor = SensorConfig::fromJson(doc["orpSensor"]);
    cfg.tempAirSensor = SensorConfig::fromJson(doc["tempAirSensor"]);
    cfg.tempWaterSensor = SensorConfig::fromJson(doc["tempWaterSensor"]);
    cfg.pressureSensor = SensorConfig::fromJson(doc["pressureSensor"]);
    cfg.phPump = PumpConfig::fromJson(doc["phPump"]);
    cfg.chlorinePump = PumpConfig::fromJson(doc["chlorinePump"]);
    cfg.filterPump = FilterPumpConfig::fromJson(doc["filterPump"]);

    cfg.relayCount = doc["relayCount"] | 0;
    if (doc["relays"].is<JsonArray>()) {
        JsonArrayConst arr = doc["relays"].as<JsonArrayConst>();
        int count = min((int)arr.size(), MAX_RELAYS);
        for (int i = 0; i < count; i++) {
            cfg.relays[i] = RelayConfig::fromJson(arr[i]);
        }
        cfg.relayCount = count;
    }

    cfg.logLevel = doc["logLevel"] | cfg.logLevel;
    cfg.loopDelayMs = doc["loopDelayMs"] | cfg.loopDelayMs;
    return cfg;
}

// ─── ConfigManager ────────────────────────────────────────────────────

ConfigManager::ConfigManager() {}

void ConfigManager::setDefaults() {
    _config = AppConfig();

    // Default relay configuration for KC868-A8
    const char* relayNames[MAX_RELAYS] = {
        "Filter Pumpe", "pH Pumpe", "Chlor Pumpe",
        "Relay 4", "Relay 5", "Relay 6",
        "Relay 7", "Relay 8"
    };
    for (int i = 0; i < MAX_RELAYS; i++) {
        _config.relays[i].channel = i;
        _config.relays[i].name = relayNames[i];
        _config.relays[i].normallyOpen = true;
        _config.relays[i].maxOnTimeSec = 0;
    }
    _config.relayCount = MAX_RELAYS;

    // Pump relay assignments
    _config.phPump.relayChannel = 1;
    _config.chlorinePump.relayChannel = 2;
    _config.filterPump.relayChannel = 0;

    // pH sensor defaults (simulation mode)
    _config.phSensor.simMin = 6.8;
    _config.phSensor.simMax = 7.6;
    _config.phSensor.simDriftPerHour = 0.05;

    // ORP sensor defaults
    _config.orpSensor.simMin = 200.0;
    _config.orpSensor.simMax = 800.0;
    _config.orpSensor.simDriftPerHour = 10.0;

    // Temperature sensors
    _config.tempAirSensor.simMin = 10.0;
    _config.tempAirSensor.simMax = 40.0;
    _config.tempWaterSensor.simMin = 5.0;
    _config.tempWaterSensor.simMax = 35.0;

    // Pressure sensor
    _config.pressureSensor.simMin = 0.0;
    _config.pressureSensor.simMax = 2.5;
    _config.pressureSensor.simDriftPerHour = 0.1;

    // PID defaults for pH
    _config.phPID.kp = 1.2;
    _config.phPID.ki = 0.08;
    _config.phPID.kd = 0.04;
    _config.phPID.setpoint = 7.2;
    _config.phPID.minOnTimeSec = 15;
    _config.phPID.minOffTimeSec = 60;

    // PID defaults for chlorine (ORP target ~650mV)
    _config.chlorinePID.kp = 0.8;
    _config.chlorinePID.ki = 0.05;
    _config.chlorinePID.kd = 0.02;
    _config.chlorinePID.setpoint = 650.0;
    _config.chlorinePID.minOnTimeSec = 30;
    _config.chlorinePID.minOffTimeSec = 120;
}

bool ConfigManager::begin() {
    if (!SPIFFS.begin(true)) {
        log_e("SPIFFS mount failed");
        setDefaults();
        return false;
    }

    if (!loadFromSPIFFS()) {
        log_w("No config found, creating defaults");
        setDefaults();
        saveToSPIFFS();
    }

    log_i("Config loaded successfully");
    return true;
}

bool ConfigManager::loadFromSPIFFS() {
    if (!SPIFFS.exists(CONFIG_FILE)) {
        return false;
    }

    File file = SPIFFS.open(CONFIG_FILE, FILE_READ);
    if (!file) {
        log_e("Failed to open config file");
        return false;
    }

    StaticJsonDocument<CONFIG_JSON_SIZE> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        log_e("Config parse error: %s", error.c_str());
        return false;
    }

    _config = AppConfig::fromJson(doc);
    return true;
}

bool ConfigManager::saveToSPIFFS() {
    File file = SPIFFS.open(CONFIG_FILE, FILE_WRITE);
    if (!file) {
        log_e("Failed to write config file");
        return false;
    }

    String json = _config.toJson();
    size_t written = file.print(json);
    file.close();

    log_i("Config saved (%u bytes)", written);
    return written > 0;
}

bool ConfigManager::createDefaultConfig() {
    setDefaults();
    return saveToSPIFFS();
}

AppConfig& ConfigManager::get() {
    return _config;
}

bool ConfigManager::save() {
    return saveToSPIFFS();
}

bool ConfigManager::updateFromJson(const String& json) {
    StaticJsonDocument<CONFIG_JSON_SIZE> doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        log_e("Config update parse error: %s", error.c_str());
        return false;
    }

    // Apply partial updates — only overwrite keys that exist in the incoming JSON
    AppConfig current = _config;

    if (doc["wifi"].is<JsonObject>()) {
        JsonObject w = doc["wifi"];
        if (w["ssid"].is<const char*>()) current.wifi.ssid = w["ssid"].as<const char*>();
        if (w["password"].is<const char*>()) current.wifi.password = w["password"].as<const char*>();
        if (w["hostname"].is<const char*>()) current.wifi.hostname = w["hostname"].as<const char*>();
        if (w["fallbackAP"].is<bool>()) current.wifi.fallbackAP = w["fallbackAP"];
        if (w["apSSID"].is<const char*>()) current.wifi.apSSID = w["apSSID"].as<const char*>();
        if (w["apPassword"].is<const char*>()) current.wifi.apPassword = w["apPassword"].as<const char*>();
    }

    if (doc["mqtt"].is<JsonObject>()) {
        JsonObject m = doc["mqtt"];
        if (m["broker"].is<const char*>()) current.mqtt.broker = m["broker"].as<const char*>();
        if (m["port"].is<int>()) current.mqtt.port = m["port"];
        if (m["clientId"].is<const char*>()) current.mqtt.clientId = m["clientId"].as<const char*>();
        if (m["username"].is<const char*>()) current.mqtt.username = m["username"].as<const char*>();
        if (m["password"].is<const char*>()) current.mqtt.password = m["password"].as<const char*>();
        if (m["baseTopic"].is<const char*>()) current.mqtt.baseTopic = m["baseTopic"].as<const char*>();
        if (m["keepAliveSec"].is<int>()) current.mqtt.keepAliveSec = m["keepAliveSec"];
    }

    auto mergePID = [](JsonObject& src, PIDParams& dst) {
        if (src["kp"].is<float>()) dst.kp = src["kp"];
        if (src["ki"].is<float>()) dst.ki = src["ki"];
        if (src["kd"].is<float>()) dst.kd = src["kd"];
        if (src["setpoint"].is<float>()) dst.setpoint = src["setpoint"];
        if (src["outputMin"].is<float>()) dst.outputMin = src["outputMin"];
        if (src["outputMax"].is<float>()) dst.outputMax = src["outputMax"];
        if (src["minOnTimeSec"].is<int>()) dst.minOnTimeSec = src["minOnTimeSec"];
        if (src["minOffTimeSec"].is<int>()) dst.minOffTimeSec = src["minOffTimeSec"];
    };

    auto mergeSensor = [](JsonObject& src, SensorConfig& dst) {
        if (src["enabled"].is<bool>()) dst.enabled = src["enabled"];
        if (src["simulate"].is<bool>()) dst.simulate = src["simulate"];
        if (src["updateIntervalMs"].is<int>()) dst.updateIntervalMs = src["updateIntervalMs"];
        if (src["simMin"].is<float>()) dst.simMin = src["simMin"];
        if (src["simMax"].is<float>()) dst.simMax = src["simMax"];
        if (src["simDriftPerHour"].is<float>()) dst.simDriftPerHour = src["simDriftPerHour"];
    };

    if (doc["phPID"].is<JsonObject>()) { JsonObject o = doc["phPID"]; mergePID(o, current.phPID); }
    if (doc["chlorinePID"].is<JsonObject>()) { JsonObject o = doc["chlorinePID"]; mergePID(o, current.chlorinePID); }
    if (doc["phSensor"].is<JsonObject>()) { JsonObject o = doc["phSensor"]; mergeSensor(o, current.phSensor); }
    if (doc["orpSensor"].is<JsonObject>()) { JsonObject o = doc["orpSensor"]; mergeSensor(o, current.orpSensor); }
    if (doc["tempAirSensor"].is<JsonObject>()) { JsonObject o = doc["tempAirSensor"]; mergeSensor(o, current.tempAirSensor); }
    if (doc["tempWaterSensor"].is<JsonObject>()) { JsonObject o = doc["tempWaterSensor"]; mergeSensor(o, current.tempWaterSensor); }
    if (doc["pressureSensor"].is<JsonObject>()) { JsonObject o = doc["pressureSensor"]; mergeSensor(o, current.pressureSensor); }

    auto mergePump = [](JsonObject& src, PumpConfig& dst) {
        if (src["relayChannel"].is<int>()) dst.relayChannel = src["relayChannel"];
        if (src["minOnTimeSec"].is<int>()) dst.minOnTimeSec = src["minOnTimeSec"];
        if (src["minOffTimeSec"].is<int>()) dst.minOffTimeSec = src["minOffTimeSec"];
        if (src["maxDailyRuntimeMin"].is<float>()) dst.maxDailyRuntimeMin = src["maxDailyRuntimeMin"];
    };

    if (doc["phPump"].is<JsonObject>()) { JsonObject o = doc["phPump"]; mergePump(o, current.phPump); }
    if (doc["chlorinePump"].is<JsonObject>()) { JsonObject o = doc["chlorinePump"]; mergePump(o, current.chlorinePump); }

    if (doc["filterPump"].is<JsonObject>()) {
        JsonObject o = doc["filterPump"];
        if (o["relayChannel"].is<int>()) current.filterPump.relayChannel = o["relayChannel"];
        if (o["tempSlope"].is<float>()) current.filterPump.tempSlope = o["tempSlope"];
        if (o["tempIntercept"].is<float>()) current.filterPump.tempIntercept = o["tempIntercept"];
        if (o["windowStart"].is<const char*>()) current.filterPump.windowStart = o["windowStart"].as<const char*>();
        if (o["windowEnd"].is<const char*>()) current.filterPump.windowEnd = o["windowEnd"].as<const char*>();
        if (o["minCycleMinutes"].is<int>()) current.filterPump.minCycleMinutes = o["minCycleMinutes"];
        if (o["maxCycleMinutes"].is<int>()) current.filterPump.maxCycleMinutes = o["maxCycleMinutes"];
    }

    if (doc["relayCount"].is<int>()) current.relayCount = doc["relayCount"];
    if (doc["logLevel"].is<int>()) current.logLevel = doc["logLevel"];
    if (doc["loopDelayMs"].is<int>()) current.loopDelayMs = doc["loopDelayMs"];

    if (doc["relays"].is<JsonArray>()) {
        JsonArray arr = doc["relays"].as<JsonArray>();
        int count = min((int)arr.size(), MAX_RELAYS);
        for (int i = 0; i < count; i++) {
            current.relays[i] = RelayConfig::fromJson(arr[i]);
        }
        current.relayCount = count;
    }

    _config = current;
    return saveToSPIFFS();
}

String ConfigManager::toJson() {
    return _config.toJson();
}

void ConfigManager::print() {
    Serial.println("─── Pool Controller Config ───");
    Serial.println(toJson());
    Serial.println("──────────────────────────────");
}