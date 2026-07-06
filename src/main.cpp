/**
 * Pool Controller for ESP32 (Kincony KC868-A8)
 *
 * Controls pool filter pump, pH dosing, and chlorine dosing
 * with sensor feedback, PID control, MQTT/HA integration.
 *
 * Board: Kincony KC868-A8 (ESP32)
 * Relays: 0=Filter, 1=pH Pump, 2=Chlorine Pump
 * Sensors: pH (ADS1115 ch0), ORP (ADS1115 ch1), DS18B20 (water GPIO14, air GPIO13), Pressure (ADS1115 ch2)
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
PumpController* filterPumpCtrl = nullptr;
FilterPumpLogic* filterPumpLogic = nullptr;
PoolChemistryController* chemistryController = nullptr;
MQTTManager* mqttManager = nullptr;

// ─── Manual Override Mode ────────────────────────────────────────────

static bool manualMode = false;

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
static const char* AP_PASSWORD = "***";
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GATEWAY(192, 168, 4, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

// ─── Web Server ───────────────────────────────────────────────────────

#include <WebServer.h>
WebServer webServer(80);

// ─── Helper: button HTML ──────────────────────────────────────────────

String pumpButton(const char* id, const char* label, bool state, unsigned long runMins, unsigned long dayMins) {
    String cls = state ? "btn-on" : "btn-off";
    String txt = state ? "ON" : "OFF";
    String out = "<span class='pump-label'>" + String(label) + ":</span> ";
    out += "<button class='pump-btn " + cls + "' id='btn-" + String(id) + "' onclick=\"fetch('/api/pump/set?id=" + String(id) + "&state=" + String(state ? 0 : 1) + "').then(r=>r.json()).then(d=>location.reload())\">" + txt + "</button>";
    out += " <span class='runtime' id='run-" + String(id) + "'>(" + String(state ? String(runMins) : "0") + "m / " + String(dayMins) + "m today)</span>";
    return out;
}

String relayButton(int channel, bool state) {
    String cls = state ? "btn-on" : "btn-off";
    String txt = state ? "ON" : "OFF";
    String out = "<button class='relay-btn " + cls + "' id='rel-" + String(channel) + "' onclick=\"fetch('/api/relay/set?channel=" + String(channel) + "&state=" + String(state ? 0 : 1) + "').then(r=>r.json()).then(d=>location.reload())\">R" + String(channel) + ": " + txt + "</button>";
    return out;
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>Pool Controller</title>";
    html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<style>";
    html += "body{font-family:Arial,sans-serif;margin:10px;background:#1a1a2e;color:#eee}";
    html += "h1{color:#0f0;font-size:1.3em;margin:8px 0}h2{color:#0af;font-size:1em;margin:6px 0}";
    html += ".card{background:#16213e;border-radius:8px;padding:12px;margin:8px 0}";
    html += ".value{font-size:1.3em;font-weight:bold;color:#0f0}.unit{color:#888}.bad{color:#f44}";
    html += ".btn-on{background:#0a0;color:#fff;border:none;padding:6px 16px;border-radius:4px;font-weight:bold;cursor:pointer;margin:2px;min-width:60px}";
    html += ".btn-off{background:#444;color:#ccc;border:none;padding:6px 16px;border-radius:4px;font-weight:bold;cursor:pointer;margin:2px;min-width:60px}";
    html += ".btn-on:hover{background:#0c0}.btn-off:hover{background:#666}";
    html += ".relay-btn{font-size:0.75em;padding:4px 8px;margin:2px;min-width:52px}";
    html += ".pump-label{display:inline-block;width:100px}";
    html += ".runtime{color:#888;font-size:0.75em;margin-left:6px}";
    html += ".manual-badge{background:#f80;color:#000;padding:2px 8px;border-radius:4px;font-weight:bold;font-size:0.85em}";
    html += ".auto-badge{background:#0a0;color:#000;padding:2px 8px;border-radius:4px;font-weight:bold;font-size:0.85em}";
    html += ".mode-btn{background:#f80;color:#000;border:none;padding:6px 16px;border-radius:4px;font-weight:bold;cursor:pointer}";
    html += ".mode-btn.auto{background:#0a0}";
    html += ".sp-label{display:inline-block;width:80px;text-align:right;margin-right:8px}";
    html += ".sp-input{width:70px;border:1px solid #0af;background:#1a1a2e;color:#0f0;padding:4px 8px;border-radius:4px;text-align:center}";
    html += ".sp-range{width:140px;margin:0 8px;vertical-align:middle}";
    html += ".sp-val{display:inline-block;width:45px;color:#0f0;font-weight:bold}";
    html += ".sp-apply{background:#0af;color:#000;border:none;padding:6px 20px;border-radius:4px;font-weight:bold;cursor:pointer;margin-top:8px}";
    html += ".sp-apply:hover{background:#0cf}";
    html += ".sp-row{margin:4px 0}";
    html += ".sp-saved{color:#0f0;font-size:0.8em;margin-left:10px;display:none}";
    html += "</style>";
    html += "</head><body><h1>🏊 Pool Controller</h1>";

    // System status
    html += "<div class='card'><h2>System</h2>";
    html += "<p>Mode: <span id='sys-mode'>" + String(manualMode ? "<span class='manual-badge'>🔧 MANUAL</span>" : "<span class='auto-badge'>🤖 AUTO</span>") + "</span></p>";
    html += "<p>Uptime: <span id='sys-uptime'>" + String(millis() / 1000 / 60) + "</span> min | ";
    html += "WiFi: <span id='sys-wifi'>" + String(WiFi.isConnected() ? "✅" : "❌") + "</span> | ";
    html += "MQTT: <span id='sys-mqtt'>" + String(mqttManager && mqttManager->isConnected() ? "✅" : "❌") + "</span> | ";
    html += "IP: " + (WiFi.isConnected() ? WiFi.localIP().toString() : String(AP_SSID)) + "</p>";
    html += "</div>";

    // Sensor readings
    html += "<div class='card'><h2>Sensors</h2>";
    html += "<p id='sensor-row'>";
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

    // Setpoints card — NEW
    float phSP = chemistryController ? chemistryController->getPHPID().getSetpoint() : cfg.phPID.setpoint;
    float orpSP = chemistryController ? chemistryController->getChlorinePID().getSetpoint() : cfg.chlorinePID.setpoint;
    html += "<div class='card'><h2>🎯 Chemistry Setpoints</h2>";
    html += "<div class='sp-row'><span class='sp-label'>pH Target:</span>";
    html += "<input type='range' class='sp-range' id='sp-ph-range' min='6.0' max='8.0' step='0.1' value='" + String(phSP, 1) + "' oninput=\"document.getElementById('sp-ph-val').textContent=this.value\">";
    html += "<span class='sp-val' id='sp-ph-val'>" + String(phSP, 1) + "</span> pH</div>";
    html += "<div class='sp-row'><span class='sp-label'>ORP Target:</span>";
    html += "<input type='range' class='sp-range' id='sp-orp-range' min='200' max='900' step='10' value='" + String((int)orpSP) + "' oninput=\"document.getElementById('sp-orp-val').textContent=this.value\">";
    html += "<span class='sp-val' id='sp-orp-val'>" + String((int)orpSP) + "</span> mV</div>";
    html += "<button class='sp-apply' id='sp-apply-btn' onclick=\"applySetpoints()\">Apply Setpoints</button>";
    html += "<span class='sp-saved' id='sp-saved'>✅ Saved!</span>";
    html += "<p style='font-size:0.7em;color:#888;margin-top:6px'>Changes take effect immediately in the PID controller.</p>";
    html += "</div>";

    // Manual mode toggle
    html += "<div class='card'><h2>Control Mode</h2>";
    html += "<p>";
    if (manualMode) {
        html += "<button class='mode-btn auto' onclick=\"fetch('/api/manual?mode=0').then(r=>r.json()).then(d=>location.reload())\">Return to AUTO Mode</button>";
    } else {
        html += "<button class='mode-btn' onclick=\"fetch('/api/manual?mode=1').then(r=>r.json()).then(d=>location.reload())\">Switch to MANUAL Mode</button>";
    }
    html += "</p><p style='font-size:0.8em;color:#888'>Manual mode disables automatic PID & filter pump control for testing.</p>";
    html += "</div>";

    // Pump controls
    html += "<div class='card'><h2>Pump Control</h2><p id='pump-row'>";
    if (filterPumpCtrl) {
        html += pumpButton("filter", "Filter", filterPumpCtrl->isOn(),
                          filterPumpCtrl->getLastOnDuration() / 60000,
                          filterPumpCtrl->getRuntimeMinutes()) + "<br>";
    }
    if (phPumpCtrl) {
        html += pumpButton("ph", "pH Pump", phPumpCtrl->isOn(),
                          phPumpCtrl->getLastOnDuration() / 60000,
                          phPumpCtrl->getRuntimeMinutes()) + "<br>";
    }
    if (chlorinePumpCtrl) {
        html += pumpButton("chlorine", "Chlorine", chlorinePumpCtrl->isOn(),
                          chlorinePumpCtrl->getLastOnDuration() / 60000,
                          chlorinePumpCtrl->getRuntimeMinutes()) + "<br>";
    }
    html += "</p></div>";

    // Individual relay test
    html += "<div class='card'><h2>Relay Test (Direct)</h2><p id='relay-row'>";
    for (int i = 0; i < KC868_A8_RELAY_COUNT; i++) {
        html += relayButton(i, relayManager.getRelayState(i));
    }
    html += "</p><p style='font-size:0.7em;color:#888'>Direct relay control — bypasses pump logic. Use for hardware testing.</p>";
    html += "</div>";

    // Chemistry status
    html += "<div class='card'><h2>Chemistry Status</h2>";
    html += "<p id='chem-row'>";
    if (chemistryController) {
        html += "pH Control: " + String(chemistryController->isPHEnabled() ? "✅ ON" : "❌ OFF");
        html += " | Chlorine Control: " + String(chemistryController->isChlorineEnabled() ? "✅ ON" : "❌ OFF");
    }
    html += "</p></div>";

    // Actions
    html += "<div class='card'><h2>Quick Actions</h2><p>";
    html += "<a href='/api/alloff' style='color:#f44;text-decoration:none'>🛑 Emergency All Off</a>";
    html += "</p></div>";

    // Auto-refresh script: poll /api every 5 seconds
    html += "<script>";
    html += "function fmtMin(m){return m+'m';}";
    html += "function applySetpoints(){";
    html += "var ph=document.getElementById('sp-ph-range').value;";
    html += "var orp=document.getElementById('sp-orp-range').value;";
    html += "var btn=document.getElementById('sp-apply-btn');";
    html += "btn.textContent='Saving...';btn.disabled=true;";
    html += "fetch('/api/setpoint?ph='+encodeURIComponent(ph)+'&orp='+encodeURIComponent(orp))";
    html += ".then(r=>r.json()).then(d=>{";
    html += "btn.textContent='Apply Setpoints';btn.disabled=false;";
    html += "var s=document.getElementById('sp-saved');s.style.display='inline';";
    html += "setTimeout(function(){s.style.display='none';},2500);";
    html += "}).catch(function(){btn.textContent='Apply Setpoints';btn.disabled=false;});";
    html += "}";
    html += "setInterval(function(){";
    html += "fetch('/api').then(r=>r.json()).then(d=>{";
    html += "var el=document.getElementById('val-ph');if(el)el.textContent=d.ph.toFixed(2);";
    html += "el=document.getElementById('val-orp');if(el)el.textContent=d.orp.toFixed(0);";
    html += "el=document.getElementById('val-water');if(el)el.textContent=d.water_temp.toFixed(1);";
    html += "el=document.getElementById('val-air');if(el)el.textContent=d.air_temp.toFixed(1);";
    html += "el=document.getElementById('val-pressure');if(el)el.textContent=d.filter_pressure.toFixed(2);";
    html += "el=document.getElementById('sys-uptime');if(el)el.textContent=Math.floor(d.uptime_ms/60000);";
    html += "el=document.getElementById('sys-wifi');if(el)el.textContent=d.wifi?'✅':'❌';";
    html += "el=document.getElementById('sys-mqtt');if(el)el.textContent=d.mqtt?'✅':'❌';";
    html += "el=document.getElementById('sys-mode');";
    html += "if(el)el.innerHTML=d.manual_mode?'<span class=\"manual-badge\">🔧 MANUAL</span>':'<span class=\"auto-badge\">🤖 AUTO</span>';";
    html += "// Update setpoint sliders from server";
    html += "if(d.ph_setpoint!==undefined){";
    html += "var r=document.getElementById('sp-ph-range');";
    html += "var v=document.getElementById('sp-ph-val');";
    html += "if(r&&parseFloat(r.value)!==d.ph_setpoint){r.value=d.ph_setpoint;if(v)v.textContent=d.ph_setpoint.toFixed(1);}";
    html += "}";
    html += "if(d.orp_setpoint!==undefined){";
    html += "var r=document.getElementById('sp-orp-range');";
    html += "var v=document.getElementById('sp-orp-val');";
    html += "if(r&&parseInt(r.value)!==d.orp_setpoint){r.value=d.orp_setpoint;if(v)v.textContent=d.orp_setpoint;}";
    html += "}";
    html += "['filter','ph','chlorine'].forEach(function(id){";
    html += "var b=document.getElementById('btn-'+id);";
    html += "var r=document.getElementById('run-'+id);";
    html += "if(b&&d.pumps&&d.pumps[id]){";
    html += "var p=d.pumps[id];var on=p.on;b.textContent=on?'ON':'OFF';";
    html += "b.className='pump-btn '+(on?'btn-on':'btn-off');";
    html += "if(r)r.textContent='('+(p.current_min?p.current_min:0)+'m / '+p.today_min+'m today)';}});";
    html += "if(d.relays)for(var i=0;i<d.relays.length;i++){";
    html += "var r=document.getElementById('rel-'+i);if(r){";
    html += "var on=d.relays[i];r.textContent='R'+i+': '+(on?'ON':'OFF');";
    html += "r.className='relay-btn '+(on?'btn-on':'btn-off');}}";
    html += "}).catch(function(){});";
    html += "},5000);";
    html += "</script>";

    html += "<p style='color:#666;font-size:0.75em'>Pool Controller v1.0.0 | ESP32 KC868-A8</p>";
    html += "</body></html>";
    webServer.send(200, "text/html", html);
}

void handleAPI() {
    AppConfig& cfg = configManager.get();
    StaticJsonDocument<1024> doc;
    doc["uptime_ms"] = millis();
    doc["wifi"] = WiFi.isConnected();
    doc["mqtt"] = mqttManager ? mqttManager->isConnected() : false;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["manual_mode"] = manualMode;
    doc["ph"] = sensorManager ? sensorManager->getPH() : 0;
    doc["orp"] = sensorManager ? sensorManager->getORP() : 0;
    doc["water_temp"] = sensorManager ? sensorManager->getWaterTemperature() : 0;
    doc["air_temp"] = sensorManager ? sensorManager->getAirTemperature() : 0;
    doc["filter_pressure"] = sensorManager ? sensorManager->getFilterPressure() : 0;

    // Setpoint values
    doc["ph_setpoint"] = chemistryController ? chemistryController->getPHPID().getSetpoint() : cfg.phPID.setpoint;
    doc["orp_setpoint"] = chemistryController ? chemistryController->getChlorinePID().getSetpoint() : cfg.chlorinePID.setpoint;

    // Pump states with runtime
    JsonObject pumps = doc.createNestedObject("pumps");
    if (filterPumpCtrl) {
        JsonObject f = pumps.createNestedObject("filter");
        f["on"] = filterPumpCtrl->isOn();
        f["current_min"] = filterPumpCtrl->getLastOnDuration() / 60000;
        f["today_min"] = filterPumpCtrl->getRuntimeMinutes();
    }
    if (phPumpCtrl) {
        JsonObject p = pumps.createNestedObject("ph");
        p["on"] = phPumpCtrl->isOn();
        p["current_min"] = phPumpCtrl->getLastOnDuration() / 60000;
        p["today_min"] = phPumpCtrl->getRuntimeMinutes();
    }
    if (chlorinePumpCtrl) {
        JsonObject c = pumps.createNestedObject("chlorine");
        c["on"] = chlorinePumpCtrl->isOn();
        c["current_min"] = chlorinePumpCtrl->getLastOnDuration() / 60000;
        c["today_min"] = chlorinePumpCtrl->getRuntimeMinutes();
    }

    // Relay states
    JsonArray relays = doc.createNestedArray("relays");
    for (int i = 0; i < KC868_A8_RELAY_COUNT; i++) {
        relays.add(relayManager.getRelayState(i));
    }

    String json;
    serializeJson(doc, json);
    webServer.send(200, "application/json", json);
}

void handleAPIRelaySet() {
    if (!webServer.hasArg("channel") || !webServer.hasArg("state")) {
        webServer.send(400, "application/json", "{\"error\":\"missing channel or state\"}");
        return;
    }
    int channel = webServer.arg("channel").toInt();
    bool state = (webServer.arg("state") == "1" || webServer.arg("state") == "true");

    if (channel < 0 || channel >= KC868_A8_RELAY_COUNT) {
        webServer.send(400, "application/json", "{\"error\":\"invalid channel\"}");
        return;
    }

    relayManager.setRelay((uint8_t)channel, state);
    log_i("Web: Relay %d -> %s", channel, state ? "ON" : "OFF");

    StaticJsonDocument<128> doc;
    doc["ok"] = true;
    doc["channel"] = channel;
    doc["state"] = state;
    String json;
    serializeJson(doc, json);
    webServer.send(200, "application/json", json);
}

void handleAPIPumpSet() {
    if (!webServer.hasArg("id") || !webServer.hasArg("state")) {
        webServer.send(400, "application/json", "{\"error\":\"missing id or state\"}");
        return;
    }
    String id = webServer.arg("id");
    bool state = (webServer.arg("state") == "1" || webServer.arg("state") == "true");

    PumpController* pump = nullptr;
    if (id == "filter") pump = filterPumpCtrl;
    else if (id == "ph") pump = phPumpCtrl;
    else if (id == "chlorine") pump = chlorinePumpCtrl;

    if (!pump) {
        webServer.send(400, "application/json", "{\"error\":\"unknown pump id\"}");
        return;
    }

    bool ok = state ? pump->turnOn() : pump->turnOff();
    log_i("Web: Pump '%s' -> %s (%s)", id.c_str(), state ? "ON" : "OFF", ok ? "ok" : "blocked");

    StaticJsonDocument<128> doc;
    doc["ok"] = ok;
    doc["pump"] = id;
    doc["state"] = state;
    doc["actual"] = pump->isOn();
    String json;
    serializeJson(doc, json);
    webServer.send(200, "application/json", json);
}

void handleAPIManualMode() {
    if (!webServer.hasArg("mode")) {
        webServer.send(400, "application/json", "{\"error\":\"missing mode\"}");
        return;
    }
    manualMode = (webServer.arg("mode") == "1" || webServer.arg("mode") == "true");

    if (!manualMode) {
        if (chemistryController) chemistryController->begin();
        log_i("Web: Switched to AUTO mode");
    } else {
        log_i("Web: Switched to MANUAL mode");
    }

    StaticJsonDocument<64> doc;
    doc["ok"] = true;
    doc["manual_mode"] = manualMode;
    String json;
    serializeJson(doc, json);
    webServer.send(200, "application/json", json);
}

void handleAPIAllOff() {
    relayManager.allOff();
    log_i("Web: Emergency all off!");

    StaticJsonDocument<64> doc;
    doc["ok"] = true;
    doc["msg"] = "all relays off";
    String json;
    serializeJson(doc, json);
    webServer.send(200, "application/json", json);
}

void handleAPISetpoint() {
    AppConfig& cfg = configManager.get();
    StaticJsonDocument<256> doc;
    bool changed = false;

    if (webServer.hasArg("ph")) {
        float ph = webServer.arg("ph").toFloat();
        if (ph >= 6.0f && ph <= 8.0f) {
            if (chemistryController) chemistryController->setPHSetpoint(ph);
            else cfg.phPID.setpoint = ph;
            doc["ph_setpoint"] = ph;
            changed = true;
            log_i("Web: pH setpoint -> %.1f", ph);
        } else {
            doc["ph_error"] = "must be 6.0-8.0";
        }
    }

    if (webServer.hasArg("orp")) {
        float orp = webServer.arg("orp").toFloat();
        if (orp >= 200.0f && orp <= 900.0f) {
            if (chemistryController) chemistryController->setORPSetpoint(orp);
            else cfg.chlorinePID.setpoint = orp;
            doc["orp_setpoint"] = orp;
            changed = true;
            log_i("Web: ORP setpoint -> %.0f mV", orp);
        } else {
            doc["orp_error"] = "must be 200-900";
        }
    }

    if (changed && chemistryController) {
        configManager.save();
    }

    doc["ok"] = changed;
    String json;
    serializeJson(doc, json);
    webServer.send(changed ? 200 : 400, "application/json", json);
}

void setupWebServer() {
    webServer.on("/", handleRoot);
    webServer.on("/api", handleAPI);
    webServer.on("/api/relay/set", handleAPIRelaySet);
    webServer.on("/api/pump/set", handleAPIPumpSet);
    webServer.on("/api/manual", handleAPIManualMode);
    webServer.on("/api/alloff", handleAPIAllOff);
    webServer.on("/api/setpoint", handleAPISetpoint);
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

    Wire.begin(4, 5);
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
    filterPumpCtrl = new PumpController(relayManager, fpCfg.relayChannel, "Filter Pump");
    filterPumpCtrl->begin();
    filterPumpLogic = new FilterPumpLogic(configManager, *filterPumpCtrl);
    filterPumpLogic->begin();

    filterPumpCtrl->addDependent(phPumpCtrl);
    filterPumpCtrl->addDependent(chlorinePumpCtrl);

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

    if (chemistryController && systemReady && !manualMode) {
        chemistryController->update();
    }

    if (filterPumpLogic && sensorManager && !manualMode) {
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
