/**
 * Pool Controller for ESP32 (Kincony KC868-A8)
 *
 * Controls pool filter pump, pH dosing, and chlorine dosing
 * with sensor feedback, PID control, MQTT/HA integration.
 *
 * Board: Kincony KC868-A8 (ESP32)
 * Relays: 0=Filter, 1=pH Pump, 2=Chlorine Pump
 * Sensors: pH (ADS1115 ch0), ORP (ADS1115 ch1), DS18B20, DHT22, Pressure (ADS1115 ch2)
 * Communication: MQTT, WiFi, Serial
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "config/ConfigManager.h"
#include "sensors/SensorManager.h"
#include "actuators/RelayManager.h"
#include "actuators/PumpController.h"
#include "pid/PoolChemistryController.h"
#include "utils/FilterPumpLogic.h"
#include "utils/TimingUtils.h"
#include "mqtt/MQTTManager.h"

// ─── Forward Declarations ─────────────────────────────────────────────

void setupWiFi();
void setupWebServer();
void handleMQTTCommand(const char* topic, const String& payload);
void resetByWatchdog();

// ─── Global Objects ───────────────────────────────────────────────────

ConfigManager configManager;
RelayManager relayManager;
SensorManager* sensorManager = nullptr;
PumpController* phPumpCtrl = nullptr;
PumpController* chlorinePumpCtrl = nullptr;
FilterPumpLogic* filterPumpLogic = nullptr;
PoolChemistryController* chemistryController = nullptr;
MQTTManager* mqttManager = nullptr;

// ─── Timing & Status ──────────────────────────────────────────────────

static unsigned long lastSensorPublish = 0;
static const unsigned long SENSOR_PUBLISH_INTERVAL = 30000;
static unsigned long wifiReconnectTime = 0;
static bool wifiConnected = false;
static bool ntpSynced = false;
static bool systemReady = false;
static unsigned long startTime = 0;
static int watchdogCount = 0;

// ─── WiFi Configuration ───────────────────────────────────────────────

static const char* WIFI_HOSTNAME = "pool-controller";
static const char* AP_SSID = "PoolController-AP";
static const char* AP_PASSWORD = "12345678";
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GATEWAY(192, 168, 4, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

// ─── Web Server (minimal status page) ─────────────────────────────────

#include <WebServer.h>
WebServer webServer(80);

void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>Pool Controller</title>";
    html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<style>body{font-family:Arial,sans-serif;margin:20px;background:#1a1a2e;color:#eee}";
    html += "h1{color:#0f0}h2{color:#0af}.card{background:#16213e;border-radius:8px;padding:15px;margin:10px 0}";
    html += ".value{font-size:1.5em;font-weight:bold;color:#0f0}.unit{color:#888}.bad{color:#f44}.ok{color:#0f0}</style>";
    html += "</head><body><h1>🏊 Pool Controller</h1>";

    html += "<div class='card'><h2>System</h2>";
    html += "<p>Uptime: " + String(millis() / 1000 / 60) + " min</p>";
    html += "<p>WiFi: " + String(WiFi.isConnected() ? "✅ Connected" : "❌ Disconnected") + "</p>";
    html += "<p>IP: " + (WiFi.isConnected() ? WiFi.localIP().toString() : String(AP_SSID)) + "</p>";
    html += "<p>MQTT: " + String(mqttManager && mqttManager->isConnected() ? "✅ Connected" : "❌ Disconnected") + "</p>";
    html += "</div>";

    if (sensorManager) {
        html += "<div class='card'><h2>Sensors</h2>";
        html += "<p>pH: <span class='value'>" + String(sensorManager->getPH(), 2) + "</span> <span class='unit'>pH</span>";
        html += sensorManager->isPHConnected() ? " ✅" : " <span class='bad'>⚠ sim</span></p>";
        html += "<p>ORP: <span class='value'>" + String(sensorManager->getORP(), 0) + "</span> <span class='unit'>mV</span>";
        html += sensorManager->isORPConnected() ? " ✅" : " <span class='bad'>⚠ sim</span></p>";
        html += "<p>Water Temp: <span class='value'>" + String(sensorManager->getWaterTemperature(), 1) + "</span> <span class='unit'>°C</span></p>";
        html += "<p>Air Temp: <span class='value'>" + String(sensorManager->getAirTemperature(), 1) + "</span> <span class='unit'>°C</span></p>";
        html += "<p>Humidity: <span class='value'>" + String(sensorManager->getHumidity(), 0) + "</span> <span class='unit'>%</span></p>";
        html += "<p>Filter Pressure: <span class='value'>" + String(sensorManager->getFilterPressure(), 2) + "</span> <span class='unit'>bar</span></p>";
        html += "</div>";
    }

    html += "<div class='card'><h2>Chemistry</h2>";
    if (chemistryController) {
        html += "<p>pH Control: " + String(chemistryController->isPHEnabled() ? "✅ ON" : "❌ OFF") + "</p>";
        html += "<p>Chlorine Control: " + String(chemistryController->isChlorineEnabled() ? "✅ ON" : "❌ OFF") + "</p>";
    }
    if (phPumpCtrl) {
        html += "<p>pH Pump: " + String(phPumpCtrl->isOn() ? "🟢 ON" : "⚫ OFF") + "</p>";
        html += "<p>Chlorine Pump: " + String(chlorinePumpCtrl && chlorinePumpCtrl->isOn() ? "🟢 ON" : "⚫ OFF") + "</p>";
    }
    html += "</div>";

    html += "<div class='card'><h2>Filter Pump</h2>";
    if (filterPumpLogic) {
        html += "<p>Runtime today: " + String(phPumpCtrl ? phPumpCtrl->getRuntimeMinutes() : 0) + " min</p>";
    }
    html += "</div>";

    html += "<p style='color:#666'>Pool Controller v1.0.0 | ESP32 KC868-A8</p>";
    html += "</body></html>";
    webServer.send(200, "text/html", html);
}

void handleAPI() {
    StaticJsonDocument<1024> doc;
    doc["uptime_ms"] = millis();
    doc["wifi"] = WiFi.isConnected();
    doc["mqtt"] = mqttManager ? mqttManager->isConnected() : false;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["ph"] = sensorManager ? sensorManager->getPH() : 0;
    doc["orp"] = sensorManager ? sensorManager->getORP() : 0;
    doc["water_temp"] = sensorManager ? sensorManager->getWaterTemperature() : 0;
    doc["air_temp"] = sensorManager ? sensorManager->getAirTemperature() : 0;
    doc["filter_pressure"] = sensorManager ? sensorManager->getFilterPressure() : 0;

    String json;
    serializeJson(doc, json);
    webServer.send(200, "application/json", json);
}

void setupWebServer() {
    webServer.on("/", handleRoot);
    webServer.on("/api", handleAPI);
    webServer.begin();
    log_i("Web server started on port 80");
}

// ─── WiFi ────────────────────────────────────────────────────────────

void setupWiFi() {
    AppConfig& cfg = configManager.get();

    String hostname = cfg.wifi.hostname;
    if (hostname.length() == 0) hostname = WIFI_HOSTNAME;

    WiFi.setHostname(hostname.c_str());
    WiFi.mode(WIFI_MODE_STA);

    if (cfg.wifi.ssid.length() > 0) {
        log_i("Connecting to WiFi: %s", cfg.wifi.ssid.c_str());
        WiFi.begin(cfg.wifi.ssid.c_str(), cfg.wifi.password.c_str());

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 40) {
            delay(500);
            attempts++;
            log_i("WiFi attempt %d/40", attempts);
        }

        if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            log_i("WiFi connected: %s (%s)", WiFi.localIP().toString().c_str(), hostname.c_str());
            return;
        }
    }

    if (cfg.wifi.fallbackAP) {
        String apSSID = cfg.wifi.apSSID;
        String apPassword = cfg.wifi.apPassword;
        if (apSSID.length() == 0) apSSID = AP_SSID;
        if (apPassword.length() == 0) apPassword = AP_PASSWORD;

        WiFi.mode(WIFI_MODE_AP);
        WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
        WiFi.softAP(apSSID.c_str(), apPassword.c_str());

        log_i("Fallback AP started: %s / %s", apSSID.c_str(), apPassword.c_str());
        log_i("AP IP: %s", WiFi.softAPIP().toString().c_str());
    }
}

void maintainWiFi() {
    if (WiFi.status() != WL_CONNECTED && !wifiConnected) {
        unsigned long now = millis();
        if (now - wifiReconnectTime > 60000) {
            wifiReconnectTime = now;
            log_i("WiFi reconnect attempt...");
            AppConfig& cfg = configManager.get();
            if (cfg.wifi.ssid.length() > 0) {
                WiFi.reconnect();
            } else {
                setupWiFi();
            }
        }
    } else if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
    }
}

// ─── MQTT Command Handler ────────────────────────────────────────────

void handleMQTTCommand(const char* topic, const String& payload) {
    String t = String(topic);
    AppConfig& cfg = configManager.get();
    String base = cfg.mqtt.baseTopic + "/command/";

    log_i("Command: %s -> %s", t.c_str(), payload.c_str());

    if (t == base + "ph_setpoint") {
        float val = payload.toFloat();
        if (val >= 6.0 && val <= 8.0 && chemistryController) {
            chemistryController->setPHSetpoint(val);
        }
    } else if (t == base + "orp_setpoint") {
        float val = payload.toFloat();
        if (val >= 200 && val <= 900 && chemistryController) {
            chemistryController->setORPSetpoint(val);
        }
    } else if (t == base + "ph_set_enabled") {
        bool en = (payload == "true" || payload == "1" || payload == "ON");
        if (chemistryController) chemistryController->setPHEnabled(en);
    } else if (t == base + "cl_set_enabled") {
        bool en = (payload == "true" || payload == "1" || payload == "ON");
        if (chemistryController) chemistryController->setChlorineEnabled(en);
    } else if (t == base + "relay") {
        int comma1 = payload.indexOf(',');
        int comma2 = payload.indexOf(',', comma1 + 1);
        if (comma1 > 0 && comma2 > 0) {
            int channel = payload.substring(0, comma1).toInt();
            String stateStr = payload.substring(comma1 + 1, comma2);
            bool state = (stateStr == "1" || stateStr == "on" || stateStr == "ON");
            relayManager.setRelay((uint8_t)channel, state);
        }
    } else if (t == base + "all_off") {
        relayManager.allOff();
    } else if (t == base + "reset_config") {
        if (payload == "confirm") {
            configManager.get() = AppConfig();
            configManager.save();
            log_w("Config reset to defaults — restarting...");
            delay(1000);
            ESP.restart();
        }
    } else if (t == base + "restart") {
        if (payload == "confirm") {
            log_w("MQTT restart command received");
            delay(500);
            ESP.restart();
        }
    }
}

// ─── Task Watchdog ───────────────────────────────────────────────────

void feedWatchdog() {
    esp_task_wdt_reset();
    watchdogCount++;
}

// ─── Arduino Setup ──────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(1000);
    log_i("\n\n═══ Pool Controller v1.0.0 ═══");
    log_i("ESP32 @ %d MHz", getCpuFrequencyMhz());
    log_i("Free heap: %u bytes", ESP.getFreeHeap());

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 30000,
        .trigger_panic = true,
    };
    esp_task_wdt_init(&twdt_config);
#else
    esp_task_wdt_init(30, true);
#endif
    esp_task_wdt_add(NULL);

    if (!configManager.begin()) {
        log_w("Config manager initialized with defaults");
    }
    configManager.print();

    relayManager.begin();
    relayManager.allOff();
    log_i("All relays initialized OFF");

    sensorManager = new SensorManager(configManager);
    if (!sensorManager->begin()) {
        log_w("Sensor manager initialized with fallbacks");
    }

    AppConfig& cfg = configManager.get();
    phPumpCtrl = new PumpController(relayManager, cfg.phPump.relayChannel, "pH Pump");
    chlorinePumpCtrl = new PumpController(relayManager, cfg.chlorinePump.relayChannel, "Chlorine Pump");
    phPumpCtrl->begin();
    chlorinePumpCtrl->begin();

    phPumpCtrl->setMinOnTime(cfg.phPID.minOnTimeSec * 1000);
    phPumpCtrl->setMinOffTime(cfg.phPID.minOffTimeSec * 1000);
    chlorinePumpCtrl->setMinOnTime(cfg.chlorinePID.minOnTimeSec * 1000);
    chlorinePumpCtrl->setMinOffTime(cfg.chlorinePID.minOffTimeSec * 1000);

    FilterPumpConfig& fpCfg = cfg.filterPump;
    PumpController* filterPumpCtrl = new PumpController(relayManager, fpCfg.relayChannel, "Filter Pump");
    filterPumpCtrl->begin();
    filterPumpLogic = new FilterPumpLogic(configManager, *filterPumpCtrl);
    filterPumpLogic->begin();

    chemistryController = new PoolChemistryController(configManager, *sensorManager,
                                                       *phPumpCtrl, *chlorinePumpCtrl);
    chemistryController->begin();

    setupWiFi();
    setupWebServer();

    if (wifiConnected) {
        ntpSynced = initNTP(7200, 3600);
        ntpSynced = waitForNTPSync(15);
    }

    mqttManager = new MQTTManager(configManager);
    mqttManager->begin();
    mqttManager->setCommandCallback(handleMQTTCommand);

    startTime = millis();
    systemReady = true;
    log_i("═══ System ready (%lu ms) ═══", millis());
}

// ─── Arduino Loop ───────────────────────────────────────────────────

void loop() {
    unsigned long loopStart = millis();

    feedWatchdog();
    maintainWiFi();

    if (mqttManager) {
        mqttManager->loop();
    }

    webServer.handleClient();

    if (sensorManager) {
        sensorManager->update();
    }

    if (chemistryController && systemReady) {
        chemistryController->update();
    }

    if (filterPumpLogic && sensorManager) {
        float waterTemp = sensorManager->getWaterTemperature();
        filterPumpLogic->update(waterTemp);
    }

    if (mqttManager && systemReady) {
        unsigned long now = millis();
        if (now - lastSensorPublish >= SENSOR_PUBLISH_INTERVAL) {
            lastSensorPublish = now;

            String sensorStates = sensorManager ? sensorManager->getAllStateJSON() : "{}";
            String chemistryState = chemistryController ? chemistryController->getStateJSON() : "{}";
            String filterState = filterPumpLogic ? filterPumpLogic->getStateJSON() : "{}";

            StaticJsonDocument<512> pumpDoc;
            if (phPumpCtrl) {
                JsonObject ph = pumpDoc.createNestedObject("ph_pump");
                ph["name"] = phPumpCtrl->getName();
                ph["on"] = phPumpCtrl->isOn();
                ph["runtime_today_min"] = phPumpCtrl->getRuntimeMinutes();
                ph["last_on_duration_ms"] = phPumpCtrl->getLastOnDuration();
            }
            if (chlorinePumpCtrl) {
                JsonObject cl = pumpDoc.createNestedObject("chlorine_pump");
                cl["name"] = chlorinePumpCtrl->getName();
                cl["on"] = chlorinePumpCtrl->isOn();
                cl["runtime_today_min"] = chlorinePumpCtrl->getRuntimeMinutes();
                cl["last_on_duration_ms"] = chlorinePumpCtrl->getLastOnDuration();
            }
            String pumpStates;
            serializeJson(pumpDoc, pumpStates);

            mqttManager->publishState(sensorStates, chemistryState, filterState, pumpStates);

            StaticJsonDocument<256> relayDoc;
            for (int i = 0; i < KC868_A8_RELAY_COUNT; i++) {
                String key = "relay_" + String(i);
                relayDoc[key.c_str()] = relayManager.getRelayState(i);
            }
            String relayJson;
            serializeJson(relayDoc, relayJson);
            mqttManager->publish("relays", relayJson, false);
        }
    }

    AppConfig& cfg = configManager.get();
    unsigned long elapsed = millis() - loopStart;
    int delayMs = cfg.loopDelayMs - (int)elapsed;
    if (delayMs > 0) {
        delay(delayMs);
    } else if (delayMs < -100) {
        static unsigned long lastWarn = 0;
        if (millis() - lastWarn > 60000) {
            log_w("Loop took %lu ms (max %d)", elapsed, cfg.loopDelayMs);
            lastWarn = millis();
        }
    }
}

void resetByWatchdog() {
    log_e("Watchdog triggered! Resetting...");
    relayManager.allOff();
    if (mqttManager) {
        mqttManager->publishOffline();
    }
    delay(500);
    ESP.restart();
}
