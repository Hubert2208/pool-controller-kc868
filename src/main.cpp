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
    html += "<style>";
    html += "body{font-family:Arial,sans-serif;margin:10px;background:#1a1a2e;color:#eee}";
    html += "h1{color:#0f0;font-size:1.3em;margin:8px 0}h2{color:#0af;font-size:1em;margin:6px 0}";
    html += ".card{background:#16213e;border-radius:8px;padding:12px;margin:8px 0}";
    html += ".value{font-size:1.3em;font-weight:bold;color:#0f0}.bad{color:#f44}";
    html += ".btn-on{background:#0a0;color:#fff;border:none;padding:6px 16px;border-radius:4px;font-weight:bold;cursor:pointer;margin:2px;min-width:60px}";
    html += ".btn-off{background:#444;color:#ccc;border:none;padding:6px 16px;border-radius:4px;font-weight:bold;cursor:pointer;margin:2px;min-width:60px}";
    html += ".btn-on:hover{background:#0c0}.btn-off:hover{background:#666}";
    html += ".relay-btn{font-size:0.75em;padding:4px 8px;margin:2px;min-width:52px}";
    html += ".pump-label{display:inline-block;width:100px}.runtime{color:#888;font-size:0.75em;margin-left:6px}";
    html += ".manual-badge{background:#f80;color:#000;padding:2px 8px;border-radius:4px;font-weight:bold;font-size:0.85em}";
    html += ".auto-badge{background:#0a0;color:#000;padding:2px 8px;border-radius:4px;font-weight:bold;font-size:0.85em}";
    html += ".mode-btn{background:#f80;color:#000;border:none;padding:6px 16px;border-radius:4px;font-weight:bold;cursor:pointer}.mode-btn.auto{background:#0a0}";
    html += ".sp-label{display:inline-block;width:80px;text-align:right;margin-right:8px}";
    html += ".sp-range{width:140px;margin:0 8px;vertical-align:middle}";
    html += ".sp-val{display:inline-block;width:45px;color:#0f0;font-weight:bold}";
    html += ".sp-apply{background:#0af;color:#000;border:none;padding:6px 20px;border-radius:4px;font-weight:bold;cursor:pointer;margin-top:8px}";
    html += ".sp-apply:hover{background:#0cf}.sp-row{margin:4px 0}";
    html += ".sp-saved{color:#0f0;font-size:0.8em;margin-left:10px;display:none}";
    html += ".pt-row{margin:4px 0;font-size:0.85em}";
    html += ".pt-label{display:inline-block;width:70px}";
    html += ".pt-input{background:#0a0a2e;color:#0f0;border:1px solid #333;padding:2px 4px;border-radius:3px;width:55px;text-align:center}";
    html += ".pt-note{color:#888;font-size:0.73em;margin:2px 0 6px 0}";
    html += ".rt-label{display:inline-block;width:90px;text-align:right;margin-right:6px;color:#888;font-size:0.8em}";
    html += ".rt-value{color:#0f0;font-weight:bold}";
    html += ".rt-bad{color:#f44}";
    html += ".progress-bg{background:#0a0a2e;border:1px solid #333;border-radius:4px;height:14px;margin:4px 0;overflow:hidden}";
    html += ".progress-fill{height:100%;width:0;background:linear-gradient(90,#0af,#0f0);border-radius:2px;transition:width 0.5s}";
    html += ".progress-fill.low{background:linear-gradient(90,#f80,#f44)}";
    html += ".progress-fill.ok{background:linear-gradient(90,#0af,#0f0)}";
    html += "</style></head><body><h1>🏊 Pool Controller</h1>";
    html += "<div class='card'><h2>System</h2>";
    html += "<p>Mode: <span id='sys-mode'>" + String(manualMode ? "<span class='manual-badge'>🔧 MANUAL</span>" : "<span class='auto-badge'>🤖 AUTO</span>") + "</span></p>";
    html += "<p>Uptime: <span id='sys-uptime'>" + String(millis() / 1000 / 60) + "</span> min | WiFi: <span id='sys-wifi'>" + String(WiFi.isConnected() ? "✅" : "❌") + "</span> | MQTT: <span id='sys-mqtt'>" + String(mqttManager && mqttManager->isConnected() ? "✅" : "❌") + "</span> | IP: " + (WiFi.isConnected() ? WiFi.localIP().toString() : String(AP_SSID)) + "</p></div>";
    html += "<div class='card'><h2>Sensors</h2><p>";
    if (sensorManager) {
        html += "pH: <span class='value' id='val-ph'>" + String(sensorManager->getPH(), 2) + "</span> pH";
        html += sensorManager->isPHConnected() ? " ✅ " : " <span class='bad'>⚠sim</span> ";
        html += "| ORP: <span class='value' id='val-orp'>" + String(sensorManager->getORP(), 0) + "</span> mV";
        html += sensorManager->isORPConnected() ? " ✅ " : " <span class='bad'>⚠sim</span> ";
        html += "| Water: <span class='value' id='val-water'>" + String(sensorManager->getWaterTemperature(), 1) + "</span>°C";
        html += "| Air: <span class='value' id='val-air'>" + String(sensorManager->getAirTemperature(), 1) + "</span>°C";
        html += "| Pressure: <span class='value' id='val-pressure'>" + String(sensorManager->getFilterPressure(), 2) + "</span> bar";
    }
    html += "</p></div>";
    float phSP = chemistryController ? chemistryController->getPHPID().getSetpoint() : cfg.phPID.setpoint;
    float orpSP = chemistryController ? chemistryController->getChlorinePID().getSetpoint() : cfg.chlorinePID.setpoint;
    html += "<div class='card'><h2>🎯 Chemistry Setpoints</h2>";
    html += "<div class='sp-row'><span class='sp-label'>pH Target:</span><input type='range' class='sp-range' id='sp-ph-range' min='6.0' max='8.0' step='0.1' value='" + String(phSP, 1) + "' oninput=\"document.getElementById('sp-ph-val').textContent=this.value\"><span class='sp-val' id='sp-ph-val'>" + String(phSP, 1) + "</span> pH</div>";
    html += "<div class='sp-row'><span class='sp-label'>ORP Target:</span><input type='range' class='sp-range' id='sp-orp-range' min='200' max='900' step='10' value='" + String((int)orpSP) + "' oninput=\"document.getElementById('sp-orp-val').textContent=this.value\"><span class='sp-val' id='sp-orp-val'>" + String((int)orpSP) + "</span> mV</div>";
    html += "<button class='sp-apply' onclick=\"applySetpoints(event)\">Apply Setpoints</button><span class='sp-saved' id='sp-saved'>✅ Saved!</span>";
    html += "<p style='font-size:0.7em;color:#888;margin-top:6px'>Changes take effect immediately.</p></div>";
    html += "<div class='card'><h2>⏱️ Pump Timing (Anti-Short-Cycle)</h2>";
    html += "<p style='font-size:0.75em;color:#888;margin:0 0 8px 0'>Pump won't start until min OFF time elapsed; won't stop until min ON time elapsed.</p>";
    html += pumpTimingRow("ph", "pH Pump", cfg.phPump.minOnTimeSec, cfg.phPump.minOffTimeSec, cfg.phPump.maxDailyRuntimeMin);
    html += pumpTimingRow("cl", "Chlorine", cfg.chlorinePump.minOnTimeSec, cfg.chlorinePump.minOffTimeSec, cfg.chlorinePump.maxDailyRuntimeMin);
    html += pumpTimingRow("filter", "Filter", cfg.filterPump.minOnTimeSec, cfg.filterPump.minOffTimeSec, cfg.filterPump.maxDailyRuntimeMin);
    // ── Filter pre-run delay ──
    html += "<div class='pt-row'><span class='pt-label'>🔒 Pre-Run:</span>";
    html += " Filter must run <input type='number' class='pt-input' style='width:55px' id='ptd-prerun' value='" + String(cfg.filterPump.filterPreRunDelayMin) + "' min='1' max='60' step='1'> min before pH/Cl pumps start";
    html += "</div>";
    html += "<p class='pt-note'>⚠️ Chemistry pumps wait for filter pre-run delay after each filter restart.</p>";
    html += "<button class='sp-apply' onclick=\"applyPumpTiming(event)\">Apply Timing</button><span class='sp-saved' id='pt-saved'>✅ Saved!</span>";
    html += "</div>";
    html += "<div class='card'><h2>Control Mode</h2><p>";
    if (manualMode) html += "<button class='mode-btn auto' onclick=\"fetch('/api/manual?mode=0').then(r=>r.json()).then(d=>location.reload())\">Return to AUTO Mode</button>";
    else html += "<button class='mode-btn' onclick=\"fetch('/api/manual?mode=1').then(r=>r.json()).then(d=>location.reload())\">Switch to MANUAL Mode</button>";
    html += "</p></div>";
    html += "<div class='card'><h2>Pump Control</h2><p>";
    if (filterPumpCtrl) html += pumpButton("filter", "Filter", filterPumpCtrl->isOn(), filterPumpCtrl->getLastOnDuration() / 60000, filterPumpCtrl->getRuntimeMinutes()) + "<br>";
    if (phPumpCtrl) html += pumpButton("ph", "pH Pump", phPumpCtrl->isOn(), phPumpCtrl->getLastOnDuration() / 60000, phPumpCtrl->getRuntimeMinutes()) + "<br>";
    if (chlorinePumpCtrl) html += pumpButton("chlorine", "Chlorine", chlorinePumpCtrl->isOn(), chlorinePumpCtrl->getLastOnDuration() / 60000, chlorinePumpCtrl->getRuntimeMinutes()) + "<br>";
    html += "</p></div>";
    // ── Filter Pump Runtime Status ──
    html += "<div class='card'><h2>⏱️ Filter Pump Runtime</h2><p style='font-size:0.8em;color:#aaa;margin:0 0 6px 0'>Required runtime is calculated from water temperature (T/2 × 60 min).</p>";
    if (filterPumpCtrl && filterPumpLogic) {
        float requiredMin = filterPumpLogic->getDailyRequiredMinutes();
        unsigned long actualMin = filterPumpCtrl->getRuntimeMinutes();
        int pct = (requiredMin > 0) ? (int)((actualMin / requiredMin) * 100.0f) : 0;
        if (pct > 100) pct = 100;
        const char* barClass = (pct >= 100) ? "ok" : "low";
        const char* valClass = (pct >= 100) ? "" : " rt-bad";
        html += "<div class='rt-row'><span class='rt-label'>Required:</span><span class='rt-value'>" + String((int)requiredMin) + " min</span></div>";
        html += "<div class='rt-row'><span class='rt-label'>Today:</span><span class='rt-value'>" + String(actualMin) + " min</span></div>";
        html += "<div class='rt-row'><span class='rt-label'>Progress:</span><span class='rt-value" + String(valClass) + "'>" + String(pct) + "%</span></div>";
        html += "<div class='progress-bg'><div class='progress-fill " + String(barClass) + "' style='width:" + String(pct) + "%" + (pct > 0 ? ";min-width:4px" : "") + "'></div></div>";
    } else {
        html += "<span class='rt-bad'>Filter pump logic not available</span>";
    }
    html += "</p></div>";
    html += "<div class='card'><h2>Relay Test</h2><p>";
    for (int i = 0; i < KC868_A8_RELAY_COUNT; i++) html += relayButton(i, relayManager.getRelayState(i));
    html += "</p></div>";
    html += "<div class='card'><h2>Chemistry Status</h2><p>";
    if (chemistryController) { html += "pH Control: " + String(chemistryController->isPHEnabled() ? "✅ ON" : "❌ OFF"); html += " | Chlorine Control: " + String(chemistryController->isChlorineEnabled() ? "✅ ON" : "❌ OFF"); }
    html += "</p></div>";
    html += "<div class='card'><h2>Quick Actions</h2><p><a href='/api/alloff' style='color:#f44;text-decoration:none'>🛑 Emergency All Off</a></p></div>";
    html += "<script>";
    html += "function applySetpoints(e){var ph=document.getElementById('sp-ph-range').value;var orp=document.getElementById('sp-orp-range').value;var btn=e.target;btn.textContent='Saving...';btn.disabled=true;fetch('/api/setpoint?ph='+encodeURIComponent(ph)+'&orp='+encodeURIComponent(orp)).then(r=>r.json()).then(d=>{btn.textContent='Apply Setpoints';btn.disabled=false;var s=document.getElementById('sp-saved');s.style.display='inline';setTimeout(function(){s.style.display='none'},2500)}).catch(function(){btn.textContent='Apply Setpoints';btn.disabled=false})}";
    html += "function applyPumpTiming(e){var phOn=document.getElementById('pto-ph').value;var phOff=document.getElementById('ptf-ph').value;var clOn=document.getElementById('pto-cl').value;var clOff=document.getElementById('ptf-cl').value;var fOn=document.getElementById('pto-filter').value;var fOff=document.getElementById('ptf-filter').value;var btn=e.target;btn.textContent='Saving...';btn.disabled=true;fetch('/api/pump/timing?ph_on='+phOn+'&ph_off='+phOff+'&cl_on='+clOn+'&cl_off='+clOff+'&filter_on='+fOn+'&filter_off='+fOff+'&ph_day='+document.getElementById('ptd-ph').value+'&cl_day='+document.getElementById('ptd-cl').value+'&filter_day='+document.getElementById('ptd-filter').value+'&filter_prerun='+document.getElementById('ptd-prerun').value).then(r=>r.json()).then(d=>{btn.textContent='Apply Timing';btn.disabled=false;var s=document.getElementById('pt-saved');s.style.display='inline';setTimeout(function(){s.style.display='none'},2500)}).catch(function(){btn.textContent='Apply Timing';btn.disabled=false})}";
    html += "</script>";
    html += "<p style='color:#666;font-size:0.75em'>Pool Controller v1.0.0 | ESP32 KC868-A8</p></body></html>";
    webServer.send(200, "text/html", html);
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
    // ── Filter pre-run delay ──
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
    // ── Individual pump timing commands (used by HA autodiscovery number entities) ──
    else if (t == base + "ph_pump_min_on")  { int v = payload.toInt(); if (v >= 1 && v <= 3600) { cfg.phPump.minOnTimeSec = v; configManager.save(); applyPumpConfig(); } }
    else if (t == base + "ph_pump_min_off") { int v = payload.toInt(); if (v >= 1 && v <= 7200) { cfg.phPump.minOffTimeSec = v; configManager.save(); applyPumpConfig(); } }
    else if (t == base + "cl_pump_min_on")  { int v = payload.toInt(); if (v >= 1 && v <= 3600) { cfg.chlorinePump.minOnTimeSec = v; configManager.save(); applyPumpConfig(); } }
    else if (t == base + "cl_pump_min_off") { int v = payload.toInt(); if (v >= 1 && v <= 7200) { cfg.chlorinePump.minOffTimeSec = v; configManager.save(); applyPumpConfig(); } }
    else if (t == base + "filter_pump_min_on")  { int v = payload.toInt(); if (v >= 1 && v <= 3600) { cfg.filterPump.minOnTimeSec = v; configManager.save(); applyPumpConfig(); } }
    else if (t == base + "filter_pump_min_off") { int v = payload.toInt(); if (v >= 1 && v <= 7200) { cfg.filterPump.minOffTimeSec = v; configManager.save(); applyPumpConfig(); } }
    // ── Individual daily limit commands ──
    else if (t == base + "ph_pump_max_day")  { float v = payload.toFloat(); if (v >= 1 && v <= 1440) { cfg.phPump.maxDailyRuntimeMin = v; configManager.save(); } }
    else if (t == base + "cl_pump_max_day")  { float v = payload.toFloat(); if (v >= 1 && v <= 1440) { cfg.chlorinePump.maxDailyRuntimeMin = v; configManager.save(); } }
    else if (t == base + "filter_pump_max_day")  { float v = payload.toFloat(); if (v >= 1 && v <= 1440) { cfg.filterPump.maxDailyRuntimeMin = v; configManager.save(); } }
    // ── Filter pre-run delay ──
    else if (t == base + "filter_prerun_delay") { int v = payload.toInt(); if (v >= 1 && v <= 60) { cfg.filterPump.filterPreRunDelayMin = v; configManager.save(); applyPumpConfig(); log_i("MQTT: filter pre-run delay set to %d min", v); } }
    // ── Bulk pump timing (JSON) ──
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

    // Apply anti-short-cycle timing from pump config (not PID params)
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
    log_i("═══ System ready (%lu ms) ═══", millis());
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

    // MQTT State Publishing — matches HA Discovery value_template paths
    if (mqttManager && mqttManager->isConnected() && systemReady) {
        unsigned long now = millis();
        if (now - lastSensorPublish >= SENSOR_PUBLISH_INTERVAL) {
            lastSensorPublish = now;
            log_i("MQTT: publishing sensor states");

            // Dedicated raw-value topics (no JSON parsing needed by HA)
            if (sensorManager)
                mqttManager->publish("ph", String(sensorManager->getPH(), 2), false);

            // Composite JSON topics (multiple values per topic)
            // Sensor states via SensorManager::getAllStateJSON()
            if (sensorManager)
                mqttManager->publish("sensors", sensorManager->getAllStateJSON(), false);

            // Chemistry controller state via PoolChemistryController::getStateJSON()
            if (chemistryController)
                mqttManager->publish("chemistry", chemistryController->getStateJSON(), false);

            // Filter pump logic state via FilterPumpLogic::getStateJSON()
            if (filterPumpLogic)
                mqttManager->publish("filter", filterPumpLogic->getStateJSON(), false);

            // Pump runtime states
            StaticJsonDocument<256> pumpDoc;
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
            if (filterPumpCtrl) {
                JsonObject f = pumpDoc.createNestedObject("filter_pump");
                f["name"] = filterPumpCtrl->getName();
                f["on"] = filterPumpCtrl->isOn();
                f["runtime_today_min"] = filterPumpCtrl->getRuntimeMinutes();
                f["last_on_duration_ms"] = filterPumpCtrl->getLastOnDuration();
                f["required_runtime_min"] = filterPumpLogic ? filterPumpLogic->getDailyRequiredMinutes() : 0;
            }
            String pumpStates;
            serializeJson(pumpDoc, pumpStates);
            mqttManager->publish("pumps", pumpStates, false);

            // Pump timing config state (for HA autodiscovery number entities)
            StaticJsonDocument<192> timingDoc;
            JsonObject tph = timingDoc.createNestedObject("ph");
            tph["minOn"] = cfg.phPump.minOnTimeSec;
            tph["minOff"] = cfg.phPump.minOffTimeSec;
            tph["maxDailyMin"] = cfg.phPump.maxDailyRuntimeMin;
            JsonObject tcl = timingDoc.createNestedObject("chlorine");
            tcl["minOn"] = cfg.chlorinePump.minOnTimeSec;
            tcl["minOff"] = cfg.chlorinePump.minOffTimeSec;
            tcl["maxDailyMin"] = cfg.chlorinePump.maxDailyRuntimeMin;
            JsonObject tf = timingDoc.createNestedObject("filter");
            tf["minOn"] = cfg.filterPump.minOnTimeSec;
            tf["minOff"] = cfg.filterPump.minOffTimeSec;
            tf["maxDailyMin"] = cfg.filterPump.maxDailyRuntimeMin;
            tf["preRunDelay"] = cfg.filterPump.filterPreRunDelayMin;
            String timingJson;
            serializeJson(timingDoc, timingJson);
            mqttManager->publish("pump_config", timingJson, false);
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
