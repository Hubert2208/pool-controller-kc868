/**
 * Pool Controller for ESP32 (Kincony KC868-A8)
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

void setupWiFi();
void setupWebServer();
void handleMQTTCommand(const char* topic, const String& payload);
void applyPumpConfig();

ConfigManager configManager;
RelayManager relayManager;
SensorManager* sensorManager = nullptr;
PumpController* phPumpCtrl = nullptr;
PumpController* chlorinePumpCtrl = nullptr;
PumpController* filterPumpCtrl = nullptr;
FilterPumpLogic* filterPumpLogic = nullptr;
PoolChemistryController* chemistryController = nullptr;
MQTTManager* mqttManager = nullptr;

static bool manualMode = false;
static unsigned long lastSensorPublish = 0;
static const unsigned long SENSOR_PUBLISH_INTERVAL = 30000;
static unsigned long wifiReconnectTime = 0;
static bool wifiConnected = false;
static bool systemReady = false;
static const char* WIFI_HOSTNAME = "pool-controller";
static const char* AP_SSID = "PoolController-AP";
static const char* AP_PASSWORD = "***";
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GATEWAY(192, 168, 4, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

#include <WebServer.h>
WebServer webServer(80);

String pumpButton(const char* id, const char* label, bool state, unsigned long runMins, unsigned long dayMins) {
    String cls = state ? "btn-on" : "btn-off"; String txt = state ? "ON" : "OFF";
    String out = "<span class='pump-label'>" + String(label) + ":</span> ";
    out += "<button class='pump-btn " + cls + "' onclick=\"fetch('/api/pump/set?id=" + String(id) + "&state=" + String(state ? 0 : 1) + "').then(r=>r.json()).then(d=>location.reload())\">" + txt + "</button>";
    out += " <span class='runtime'>(" + String(state ? String(runMins) : "0") + "m / " + String(dayMins) + "m today)</span>";
    return out;
}

String relayButton(int channel, bool state) {
    String cls = state ? "btn-on" : "btn-off"; String txt = state ? "ON" : "OFF";
    return "<button class='relay-btn " + cls + "' onclick=\"fetch('/api/relay/set?channel=" + String(channel) + "&state=" + String(state ? 0 : 1) + "').then(r=>r.json()).then(d=>location.reload())\">R" + String(channel) + ": " + txt + "</button>";
}

String pumpTimingRow(const char* id, const char* label, int minOn, int minOff, float maxDailyMin) {
    String out = "<div class='pt-row'><span class='pt-label'>" + String(label) + ":</span>";
    out += " min ON <input type='number' class='pt-input' id='pto-" + String(id) + "' value='" + String(minOn) + "' min='1' max='3600' step='1'> sec";
    out += " | min OFF <input type='number' class='pt-input' id='ptf-" + String(id) + "' value='" + String(minOff) + "' min='1' max='7200' step='1'> sec";
    out += " | max/day <input type='number' class='pt-input' style='width:68px' id='ptd-" + String(id) + "' value='" + String((int)maxDailyMin) + "' min='1' max='1440' step='1'> min";
    out += "</div>";
    return out;
}

void handleRoot() {
    AppConfig& cfg = configManager.get();
    String html = "<!DOCTYPE html><html><head><title>Pool Controller</title>";
    html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<meta http-equiv='refresh' content='30'>";
    html += "<script src='https://unpkg.com/vue@3/dist/vue.global.js'></script>";
    html += "<link href='https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap' rel='stylesheet'>";
    html += "<style>";
    html += "*{margin:0;padding:0;box-sizing:border-box}";
    html += "body{font-family:'Inter',sans-serif;background:#0f0f23;color:#e0e0e0;margin:12px;font-size:14px;line-height:1.4}";
    html += "h1{color:#00d6ff;font-size:1.4em;margin-bottom:4px}h2{color:#00d6ff;font-size:1em;margin:4px 0 8px 0}";
    html += ".grid{display:grid;gap:12px}";
    html += "@media(min-width:768px){.grid{grid-template-columns:1fr 1fr}}";
    html += ".card{background:rgba(255,255,255,0.03);border:1px solid rgba(255,255,255,0.08);border-radius:12px;padding:14px;margin:8px 0;backdrop-filter:blur(4px)}";
    html += ".value{color:#00ff88;font-weight:600}";
    html += ".bad{color:#ff4757}";
    html += ".btn{display:inline-flex;align-items:center;justify-content:center;border:none;padding:6px 16px;border-radius:8px;font-weight:600;cursor:pointer;margin:2px;min-width:56px;font-size:0.8em;transition:all 0.2s}";
    html += ".btn-on{background:#00c851;color:#000}";
    html += ".btn-off{background:#3742fa;color:#fff}";
    html += ".btn:hover{transform:translateY(-1px);box-shadow:0 4px 12px rgba(0,0,0,0.3)}";
    html += ".sp-range{width:140px;margin:0 8px;vertical-align:middle}";
    html += ".sp-val{color:#00ff88;font-weight:600;width:45px;display:inline-block}";
    html += ".sp-apply{background:#00d6ff;color:#000;border:none;padding:6px 20px;border-radius:6px;font-weight:600;cursor:pointer;font-size:0.85em}";
    html += ".sp-apply:hover{background:#00e6ff}";
    html += ".sp-row{display:flex;align-items:center;margin:6px 0}";
    html += ".sp-label{display:inline-block;width:80px;text-align:right;margin-right:8px;color:#888;font-size:0.8em}";
    html += ".sp-saved{color:#00ff88;font-size:0.75em;margin-left:10px}";
    html += ".pt-row{display:flex;align-items:center;margin:4px 0;font-size:0.85em}";
    html += ".pt-label{display:inline-block;width:70px;color:#888}";
    html += ".pt-input{background:#0a0a2e;color:#0f0;border:1px solid #333;padding:2px 4px;border-radius:3px;width:55px;text-align:center;font-size:0.8em}";
    html += ".pt-note{color:#888;font-size:0.73em;margin:4px 0 6px 0}";
    html += ".rt-label{display:inline-block;width:80px;text-align:right;margin-right:6px;color:#888;font-size:0.8em}";
    html += ".rt-value{color:#00ff88;font-weight:bold}";
    html += ".rt-bad{color:#ff4757}";
    html += ".progress-bg{background:#0a0a2e;border:1px solid #333;border-radius:4px;height:14px;margin:4px 0;overflow:hidden}";
    html += ".progress-fill{height:100%;width:0;background:linear-gradient(90,#00d6ff,#00ff88);border-radius:2px}";
    html += ".progress-fill.low{background:linear-gradient(90,#ff8c00,#ff4757)}";
    html += ".progress-fill.ok{background:linear-gradient(90,#00d6ff,#00ff88)}";
    html += ".pump-label{display:inline-block;width:80px}";
    html += ".runtime{color:#888;font-size:0.75em;margin-left:6px}";
    html += ".badge{display:inline-block;padding:1px 8px;border-radius:6px;font-size:0.75em;font-weight:600}";
    html += ".badge-auto{background:#00c851;color:#000}";
    html += ".badge-manual{background:#ff8c00;color:#000}";
    html += ".status-dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:4px}";
    html += ".dot-ok{background:#00ff88}";
    html += ".dot-bad{background:#ff4757}";
    html += ".hidden{display:none}";
    html += "</style>";
    html += "</head><body>";
    html += "<div id='app'>";
    // Header
    html += "<div class='card'><h1> Pool Controller</h1>";
    html += "<p><span class='status-dot' :class=\"apiData.wifi?'dot-ok':'dot-bad'\"></span>{{ apiData.wifi?'WiFi OK':'WiFi OFF' }} | ";
    html += "<span class='status-dot' :class=\"apiData.mqtt?'dot-ok':'dot-bad'\"></span>{{ apiData.mqtt?'MQTT OK':'MQTT OFF' }} | ";
    html += "IP: {{ apiData.ip || '-' }} | <span class='status-dot' :class=\"apiData.manual_mode?'dot-bad':'dot-ok'\"></span>{{ apiData.manual_mode?'MANUAL':'AUTO' }}</p>";
    html += "</div>";
    // System
    html += "<div class='card'><h2>System</h2>";
    html += "<p>Uptime: {{ Math.floor(apiData.uptime_ms/60000) }} min</p>";
    html += "<p>Free Heap: {{ apiData.free_heap }} bytes</p></div>";
    // Sensors
    html += "<div class='card'><h2>Sensors</h2><p>";
    html += "pH: <span class='value'>{{ formatNum(apiData.ph, 2) }}</span> pH | ";
    html += "ORP: <span class='value'>{{ formatNum(apiData.orp, 0) }}</span> mV | ";
    html += "Water: <span class='value'>{{ formatNum(apiData.water_temp, 1) }}</span>C | ";
    html += "Air: <span class='value'>{{ formatNum(apiData.air_temp, 1) }}</span>C | ";
    html += "Pressure: <span class='value'>{{ formatNum(apiData.filter_pressure, 2) }}</span> bar";
    html += "</p></div>";
    // Chemistry Setpoints
    html += "<div class='card'><h2>Chemistry Setpoints</h2>";
    html += "<div class='sp-row'><span class='sp-label'>pH Target:</span>";
    html += "<input type='range' class='sp-range' min='6.0' max='8.0' step='0.1' v-model.number='setpoints.ph'>";
    html += "<span class='sp-val'>{{ setpoints.ph.toFixed(1) }}</span> pH</div>";
    html += "<div class='sp-row'><span class='sp-label'>ORP Target:</span>";
    html += "<input type='range' class='sp-range' min='200' max='900' step='10' v-model.number='setpoints.orp'>";
    html += "<span class='sp-val'>{{ setpoints.orp }} mV</span></div>";
    html += "<button class='sp-apply' @click='applySetpoints'>Apply</button>";
    html += "<span class='sp-saved' v-show='savedMsg'>{{ savedMsg }}</span></div>";
    // Control Mode
    html += "<div class='card'><h2>Control Mode</h2><p>";
    html += "<button class='btn' :class=\"apiData.manual_mode?'btn-off':'btn-on'\" @click='setMode(false)'>AUTO Mode</button> ";
    html += "<button class='btn' :class=\"apiData.manual_mode?'btn-on':'btn-off'\" @click='setMode(true)'>MANUAL Mode</button>";
    html += "</p></div>";
    // Pump Control (FIXED: <p> -> <div> wrapping <div> v-if elements)
    html += "<div class='card'><h2>Pump Control</h2><div>";
    html += "<div v-if='pumps.filter'><button class='btn' :class=\"pumps.filter.on?'btn-on':'btn-off'\" @click=\"togglePump('filter')\">Filter {{ pumps.filter.on?'ON':'OFF' }}</button> ";
    html += "<span class='runtime'>({{ pumps.filter.current_min }}m / {{ pumps.filter.today_min }}m today)</span></div>";
    html += "<div v-if='pumps.ph'><button class='btn' :class=\"pumps.ph.on?'btn-on':'btn-off'\" @click=\"togglePump('ph')\">pH {{ pumps.ph.on?'ON':'OFF' }}</button> ";
    html += "<span class='runtime'>({{ pumps.ph.current_min }}m / {{ pumps.ph.today_min }}m today)</span></div>";
    html += "<div v-if='pumps.chlorine'><button class='btn' :class=\"pumps.chlorine.on?'btn-on':'btn-off'\" @click=\"togglePump('chlorine')\">Cl {{ pumps.chlorine.on?'ON':'OFF' }}</button> ";
    html += "<span class='runtime'>({{ pumps.chlorine.current_min }}m / {{ pumps.chlorine.today_min }}m today)</span></div>";
    html += "</div></div>";
    // Filter Pump Runtime (FIXED: <p> -> <div> wrapping <div> rt-row elements)
    html += "<div class='card'><h2>Filter Pump Runtime</h2>";
    html += "<p style='font-size:0.8em;color:#aaa'>Required: T/2 x 60 min</p><div>";
    html += "<div class='rt-row'><span class='rt-label'>Required:</span><span class='rt-value'>{{ requiredRuntime }} min</span></div>";
    html += "<div class='rt-row'><span class='rt-label'>Today:</span><span class='rt-value'>{{ actualRuntime }} min</span></div>";
    html += "<div class='rt-row'><span class='rt-label'>Progress:</span>";
    html += "<span class='rt-value' :class=\"progressBadClass\">{{ progressPct }}%</span></div>";
    html += "<div class='progress-bg'><div class='progress-fill' :class=\"progressFillClass\" :style=\"progressStyle\"></div></div>";
    html += "</div></div>";
    // Relay Test (FIXED: <p> -> <div> wrapping <div> v-for element)
    html += "<div class='card'><h2>Relay Test</h2><div>";
    html += "<div v-for='(state, i) in relays' :key='i'>";
    html += "<button class='btn' :class=\"state?'btn-on':'btn-off'\" @click='toggleRelay(i)'>R{{ i }} {{ state?'ON':'OFF' }}</button></div>";
    html += "</div></div>";
    // Chemistry Status
    html += "<div class='card'><h2>Chemistry Status</h2><p>";
    html += "pH Control: <span :class=\"phEnabled?'value':'bad'\">{{ phEnabled?'ON':'OFF' }}</span> | ";
    html += "Chlorine Control: <span :class=\"clEnabled?'value':'bad'\">{{ clEnabled?'ON':'OFF' }}</span></p></div>";
    // Quick Actions
    html += "<div class='card'><h2>Quick Actions</h2><p><a href='/api/alloff' class='bad' style='text-decoration:none'>All Off</a></p></div>";
    html += "<script>";
    html += "const api = Vue.createApp({";
    html += "data(){return {";
    html += "apiData:{},";
    html += "setpoints:{ph:7.2,orp:750},";
    html += "savedMsg:'',fetchError:''";
    html += "}}";
    html += ",computed:{";
    html += "pumps(){return this.apiData?.pumps || {}},";
    html += "relays(){return this.apiData?.relays || []},";
    html += "phEnabled(){return this.apiData?.ph_enabled || false},";
    html += "clEnabled(){return this.apiData?.cl_enabled || false},";
    html += "requiredRuntime(){return this.apiData?.pumps?.filter?.required_runtime_min || 0},";
    html += "actualRuntime(){return this.apiData?.pumps?.filter?.today_min || 0},";
    html += "progressPct(){const r=this.requiredRuntime,a=this.actualRuntime;return r>0?Math.max(1,Math.round((a/r)*100)):0},progressBadClass(){return this.progressPct>=100?'':'rt-bad'},progressFillClass(){return this.progressPct>=100?'ok':'low'},progressStyle(){return{width:this.progressPct+'%',minWidth:this.actualRuntime>=1?'4px':'0'}}";
    html += "}";
    html += ",methods:{";
    html += "async fetchApi(){try{const r=await fetch('/api');this.apiData=await r.json();if(this.apiData.ph_setpoint!==undefined)this.setpoints.ph=this.apiData.ph_setpoint;if(this.apiData.orp_setpoint!==undefined)this.setpoints.orp=this.apiData.orp_setpoint;}catch(e){console.error('fetchApi error:',e);this.fetchError=e.message;}},";
    html += "formatNum(v,d){return v!=null?v.toFixed(d):'-'},";
    html += "async applySetpoints(){this.savedMsg='Saving...';await fetch('/api/setpoint?ph='+this.setpoints.ph+'&orp='+this.setpoints.orp);this.savedMsg='Saved!';setTimeout(()=>this.savedMsg='',2000);this.fetchApi();},";
    html += "async setMode(m){await fetch('/api/manual?mode='+(m?1:0));this.fetchApi();},";
    html += "async togglePump(id){const p=this.pumps[id];if(!p)return;await fetch('/api/pump/set?id='+id+'&state='+(p.on?0:1));this.fetchApi();},";
    html += "async toggleRelay(ch){await fetch('/api/relay/set?channel='+ch+'&state='+(!this.relays[ch]?1:0));this.fetchApi();},";
    html += "async allOff(){await fetch('/api/alloff');this.fetchApi();}";
    html += "}";
    html += ",mounted(){this.fetchApi();setInterval(()=>this.fetchApi(),15000)}";
    html += "})";
    html += "api.mount('#app');";
    html += "</script>";
    html += "</body></html>";
    webServer.send(200, "text/html; charset=utf-8", html);
}

void handleAPI() {
    AppConfig& cfg = configManager.get();
    StaticJsonDocument<1024> doc;
    doc["uptime_ms"] = millis(); doc["wifi"] = WiFi.isConnected(); doc["mqtt"] = mqttManager && mqttManager->isConnected();
    doc["free_heap"] = ESP.getFreeHeap(); doc["manual_mode"] = manualMode;
    doc["ph"] = sensorManager ? sensorManager->getPH() : 0;
    doc["orp"] = sensorManager ? sensorManager->getORP() : 0;
    doc["water_temp"] = sensorManager ? sensorManager->getWaterTemperature() : 0;
    doc["air_temp"] = sensorManager ? sensorManager->getAirTemperature() : 0;
    doc["filter_pressure"] = sensorManager ? sensorManager->getFilterPressure() : 0;
    doc["ph_setpoint"] = chemistryController ? chemistryController->getPHPID().getSetpoint() : cfg.phPID.setpoint;
    doc["orp_setpoint"] = chemistryController ? chemistryController->getChlorinePID().getSetpoint() : cfg.chlorinePID.setpoint;
    doc["ip"] = WiFi.isConnected() ? WiFi.localIP().toString() : String("0.0.0.0");
    doc["ph_enabled"] = chemistryController ? chemistryController->isPHEnabled() : false;
    doc["cl_enabled"] = chemistryController ? chemistryController->isChlorineEnabled() : false;
    JsonObject pumps = doc.createNestedObject("pumps");
    if (filterPumpCtrl) { JsonObject f = pumps.createNestedObject("filter"); f["on"] = filterPumpCtrl->isOn(); f["current_min"] = filterPumpCtrl->getLastOnDuration() / 60000; f["today_min"] = filterPumpCtrl->getRuntimeMinutes(); f["required_runtime_min"] = filterPumpLogic ? filterPumpLogic->getDailyRequiredMinutes() : 0; }
    if (phPumpCtrl) { JsonObject p = pumps.createNestedObject("ph"); p["on"] = phPumpCtrl->isOn(); p["current_min"] = phPumpCtrl->getLastOnDuration() / 60000; p["today_min"] = phPumpCtrl->getRuntimeMinutes(); }
    if (chlorinePumpCtrl) { JsonObject c = pumps.createNestedObject("chlorine"); c["on"] = chlorinePumpCtrl->isOn(); c["current_min"] = chlorinePumpCtrl->getLastOnDuration() / 60000; c["today_min"] = chlorinePumpCtrl->getRuntimeMinutes(); }
    JsonArray relays = doc.createNestedArray("relays");
    for (int i = 0; i < KC868_A8_RELAY_COUNT; i++) relays.add(relayManager.getRelayState(i));
    String json; serializeJson(doc, json); webServer.send(200, "application/json", json);
}

void handleAPIRelaySet() {
    if (!webServer.hasArg("channel") || !webServer.hasArg("state")) { webServer.send(400, "application/json", "{\"error\":\"missing args\"}"); return; }
    int channel = webServer.arg("channel").toInt(); bool state = (webServer.arg("state") == "1" || webServer.arg("state") == "true");
    if (channel < 0 || channel >= KC868_A8_RELAY_COUNT) { webServer.send(400, "application/json", "{\"error\":\"bad channel\"}"); return; }
    relayManager.setRelay((uint8_t)channel, state);
    webServer.send(200, "application/json", "{\"ok\":true}");
}

void handleAPIPumpSet() {
    if (!webServer.hasArg("id") || !webServer.hasArg("state")) { webServer.send(400, "application/json", "{\"error\":\"missing args\"}"); return; }
    String id = webServer.arg("id"); bool state = (webServer.arg("state") == "1" || webServer.arg("state") == "true");
    PumpController* pump = nullptr;
    if (id == "filter") pump = filterPumpCtrl; else if (id == "ph") pump = phPumpCtrl; else if (id == "chlorine") pump = chlorinePumpCtrl;
    if (!pump) { webServer.send(400, "application/json", "{\"error\":\"bad id\"}"); return; }
    bool ok = state ? pump->turnOn() : pump->turnOff();
    String json = "{\"ok\":" + String(ok) + ",\"pump\":\"" + id + "\"}";
    webServer.send(200, "application/json", json);
}

void handleAPIManualMode() {
    if (!webServer.hasArg("mode")) { webServer.send(400, "application/json", "{\"error\":\"missing mode\"}"); return; }
    manualMode = (webServer.arg("mode") == "1" || webServer.arg("mode") == "true");
    if (!manualMode && chemistryController) chemistryController->begin();
    webServer.send(200, "application/json", "{\"ok\":true}");
}

void handleAPIAllOff() { relayManager.allOff(); webServer.send(200, "application/json", "{\"ok\":true}"); }

void handleAPISetpoint() {
    AppConfig& cfg = configManager.get();
    bool changed = false;
    if (webServer.hasArg("ph")) { float ph = webServer.arg("ph").toFloat(); if (ph >= 6.0f && ph <= 8.0f) { if (chemistryController) chemistryController->setPHSetpoint(ph); else cfg.phPID.setpoint = ph; changed = true; } }
    if (webServer.hasArg("orp")) { float orp = webServer.arg("orp").toFloat(); if (orp >= 200.0f && orp <= 900.0f) { if (chemistryController) chemistryController->setORPSetpoint(orp); else cfg.chlorinePID.setpoint = orp; changed = true; } }
    if (changed && chemistryController) configManager.save();
    webServer.send(changed ? 200 : 400, "application/json", "{\"ok\":" + String(changed) + "}");
}

void handleAPIPumpTiming() {
    AppConfig& cfg = configManager.get();
    bool changed = false;

    auto applyTiming = [](int val, int& target, int minVal, int maxVal) -> bool {
        if (val >= minVal && val <= maxVal) { target = val; return true; }
        return false;
    };

    if (webServer.hasArg("ph_on")) { int v = webServer.arg("ph_on").toInt(); if (applyTiming(v, cfg.phPump.minOnTimeSec, 1, 3600)) changed = true; }
    if (webServer.hasArg("ph_off")) { int v = webServer.arg("ph_off").toInt(); if (applyTiming(v, cfg.phPump.minOffTimeSec, 1, 7200)) changed = true; }
    if (webServer.hasArg("cl_on")) { int v = webServer.arg("cl_on").toInt(); if (applyTiming(v, cfg.chlorinePump.minOnTimeSec, 1, 3600)) changed = true; }
    if (webServer.hasArg("cl_off")) { int v = webServer.arg("cl_off").toInt(); if (applyTiming(v, cfg.chlorinePump.minOffTimeSec, 1, 7200)) changed = true; }
    if (webServer.hasArg("filter_on")) { int v = webServer.arg("filter_on").toInt(); if (applyTiming(v, cfg.filterPump.minOnTimeSec, 1, 3600)) changed = true; }
    if (webServer.hasArg("filter_off")) { int v = webServer.arg("filter_off").toInt(); if (applyTiming(v, cfg.filterPump.minOffTimeSec, 1, 7200)) changed = true; }
    auto applyDaily = [](float val, float& target, float minVal, float maxVal) -> bool {
        if (val >= minVal && val <= maxVal) { target = val; return true; }
        return false;
    };
    if (webServer.hasArg("ph_day")) { float v = webServer.arg("ph_day").toFloat(); if (applyDaily(v, cfg.phPump.maxDailyRuntimeMin, 1, 1440)) changed = true; }
    if (webServer.hasArg("cl_day")) { float v = webServer.arg("cl_day").toFloat(); if (applyDaily(v, cfg.chlorinePump.maxDailyRuntimeMin, 1, 1440)) changed = true; }
    if (webServer.hasArg("filter_day")) { float v = webServer.arg("filter_day").toFloat(); if (applyDaily(v, cfg.filterPump.maxDailyRuntimeMin, 1, 1440)) changed = true; }
    // Filter pre-run delay
    if (webServer.hasArg("filter_prerun")) { int v = webServer.arg("filter_prerun").toInt(); if (applyTiming(v, cfg.filterPump.filterPreRunDelayMin, 1, 60)) changed = true; }

    if (changed) {
        configManager.save();
        applyPumpConfig();
    }
    webServer.send(changed ? 200 : 400, "application/json", "{\"ok\":" + String(changed) + "}");
}

void setupWebServer() {
    webServer.on("/", handleRoot); webServer.on("/api", handleAPI); webServer.on("/api/relay/set", handleAPIRelaySet);
    webServer.on("/api/pump/set", handleAPIPumpSet); webServer.on("/api/manual", handleAPIManualMode);
    webServer.on("/api/alloff", handleAPIAllOff); webServer.on("/api/setpoint", handleAPISetpoint);
    webServer.on("/api/pump/timing", handleAPIPumpTiming);
    webServer.begin();
}

void setupWiFi() {
    AppConfig& cfg = configManager.get();
    String hostname = cfg.wifi.hostname; if (hostname.length() == 0) hostname = WIFI_HOSTNAME;
    WiFi.setHostname(hostname.c_str()); WiFi.mode(WIFI_MODE_STA);
    if (cfg.wifi.ssid.length() > 0) { WiFi.begin(cfg.wifi.ssid.c_str(), cfg.wifi.password.c_str()); for (int a = 0; WiFi.status() != WL_CONNECTED && a < 40; a++) delay(500); if (WiFi.status() == WL_CONNECTED) { wifiConnected = true; return; } }
    if (cfg.wifi.fallbackAP) { String s = cfg.wifi.apSSID, p = cfg.wifi.apPassword; if (s.length() == 0) s = AP_SSID; if (p.length() == 0) p = AP_PASSWORD; WiFi.mode(WIFI_MODE_AP); WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET); WiFi.softAP(s.c_str(), p.c_str()); }
}

void maintainWiFi() {
    if (WiFi.status() != WL_CONNECTED && !wifiConnected) { if (millis() - wifiReconnectTime > 60000) { wifiReconnectTime = millis(); AppConfig& cfg = configManager.get(); if (cfg.wifi.ssid.length() > 0) WiFi.reconnect(); else setupWiFi(); } }
    else if (WiFi.status() == WL_CONNECTED) wifiConnected = true;
}

void applyPumpConfig() {
    AppConfig& cfg = configManager.get();
    if (phPumpCtrl) {
        phPumpCtrl->setMinOnTime((unsigned long)(cfg.phPump.minOnTimeSec * 1000));
        phPumpCtrl->setMinOffTime((unsigned long)(cfg.phPump.minOffTimeSec * 1000));
        phPumpCtrl->setFilterPreRunDelay((unsigned long)(cfg.filterPump.filterPreRunDelayMin * 60000));
    }
    if (chlorinePumpCtrl) {
        chlorinePumpCtrl->setMinOnTime((unsigned long)(cfg.chlorinePump.minOnTimeSec * 1000));
        chlorinePumpCtrl->setMinOffTime((unsigned long)(cfg.chlorinePump.minOffTimeSec * 1000));
        chlorinePumpCtrl->setFilterPreRunDelay((unsigned long)(cfg.filterPump.filterPreRunDelayMin * 60000));
    }
    if (filterPumpCtrl) {
        filterPumpCtrl->setMinOnTime((unsigned long)(cfg.filterPump.minOnTimeSec * 1000));
        filterPumpCtrl->setMinOffTime((unsigned long)(cfg.filterPump.minOffTimeSec * 1000));
    }
    log_i("Pump timing applied: pH=%d/%ds Cl=%d/%ds Filter=%d/%ds PreRunDelay=%dmin",
          cfg.phPump.minOnTimeSec, cfg.phPump.minOffTimeSec,
          cfg.chlorinePump.minOnTimeSec, cfg.chlorinePump.minOffTimeSec,
          cfg.filterPump.minOnTimeSec, cfg.filterPump.minOffTimeSec,
          cfg.filterPump.filterPreRunDelayMin);
}

void handleMQTTCommand(const char* topic, const String& payload) {
    String t = String(topic); AppConfig& cfg = configManager.get(); String base = cfg.mqtt.baseTopic + "/command/";
    if (t == base + "ph_setpoint") { float v = payload.toFloat(); if (v >= 6.0 && v <= 8.0 && chemistryController) chemistryController->setPHSetpoint(v); }
    else if (t == base + "orp_setpoint") { float v = payload.toFloat(); if (v >= 200 && v <= 900 && chemistryController) chemistryController->setORPSetpoint(v); }
    else if (t == base + "ph_set_enabled") { if (chemistryController) chemistryController->setPHEnabled(payload == "true" || payload == "1" || payload == "ON"); }
    else if (t == base + "cl_set_enabled") { if (chemistryController) chemistryController->setChlorineEnabled(payload == "true" || payload == "1" || payload == "ON"); }
    else if (t == base + "all_off") relayManager.allOff();
    else if (t == base + "reset_config") { if (payload == "confirm") { configManager.get() = AppConfig(); configManager.save(); delay(1000); ESP.restart(); } }
    else if (t == base + "restart") { if (payload == "confirm") { delay(500); ESP.restart(); } }
    // Individual pump timing commands
    else if (t == base + "ph_pump_min_on")  { int v = payload.toInt(); if (v >= 1 && v <= 3600) { cfg.phPump.minOnTimeSec = v; configManager.save(); applyPumpConfig(); } }
    else if (t == base + "ph_pump_min_off") { int v = payload.toInt(); if (v >= 1 && v <= 7200) { cfg.phPump.minOffTimeSec = v; configManager.save(); applyPumpConfig(); } }
    else if (t == base + "cl_pump_min_on")  { int v = payload.toInt(); if (v >= 1 && v <= 3600) { cfg.chlorinePump.minOnTimeSec = v; configManager.save(); applyPumpConfig(); } }
    else if (t == base + "cl_pump_min_off") { int v = payload.toInt(); if (v >= 1 && v <= 7200) { cfg.chlorinePump.minOffTimeSec = v; configManager.save(); applyPumpConfig(); } }
    else if (t == base + "filter_pump_min_on")  { int v = payload.toInt(); if (v >= 1 && v <= 3600) { cfg.filterPump.minOnTimeSec = v; configManager.save(); applyPumpConfig(); } }
    else if (t == base + "filter_pump_min_off") { int v = payload.toInt(); if (v >= 1 && v <= 7200) { cfg.filterPump.minOffTimeSec = v; configManager.save(); applyPumpConfig(); } }
    // Individual daily limit commands
    else if (t == base + "ph_pump_max_day")  { float v = payload.toFloat(); if (v >= 1 && v <= 1440) { cfg.phPump.maxDailyRuntimeMin = v; configManager.save(); } }
    else if (t == base + "cl_pump_max_day")  { float v = payload.toFloat(); if (v >= 1 && v <= 1440) { cfg.chlorinePump.maxDailyRuntimeMin = v; configManager.save(); } }
    else if (t == base + "filter_pump_max_day")  { float v = payload.toFloat(); if (v >= 1 && v <= 1440) { cfg.filterPump.maxDailyRuntimeMin = v; configManager.save(); } }
    // Filter pre-run delay
    else if (t == base + "filter_prerun_delay") { int v = payload.toInt(); if (v >= 1 && v <= 60) { cfg.filterPump.filterPreRunDelayMin = v; configManager.save(); applyPumpConfig(); log_i("MQTT: filter pre-run delay set to %d min", v); } }
    // Bulk pump timing (JSON)
    else if (t == base + "pump_timing") {
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, payload) == DeserializationError::Ok) {
            if (doc["ph"]["minOn"].is<int>()) cfg.phPump.minOnTimeSec = doc["ph"]["minOn"];
            if (doc["ph"]["minOff"].is<int>()) cfg.phPump.minOffTimeSec = doc["ph"]["minOff"];
            if (doc["chlorine"]["minOn"].is<int>()) cfg.chlorinePump.minOnTimeSec = doc["chlorine"]["minOn"];
            if (doc["chlorine"]["minOff"].is<int>()) cfg.chlorinePump.minOffTimeSec = doc["chlorine"]["minOff"];
            if (doc["filter"]["minOn"].is<int>()) cfg.filterPump.minOnTimeSec = doc["filter"]["minOn"];
            if (doc["filter"]["minOff"].is<int>()) cfg.filterPump.minOffTimeSec = doc["filter"]["minOff"];
            if (doc["filter"]["preRunDelay"].is<int>()) cfg.filterPump.filterPreRunDelayMin = doc["filter"]["preRunDelay"];
            configManager.save();
            applyPumpConfig();
            log_i("MQTT: pump timing updated");
        }
    }
}

void feedWatchdog() { esp_task_wdt_reset(); }

void setup() {
    Serial.begin(115200); delay(1000);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_task_wdt_config_t twdt_config = { .timeout_ms = 30000, .trigger_panic = true }; esp_task_wdt_init(&twdt_config);
#else
    esp_task_wdt_init(30, true);
#endif
    esp_task_wdt_add(NULL);
    if (!configManager.begin()) log_w("Config defaults");
    Wire.begin(4, 5); relayManager.begin(); relayManager.allOff();
    sensorManager = new SensorManager(configManager); sensorManager->begin();
    AppConfig& cfg = configManager.get();
    phPumpCtrl = new PumpController(relayManager, cfg.phPump.relayChannel, "pH Pump"); phPumpCtrl->begin();
    chlorinePumpCtrl = new PumpController(relayManager, cfg.chlorinePump.relayChannel, "Chlorine Pump"); chlorinePumpCtrl->begin();
    filterPumpCtrl = new PumpController(relayManager, cfg.filterPump.relayChannel, "Filter Pump"); filterPumpCtrl->begin();

    applyPumpConfig();

    filterPumpLogic = new FilterPumpLogic(configManager, *filterPumpCtrl); filterPumpLogic->begin();
    filterPumpCtrl->addDependent(phPumpCtrl); filterPumpCtrl->addDependent(chlorinePumpCtrl);
    chemistryController = new PoolChemistryController(configManager, *sensorManager, *phPumpCtrl, *chlorinePumpCtrl); chemistryController->begin();
    setupWiFi(); setupWebServer();

    if (wifiConnected) {
        initNTP(7200, 3600);
        waitForNTPSync(15);
    }

    mqttManager = new MQTTManager(configManager); mqttManager->begin(); mqttManager->setCommandCallback(handleMQTTCommand);
    systemReady = true;
    log_i("System ready (%lu ms)", millis());
}

void loop() {
    unsigned long loopStart = millis();
    feedWatchdog();
    maintainWiFi();
    if (mqttManager) mqttManager->loop();
    webServer.handleClient();
    if (sensorManager) sensorManager->update();
    if (chemistryController && systemReady && !manualMode) chemistryController->update();
    if (filterPumpLogic && sensorManager && !manualMode) filterPumpLogic->update(sensorManager->getWaterTemperature());

    AppConfig& cfg = configManager.get();

    if (mqttManager && mqttManager->isConnected() && systemReady) {
        unsigned long now = millis();
        if (now - lastSensorPublish >= SENSOR_PUBLISH_INTERVAL) {
            lastSensorPublish = now;
            log_i("MQTT: publishing sensor states");
            if (sensorManager) mqttManager->publish("ph", String(sensorManager->getPH(), 2), false);
            if (sensorManager) mqttManager->publish("sensors", sensorManager->getAllStateJSON(), false);
            if (chemistryController) mqttManager->publish("chemistry", chemistryController->getStateJSON(), false);
            if (filterPumpLogic) mqttManager->publish("filter", filterPumpLogic->getStateJSON(), false);

            StaticJsonDocument<256> pumpDoc;
            if (phPumpCtrl) { JsonObject ph = pumpDoc.createNestedObject("ph_pump"); ph["name"] = phPumpCtrl->getName(); ph["on"] = phPumpCtrl->isOn(); ph["runtime_today_min"] = phPumpCtrl->getRuntimeMinutes(); ph["last_on_duration_ms"] = phPumpCtrl->getLastOnDuration(); }
            if (chlorinePumpCtrl) { JsonObject cl = pumpDoc.createNestedObject("chlorine_pump"); cl["name"] = chlorinePumpCtrl->getName(); cl["on"] = chlorinePumpCtrl->isOn(); cl["runtime_today_min"] = chlorinePumpCtrl->getRuntimeMinutes(); cl["last_on_duration_ms"] = chlorinePumpCtrl->getLastOnDuration(); }
            if (filterPumpCtrl) { JsonObject f = pumpDoc.createNestedObject("filter_pump"); f["name"] = filterPumpCtrl->getName(); f["on"] = filterPumpCtrl->isOn(); f["runtime_today_min"] = filterPumpCtrl->getRuntimeMinutes(); f["last_on_duration_ms"] = filterPumpCtrl->getLastOnDuration(); f["required_runtime_min"] = filterPumpLogic ? filterPumpLogic->getDailyRequiredMinutes() : 0; }
            String pumpStates; serializeJson(pumpDoc, pumpStates); mqttManager->publish("pumps", pumpStates, false);

            StaticJsonDocument<192> timingDoc;
            JsonObject tph = timingDoc.createNestedObject("ph"); tph["minOn"] = cfg.phPump.minOnTimeSec; tph["minOff"] = cfg.phPump.minOffTimeSec; tph["maxDailyMin"] = cfg.phPump.maxDailyRuntimeMin;
            JsonObject tcl = timingDoc.createNestedObject("chlorine"); tcl["minOn"] = cfg.chlorinePump.minOnTimeSec; tcl["minOff"] = cfg.chlorinePump.minOffTimeSec; tcl["maxDailyMin"] = cfg.chlorinePump.maxDailyRuntimeMin;
            JsonObject tf = timingDoc.createNestedObject("filter"); tf["minOn"] = cfg.filterPump.minOnTimeSec; tf["minOff"] = cfg.filterPump.minOffTimeSec; tf["maxDailyMin"] = cfg.filterPump.maxDailyRuntimeMin; tf["preRunDelay"] = cfg.filterPump.filterPreRunDelayMin;
            String timingJson; serializeJson(timingDoc, timingJson); mqttManager->publish("pump_config", timingJson, false);
        }
    }

    unsigned long elapsed = millis() - loopStart;
    int delayMs = cfg.loopDelayMs - (int)elapsed;
    if (delayMs > 0) {
        delay(delayMs);
    } else if (delayMs < -100) {
        static unsigned long lw = 0;
        if (millis() - lw > 60000) {
            log_w("Loop overrun: %lu ms (limit %d ms)", elapsed, cfg.loopDelayMs);
            lw = millis();
        }
    }
}
