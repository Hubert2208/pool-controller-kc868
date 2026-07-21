/**
 * Pool Controller for ESP32 (Kincony KC868-A8) v1.1.0
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include "config/ConfigManager.h"
#include "sensors/SensorManager.h"
#include "sensors/PHSensor.h"
#include "sensors/ORPSensor.h"
#include "actuators/RelayManager.h"
#include "actuators/PumpController.h"
#include "pid/PoolChemistryController.h"
#include "utils/FilterPumpLogic.h"
#include "utils/TimingUtils.h"
#include "mqtt/MQTTManager.h"

void setupWiFi(); void setupWebServer();
void handleMQTTCommand(const char* topic, const String& payload);
void applyPumpConfig();

ConfigManager configManager;
CalibrationData calibrationData;
RelayManager relayManager;
SensorManager* sensorManager = nullptr;
PumpController *phPumpCtrl = nullptr, *chlorinePumpCtrl = nullptr, *filterPumpCtrl = nullptr;
FilterPumpLogic* filterPumpLogic = nullptr;
PoolChemistryController* chemistryController = nullptr;
MQTTManager* mqttManager = nullptr;

static bool manualMode = false;
static unsigned long lastSensorPublish = 0;
static const unsigned long SENSOR_PUBLISH_INTERVAL = 30000;
static unsigned long wifiReconnectTime = 0;
static bool wifiConnected = false, systemReady = false;
static const char *WIFI_HOSTNAME = "pool-controller", *AP_SSID = "PoolController-AP";
static const IPAddress AP_IP(192,168,4,1), AP_GW(192,168,4,1), AP_SN(255,255,255,0);

#include <WebServer.h>
WebServer webServer(80);
// Calibration state machine + API handlers (shared state with main)
#include "calibration_wizard.cpp"

// ── HTML helpers ──
String pumpBtn(const char* id, const char* lb, bool st, unsigned long rm, unsigned long dm) {
    String c = st ? "btn-on" : "btn-off", t = st ? "ON" : "OFF";
    return "<span class='pl'>" + String(lb) + ":</span> <button class='pb " + c +
        "' onclick=\"f('/api/pump/set?id=" + String(id) + "&state=" + String(st?0:1) + "')\">" + t +
        "</button> <span class='rt'>(" + String(st?String(rm):"0") + "m/" + String(dm) + "m)</span>";
}
String rBtn(int ch, bool st) {
    String c = st ? "btn-on" : "btn-off", t = st ? "ON" : "OFF";
    return "<button class='rb " + c + "' onclick=\"f('/api/relay/set?channel=" + String(ch) +
        "&state=" + String(st?0:1) + "')\">R" + String(ch) + ":" + t + "</button>";
}
String ptRow(const char* id, const char* lb, int on, int off, float dmax) {
    String o = "<div class='pr'><span class='pl2'>" + String(lb) + ":</span>";
    o += "ON<input class='pi' id='pon-"+String(id)+"' value='"+String(on)+"' min='1' max='3600'>s";
    o += " OFF<input class='pi' id='pof-"+String(id)+"' value='"+String(off)+"' min='1' max='7200'>s";
    o += " day<input class='pi' style='width:60px' id='pday-"+String(id)+"' value='"+String((int)dmax)+"' min='1' max='1440'>m</div>";
    return o;
}

// ── Root page with calibration wizard ──
void handleRoot() {
    AppConfig& cfg = configManager.get();
    String h = "<!DOCTYPE html><html><head><title>Pool Ctrl</title><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><style>";
    h += "body{font:14px Arial;margin:8px;background:#1a1a2e;color:#eee}h1{color:#0f0;font-size:1.2em}h2{color:#0af;font-size:1em}";
    h += ".cd{background:#16213e;border-radius:8px;padding:10px;margin:6px 0}.v{font-size:1.2em;font-weight:bold;color:#0f0}.bd{color:#f44}.wn{color:#f80}";
    h += ".btn-on{background:#0a0;color:#fff;border:none;padding:5px 14px;border-radius:4px;font-weight:bold;cursor:pointer;margin:2px;min-width:56px}";
    h += ".btn-off{background:#444;color:#ccc;border:none;padding:5px 14px;border-radius:4px;font-weight:bold;cursor:pointer;margin:2px;min-width:56px}";
    h += ".btn-on:hover{background:#0c0}.btn-off:hover{background:#666}";
    h += ".bc{background:#0af;color:#000;border:none;padding:5px 16px;border-radius:4px;font-weight:bold;cursor:pointer;margin:2px 3px}";
    h += ".bc:hover{background:#0cf}.bc.dg{background:#f44;color:#fff}.bc.dg:hover{background:#f66}.bc:disabled{opacity:0.4}";
    h += ".rb{font-size:0.7em;padding:3px 6px;margin:1px;min-width:48px}.pl{display:inline-block;width:90px}.rt{color:#888;font-size:0.7em;margin-left:4px}";
    h += ".mb{background:#f80;color:#000;padding:2px 6px;border-radius:4px;font-weight:bold;font-size:0.8em}";
    h += ".ab{background:#0a0;color:#000;padding:2px 6px;border-radius:4px;font-weight:bold;font-size:0.8em}";
    h += ".mode-btn{background:#f80;color:#000;border:none;padding:5px 14px;border-radius:4px;font-weight:bold;cursor:pointer}.mode-btn.auto{background:#0a0}";
    h += ".sl{display:inline-block;width:75px;text-align:right;margin-right:6px}.sr{width:130px;margin:0 6px;vertical-align:middle}";
    h += ".sv{display:inline-block;width:40px;color:#0f0;font-weight:bold}.ap{background:#0af;color:#000;border:none;padding:5px 16px;border-radius:4px;font-weight:bold;cursor:pointer;margin-top:6px}";
    h += ".ap:hover{background:#0cf}.sr2{margin:3px 0}.ok{color:#0f0;font-size:0.75em;margin-left:8px;display:none}";
    h += ".pr{margin:3px 0;font-size:0.8em}.pl2{display:inline-block;width:65px}";
    h += ".pi{background:#0a0a2e;color:#0f0;border:1px solid #333;padding:1px 3px;border-radius:3px;width:50px;text-align:center}";
    h += ".cs{font-size:0.85em;margin:3px 0}.cl{font-size:1.1em;font-weight:bold;color:#0ff}.cg{color:#0f0;font-weight:bold}";
    h += "</style></head><body><h1>Pool Controller</h1>";
    // System
    h += "<div class='cd'><h2>System</h2><p>" + String(manualMode ? "<span class='mb'>MANUAL</span>" : "<span class='ab'>AUTO</span>");
    h += " Up:" + String(millis()/60000) + "m W:" + String(WiFi.isConnected()?"OK":"FAIL") + " M:" + String(mqttManager&&mqttManager->isConnected()?"OK":"FAIL");
    h += " IP:" + (WiFi.isConnected() ? WiFi.localIP().toString() : "AP") + "</p></div>";
    // Sensors
    h += "<div class='cd'><h2>Sensors</h2><p>";
    if (sensorManager) {
        h += "pH:<span class='v'>" + String(sensorManager->getPH(),2) + "</span> " + String(sensorManager->isPHConnected()?"OK":"<span class='bd'>sim</span>");
        h += " |ORP:<span class='v'>" + String(sensorManager->getORP(),0) + "</span>mV " + String(sensorManager->isORPConnected()?"OK":"<span class='bd'>sim</span>");
        h += " |W:<span class='v'>" + String(sensorManager->getWaterTemperature(),1) + "</span>C";
        h += " |A:<span class='v'>" + String(sensorManager->getAirTemperature(),1) + "</span>C |P:<span class='v'>" + String(sensorManager->getFilterPressure(),2) + "</span>bar";
    }
    h += "</p></div>";
    // Setpoints
    float psp = chemistryController ? chemistryController->getPHPID().getSetpoint() : cfg.phPID.setpoint;
    float osp = chemistryController ? chemistryController->getChlorinePID().getSetpoint() : cfg.chlorinePID.setpoint;
    h += "<div class='cd'><h2>Setpoints</h2>";
    h += "<div class='sr2'><span class='sl'>pH:</span><input class='sr' id='sph' min='6' max='8' step='0.1' value='"+String(psp,1)+"' oninput=\"ge('sphv').textContent=value\"><span class='sv' id='sphv'>"+String(psp,1)+"</span></div>";
    h += "<div class='sr2'><span class='sl'>ORP:</span><input class='sr' id='sop' min='200' max='900' step='10' value='"+String((int)osp)+"' oninput=\"ge('sopv').textContent=value\"><span class='sv' id='sopv'>"+String((int)osp)+"</span>mV</div>";
    h += "<button class='ap' onclick=\"var b=event.target;b.disabled=true;fetch('/api/setpoint?ph='+ge('sph').value+'&orp='+ge('sop').value).then(r=>r.json()).then(d=>{b.disabled=false;var s=ge('sps');s.style.display='inline';setTimeout(function(){s.style.display='none'},2000)})\">Apply</button><span class='ok' id='sps'>OK</span></div>";
    // Pump Timing
    h += "<div class='cd'><h2>Pump Timing</h2>";
    h += ptRow("ph","pH",cfg.phPump.minOnTimeSec,cfg.phPump.minOffTimeSec,cfg.phPump.maxDailyRuntimeMin);
    h += ptRow("cl","Cl",cfg.chlorinePump.minOnTimeSec,cfg.chlorinePump.minOffTimeSec,cfg.chlorinePump.maxDailyRuntimeMin);
    h += ptRow("f","Filter",cfg.filterPump.minOnTimeSec,cfg.filterPump.minOffTimeSec,cfg.filterPump.maxDailyRuntimeMin);
    h += "<div class='pr'><span class='pl2'>PreRun:</span>Filter <input class='pi' style='width:50px' id='ppr' value='"+String(cfg.filterPump.filterPreRunDelayMin)+"' min='1' max='60'>min before pH/Cl</div>";
    h += "<button class='ap' onclick=\"var b=event.target;b.disabled=true;fetch('/api/pump/timing?ph_on='+ge('pon-ph').value+'&ph_off='+ge('pof-ph').value+'&cl_on='+ge('pon-cl').value+'&cl_off='+ge('pof-cl').value+'&filter_on='+ge('pon-f').value+'&filter_off='+ge('pof-f').value+'&ph_day='+ge('pday-ph').value+'&cl_day='+ge('pday-cl').value+'&filter_day='+ge('pday-f').value+'&filter_prerun='+ge('ppr').value).then(r=>r.json()).then(d=>{b.disabled=false;var s=ge('pts');s.style.display='inline';setTimeout(function(){s.style.display='none'},2000)})\">Apply</button><span class='ok' id='pts'>OK</span></div>";
    // Calibration Card
    h += "<div class='cd'><h2>Calibration</h2>";
    h += "<div class='cs'>pH: slope=" + String(calibrationData.phSlope,3) + " pH/V | " + calDaysAgo(calibrationData.phCalibratedAt) + "</div>";
    h += "<div class='cs'>ORP: offset=" + String(calibrationData.orpOffset,1) + " mV | " + calDaysAgo(calibrationData.orpCalibratedAt) + "</div>";
    if (calState == CAL_IDLE) {
        h += "<p><button class='bc' onclick='cs()'>Calibrate pH</button> <button class='bc' onclick='co()'>Calibrate ORP</button> <button class='bc dg' onclick=\"if(confirm('Reset?'))fetch('/api/cal/reset').then(r=>r.json()).then(d=>location.reload())\">Reset</button></p>";
    }
    h += "<div id='cw'></div></div>";
    // Calibration JS
    h += "<script>function ge(id){return document.getElementById(id)}function f(u){fetch(u).then(r=>r.json()).then(d=>location.reload())}var cp=null;";
    h += "function cpoll(){fetch('/api/cal/status').then(r=>r.json()).then(d=>{var e=ge('cw');if(!e)return;";
    h += "if(d.state===0){e.innerHTML='';if(cp){clearInterval(cp);cp=null;}location.reload();}";
    h += "else if(d.state==1)e.innerHTML='<p><b>pH 7.00:</b> Place probe in pH 7.00 buffer...</p><p class=cl>'+d.voltageV.toFixed(4)+' V ('+d.voltageMV.toFixed(1)+' mV) | '+d.driftMV.toFixed(2)+' mV '+(d.stable?'<span class=cg>STABLE</span>':'<span class=wn>wait...</span>')+'</p><p><button class=bc '+(!d.stable?'disabled':'')+' onclick=cl7()>Lock pH 7.00</button> <button class=\"bc dg\" onclick=cc()>Cancel</button></p>';";
    h += "else if(d.state==2)e.innerHTML='<p><span class=cg>pH 7.00 locked:</span> '+d.lockedV.toFixed(4)+' V</p><p><button class=bc onclick=cn4()>Next: pH 4.01</button> <button class=\"bc dg\" onclick=cc()>Cancel</button></p>';";
    h += "else if(d.state==3)e.innerHTML='<p><b>pH 4.01:</b> Rinse probe, place in pH 4.01 buffer...</p><p class=cl>'+d.voltageV.toFixed(4)+' V ('+d.voltageMV.toFixed(1)+' mV) | '+d.driftMV.toFixed(2)+' mV '+(d.stable?'<span class=cg>STABLE</span>':'<span class=wn>wait...</span>')+'</p><p><button class=bc '+(!d.stable?'disabled':'')+' onclick=cl4()>Lock & Save</button> <button class=\"bc dg\" onclick=cc()>Cancel</button></p>';";
    h += "else if(d.state==4)e.innerHTML='<p class=cg>pH Done!</p><p>Slope: '+d.slope.toFixed(3)+' pH/V | Int: '+d.intercept.toFixed(3)+'</p><p><button class=bc onclick=location.reload()>Done</button></p>'; if(cp){clearInterval(cp);cp=null;}";
    h += "else if(d.state==5)e.innerHTML='<p><b>ORP:</b> Place probe in ORP cal solution ('+d.orpRefMV.toFixed(0)+' mV)</p><p class=cl>'+d.voltageMV.toFixed(1)+' mV | '+d.driftMV.toFixed(2)+' mV '+(d.stable?'<span class=cg>STABLE</span>':'<span class=wn>wait...</span>')+'</p><p><button class=bc '+(!d.stable?'disabled':'')+' onclick=clo()>Lock & Save</button> <button class=\"bc dg\" onclick=cc()>Cancel</button></p>';";
    h += "else if(d.state==6)e.innerHTML='<p class=cg>ORP Done!</p><p>Offset: '+d.offset.toFixed(1)+' mV</p><p><button class=bc onclick=location.reload()>Done</button></p>'; if(cp){clearInterval(cp);cp=null;}";
    h += "});}";
    h += "function cs(){fetch('/api/cal/start_ph').then(r=>r.json()).then(d=>{if(d.ok){cp=setInterval(cpoll,1000);cpoll();}})}";
    h += "function cl7(){fetch('/api/cal/lock_ph7').then(r=>r.json()).then(d=>cpoll())}";
    h += "function cn4(){fetch('/api/cal/lock_ph7').then(r=>r.json()).then(d=>cpoll())}";
    h += "function cl4(){fetch('/api/cal/lock_ph4').then(r=>r.json()).then(d=>cpoll())}";
    h += "function co(){fetch('/api/cal/start_orp').then(r=>r.json()).then(d=>{if(d.ok){cp=setInterval(cpoll,1000);cpoll();}})}";
    h += "function clo(){var r=prompt('ORP cal solution value (mV):','220');if(r){fetch('/api/cal/lock_orp?ref='+encodeURIComponent(r)).then(r=>r.json()).then(d=>cpoll())}}";
    h += "function cc(){fetch('/api/cal/reset?cancel=1').then(r=>r.json()).then(d=>{if(cp)clearInterval(cp);cp=null;location.reload()})}</script>";
    // Control, Pumps, Relays
    h += "<div class='cd'><h2>Control</h2><p>";
    if (manualMode) h += "<button class='mode-btn auto' onclick=\"f('/api/manual?mode=0')\">AUTO</button>";
    else h += "<button class='mode-btn' onclick=\"f('/api/manual?mode=1')\">MANUAL</button>";
    h += "</p></div><div class='cd'><h2>Pumps</h2><p>";
    if (filterPumpCtrl) h += pumpBtn("filter","Filter",filterPumpCtrl->isOn(),filterPumpCtrl->getLastOnDuration()/60000,filterPumpCtrl->getRuntimeMinutes())+"<br>";
    if (phPumpCtrl) h += pumpBtn("ph","pH",phPumpCtrl->isOn(),phPumpCtrl->getLastOnDuration()/60000,phPumpCtrl->getRuntimeMinutes())+"<br>";
    if (chlorinePumpCtrl) h += pumpBtn("chlorine","Cl",chlorinePumpCtrl->isOn(),chlorinePumpCtrl->getLastOnDuration()/60000,chlorinePumpCtrl->getRuntimeMinutes())+"<br>";
    h += "</p></div><div class='cd'><h2>Relays</h2><p>";
    for (int i = 0; i < KC868_A8_RELAY_COUNT; i++) h += rBtn(i, relayManager.getRelayState(i));
    h += "</p></div><div class='cd'><h2>Chem</h2><p>";
    if (chemistryController) { h += "pH:"+String(chemistryController->isPHEnabled()?"ON":"OFF")+" Cl:"+String(chemistryController->isChlorineEnabled()?"ON":"OFF"); }
    h += "</p></div><div class='cd'><h2>Quick</h2><p><a href='/api/alloff' style='color:#f44'>Emergency All Off</a></p></div>";
    h += "<p style='color:#555;font-size:0.7em'>Pool Controller v1.1.0</p></body></html>";
    webServer.send(200, "text/html", h);
}

// ── Standard API ──
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
    if (filterPumpCtrl) { JsonObject f = pumps.createNestedObject("filter"); f["on"] = filterPumpCtrl->isOn(); f["today_min"] = filterPumpCtrl->getRuntimeMinutes(); }
    if (phPumpCtrl) { JsonObject p = pumps.createNestedObject("ph"); p["on"] = phPumpCtrl->isOn(); p["today_min"] = phPumpCtrl->getRuntimeMinutes(); }
    if (chlorinePumpCtrl) { JsonObject c = pumps.createNestedObject("chlorine"); c["on"] = chlorinePumpCtrl->isOn(); c["today_min"] = chlorinePumpCtrl->getRuntimeMinutes(); }
    JsonArray relays = doc.createNestedArray("relays");
    for (int i = 0; i < KC868_A8_RELAY_COUNT; i++) relays.add(relayManager.getRelayState(i));
    String json; serializeJson(doc, json); webServer.send(200, "application/json", json);
}

void handleAPIRelaySet() {
    if (!webServer.hasArg("channel") || !webServer.hasArg("state")) { webServer.send(400,"application/json","{\"error\":\"missing args\"}"); return; }
    int ch = webServer.arg("channel").toInt(); bool st = (webServer.arg("state") == "1" || webServer.arg("state") == "true");
    if (ch < 0 || ch >= KC868_A8_RELAY_COUNT) { webServer.send(400,"application/json","{\"error\":\"bad channel\"}"); return; }
    relayManager.setRelay((uint8_t)ch, st); webServer.send(200,"application/json","{\"ok\":true}");
}

void handleAPIPumpSet() {
    if (!webServer.hasArg("id") || !webServer.hasArg("state")) { webServer.send(400,"application/json","{\"error\":\"missing args\"}"); return; }
    String id = webServer.arg("id"); bool st = (webServer.arg("state") == "1" || webServer.arg("state") == "true");
    PumpController* p = nullptr;
    if (id == "filter") p = filterPumpCtrl; else if (id == "ph") p = phPumpCtrl; else if (id == "chlorine") p = chlorinePumpCtrl;
    if (!p) { webServer.send(400,"application/json","{\"error\":\"bad id\"}"); return; }
    bool ok = st ? p->turnOn() : p->turnOff();
    webServer.send(200,"application/json","{\"ok\":" + String(ok) + ",\"pump\":\"" + id + "\"}");
}

void handleAPIManualMode() {
    if (!webServer.hasArg("mode")) { webServer.send(400,"application/json","{\"error\":\"missing mode\"}"); return; }
    manualMode = (webServer.arg("mode") == "1" || webServer.arg("mode") == "true");
    if (!manualMode && chemistryController) chemistryController->begin();
    webServer.send(200,"application/json","{\"ok\":true}");
}

void handleAPIAllOff() { relayManager.allOff(); webServer.send(200,"application/json","{\"ok\":true}"); }

void handleAPISetpoint() {
    AppConfig& cfg = configManager.get(); bool changed = false;
    if (webServer.hasArg("ph")) { float ph = webServer.arg("ph").toFloat(); if (ph >= 6.0f && ph <= 8.0f) { if (chemistryController) chemistryController->setPHSetpoint(ph); else cfg.phPID.setpoint = ph; changed = true; } }
    if (webServer.hasArg("orp")) { float orp = webServer.arg("orp").toFloat(); if (orp >= 200.0f && orp <= 900.0f) { if (chemistryController) chemistryController->setORPSetpoint(orp); else cfg.chlorinePID.setpoint = orp; changed = true; } }
    if (changed && chemistryController) configManager.save();
    webServer.send(changed ? 200 : 400, "application/json", "{\"ok\":" + String(changed) + "}");
}

void handleAPIPumpTiming() {
    AppConfig& cfg = configManager.get(); bool changed = false;
    auto at = [](int v, int& t, int mn, int mx)->bool { if (v>=mn&&v<=mx) { t=v; return true; } return false; };
    if (webServer.hasArg("ph_on")) { if(at(webServer.arg("ph_on").toInt(),cfg.phPump.minOnTimeSec,1,3600))changed=true; }
    if (webServer.hasArg("ph_off")) { if(at(webServer.arg("ph_off").toInt(),cfg.phPump.minOffTimeSec,1,7200))changed=true; }
    if (webServer.hasArg("cl_on")) { if(at(webServer.arg("cl_on").toInt(),cfg.chlorinePump.minOnTimeSec,1,3600))changed=true; }
    if (webServer.hasArg("cl_off")) { if(at(webServer.arg("cl_off").toInt(),cfg.chlorinePump.minOffTimeSec,1,7200))changed=true; }
    if (webServer.hasArg("filter_on")) { if(at(webServer.arg("filter_on").toInt(),cfg.filterPump.minOnTimeSec,1,3600))changed=true; }
    if (webServer.hasArg("filter_off")) { if(at(webServer.arg("filter_off").toInt(),cfg.filterPump.minOffTimeSec,1,7200))changed=true; }
    auto ad = [](float v, float& t, float mn, float mx)->bool { if (v>=mn&&v<=mx) { t=v; return true; } return false; };
    if (webServer.hasArg("ph_day")) { if(ad(webServer.arg("ph_day").toFloat(),cfg.phPump.maxDailyRuntimeMin,1,1440))changed=true; }
    if (webServer.hasArg("cl_day")) { if(ad(webServer.arg("cl_day").toFloat(),cfg.chlorinePump.maxDailyRuntimeMin,1,1440))changed=true; }
    if (webServer.hasArg("filter_day")) { if(ad(webServer.arg("filter_day").toFloat(),cfg.filterPump.maxDailyRuntimeMin,1,1440))changed=true; }
    if (webServer.hasArg("filter_prerun")) { if(at(webServer.arg("filter_prerun").toInt(),cfg.filterPump.filterPreRunDelayMin,1,60))changed=true; }
    if (changed) { configManager.save(); applyPumpConfig(); }
    webServer.send(changed ? 200 : 400, "application/json", "{\"ok\":" + String(changed) + "}");
}

void setupWebServer() {
    webServer.on("/", handleRoot); webServer.on("/api", handleAPI);
    webServer.on("/api/relay/set", handleAPIRelaySet); webServer.on("/api/pump/set", handleAPIPumpSet);
    webServer.on("/api/manual", handleAPIManualMode); webServer.on("/api/alloff", handleAPIAllOff);
    webServer.on("/api/setpoint", handleAPISetpoint); webServer.on("/api/pump/timing", handleAPIPumpTiming);
    webServer.on("/api/cal/status", handleCalStatus); webServer.on("/api/cal/start_ph", handleCalStartPH);
    webServer.on("/api/cal/lock_ph7", handleCalLockPH7); webServer.on("/api/cal/start_ph4", handleCalStartPH4);
    webServer.on("/api/cal/lock_ph4", handleCalLockPH4); webServer.on("/api/cal/start_orp", handleCalStartORP);
    webServer.on("/api/cal/lock_orp", handleCalLockORP); webServer.on("/api/cal/reset", handleCalReset);
    webServer.begin();
}

void setupWiFi() {
    AppConfig& cfg = configManager.get();
    String h = cfg.wifi.hostname; if (h.length() == 0) h = WIFI_HOSTNAME;
    WiFi.setHostname(h.c_str()); WiFi.mode(WIFI_MODE_STA);
    if (cfg.wifi.ssid.length() > 0) {
        WiFi.begin(cfg.wifi.ssid.c_str(), cfg.wifi.password.c_str());
        for (int a = 0; WiFi.status() != WL_CONNECTED && a < 40; a++) delay(500);
        if (WiFi.status() == WL_CONNECTED) { wifiConnected = true; return; }
    }
    if (cfg.wifi.fallbackAP) {
        String s = cfg.wifi.apSSID; if (s.length() == 0) s = AP_SSID;
        WiFi.mode(WIFI_MODE_AP); WiFi.softAPConfig(AP_IP, AP_GW, AP_SN);
        WiFi.softAP(s.c_str(), "12345678");
    }
}

void maintainWiFi() {
    if (WiFi.status() != WL_CONNECTED && !wifiConnected) {
        if (millis() - wifiReconnectTime > 60000) {
            wifiReconnectTime = millis(); AppConfig& cfg = configManager.get();
            if (cfg.wifi.ssid.length() > 0) WiFi.reconnect(); else setupWiFi();
        }
    } else if (WiFi.status() == WL_CONNECTED) wifiConnected = true;
}

void applyPumpConfig() {
    AppConfig& cfg = configManager.get();
    if (phPumpCtrl) { phPumpCtrl->setMinOnTime((unsigned long)(cfg.phPump.minOnTimeSec*1000)); phPumpCtrl->setMinOffTime((unsigned long)(cfg.phPump.minOffTimeSec*1000)); phPumpCtrl->setFilterPreRunDelay((unsigned long)(cfg.filterPump.filterPreRunDelayMin*60000)); }
    if (chlorinePumpCtrl) { chlorinePumpCtrl->setMinOnTime((unsigned long)(cfg.chlorinePump.minOnTimeSec*1000)); chlorinePumpCtrl->setMinOffTime((unsigned long)(cfg.chlorinePump.minOffTimeSec*1000)); chlorinePumpCtrl->setFilterPreRunDelay((unsigned long)(cfg.filterPump.filterPreRunDelayMin*60000)); }
    if (filterPumpCtrl) { filterPumpCtrl->setMinOnTime((unsigned long)(cfg.filterPump.minOnTimeSec*1000)); filterPumpCtrl->setMinOffTime((unsigned long)(cfg.filterPump.minOffTimeSec*1000)); }
}

void handleMQTTCommand(const char* topic, const String& payload) {
    String t = String(topic); AppConfig& cfg = configManager.get(); String b = cfg.mqtt.baseTopic + "/command/";
    if (t == b+"ph_setpoint") { float v = payload.toFloat(); if (v>=6.0&&v<=8.0&&chemistryController) chemistryController->setPHSetpoint(v); }
    else if (t == b+"orp_setpoint") { float v = payload.toFloat(); if (v>=200&&v<=900&&chemistryController) chemistryController->setORPSetpoint(v); }
    else if (t == b+"ph_set_enabled") { if (chemistryController) chemistryController->setPHEnabled(payload=="true"||payload=="1"||payload=="ON"); }
    else if (t == b+"cl_set_enabled") { if (chemistryController) chemistryController->setChlorineEnabled(payload=="true"||payload=="1"||payload=="ON"); }
    else if (t == b+"all_off") relayManager.allOff();
    else if (t == b+"reset_config") { if (payload=="confirm") { configManager.get() = AppConfig(); configManager.save(); delay(1000); ESP.restart(); } }
    else if (t == b+"restart") { if (payload=="confirm") { delay(500); ESP.restart(); } }
    else if (t == b+"ph_pump_min_on")  { int v = payload.toInt(); if(v>=1&&v<=3600){cfg.phPump.minOnTimeSec=v;configManager.save();applyPumpConfig();} }
    else if (t == b+"ph_pump_min_off") { int v = payload.toInt(); if(v>=1&&v<=7200){cfg.phPump.minOffTimeSec=v;configManager.save();applyPumpConfig();} }
    else if (t == b+"cl_pump_min_on")  { int v = payload.toInt(); if(v>=1&&v<=3600){cfg.chlorinePump.minOnTimeSec=v;configManager.save();applyPumpConfig();} }
    else if (t == b+"cl_pump_min_off") { int v = payload.toInt(); if(v>=1&&v<=7200){cfg.chlorinePump.minOffTimeSec=v;configManager.save();applyPumpConfig();} }
    else if (t == b+"filter_pump_min_on")  { int v = payload.toInt(); if(v>=1&&v<=3600){cfg.filterPump.minOnTimeSec=v;configManager.save();applyPumpConfig();} }
    else if (t == b+"filter_pump_min_off") { int v = payload.toInt(); if(v>=1&&v<=7200){cfg.filterPump.minOffTimeSec=v;configManager.save();applyPumpConfig();} }
    else if (t == b+"ph_pump_max_day")  { float v = payload.toFloat(); if(v>=1&&v<=1440){cfg.phPump.maxDailyRuntimeMin=v;configManager.save();} }
    else if (t == b+"cl_pump_max_day")  { float v = payload.toFloat(); if(v>=1&&v<=1440){cfg.chlorinePump.maxDailyRuntimeMin=v;configManager.save();} }
    else if (t == b+"filter_pump_max_day")  { float v = payload.toFloat(); if(v>=1&&v<=1440){cfg.filterPump.maxDailyRuntimeMin=v;configManager.save();} }
    else if (t == b+"filter_prerun_delay") { int v = payload.toInt(); if(v>=1&&v<=60){cfg.filterPump.filterPreRunDelayMin=v;configManager.save();applyPumpConfig();} }
    else if (t == b+"cal_ph_start") { PHSensor* ph = sensorManager?sensorManager->getPHSensor():nullptr; if(ph&&ph->isConnected()){calState=CAL_PH_WAIT_7;calResetWindow();} }
    else if (t == b+"cal_ph_lock7") { PHSensor* ph = sensorManager?sensorManager->getPHSensor():nullptr; if(ph&&calState==CAL_PH_WAIT_7){calPH7Voltage=ph->readRawVoltage();calState=CAL_PH_LOCKED_7;} }
    else if (t == b+"cal_ph_lock4") { PHSensor* ph = sensorManager?sensorManager->getPHSensor():nullptr; if(ph){if(calState==CAL_PH_LOCKED_7){calState=CAL_PH_WAIT_4;calResetWindow();}else if(calState==CAL_PH_WAIT_4){calPH4Voltage=ph->readRawVoltage();ph->setCalibration(calPH7Voltage,calPH4Voltage);ph->saveCalibration(calibrationData);calibrationData.save();calState=CAL_IDLE;}} }
    else if (t == b+"cal_orp_start") { ORPSensor* orp = sensorManager?sensorManager->getORPSensor():nullptr; if(orp&&orp->isConnected()){calState=CAL_ORP_WAIT;calResetWindow();} }
    else if (t == b+"cal_orp_lock") { ORPSensor* orp = sensorManager?sensorManager->getORPSensor():nullptr; if(orp&&calState==CAL_ORP_WAIT){float r=payload.toFloat();if(r>=100&&r<=900)calORPRequiredMV=r;orp->setCalibration(calORPRequiredMV);orp->saveCalibration(calibrationData);calibrationData.save();calState=CAL_IDLE;} }
    else if (t == b+"cal_reset") { calibrationData=CalibrationData();calibrationData.save();PHSensor*ph=sensorManager?sensorManager->getPHSensor():nullptr;ORPSensor*or=sensorManager?sensorManager->getORPSensor():nullptr;if(ph)ph->loadCalibration(calibrationData);if(or)or->loadCalibration(calibrationData); }
    else if (t == b+"pump_timing") { StaticJsonDocument<256> doc; if(deserializeJson(doc,payload)==DeserializationError::Ok){if(doc["ph"]["minOn"].is<int>())cfg.phPump.minOnTimeSec=doc["ph"]["minOn"];if(doc["ph"]["minOff"].is<int>())cfg.phPump.minOffTimeSec=doc["ph"]["minOff"];if(doc["chlorine"]["minOn"].is<int>())cfg.chlorinePump.minOnTimeSec=doc["chlorine"]["minOn"];if(doc["chlorine"]["minOff"].is<int>())cfg.chlorinePump.minOffTimeSec=doc["chlorine"]["minOff"];if(doc["filter"]["minOn"].is<int>())cfg.filterPump.minOnTimeSec=doc["filter"]["minOn"];if(doc["filter"]["minOff"].is<int>())cfg.filterPump.minOffTimeSec=doc["filter"]["minOff"];if(doc["filter"]["preRunDelay"].is<int>())cfg.filterPump.filterPreRunDelayMin=doc["filter"]["preRunDelay"];configManager.save();applyPumpConfig();} }
}

void feedWatchdog() { esp_task_wdt_reset(); }

void setup() {
    Serial.begin(115200); delay(1000);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
    esp_task_wdt_config_t t = { .timeout_ms = 30000, .trigger_panic = true }; esp_task_wdt_init(&t);
#else
    esp_task_wdt_init(30, true);
#endif
    esp_task_wdt_add(NULL);
    if (!configManager.begin()) log_w("Config defaults");
    calibrationData.load();
    Wire.begin(4, 5); relayManager.begin(); relayManager.allOff();
    sensorManager = new SensorManager(configManager); sensorManager->begin();
    PHSensor* ph = sensorManager->getPHSensor(); ORPSensor* orp = sensorManager->getORPSensor();
    if (ph) ph->loadCalibration(calibrationData); if (orp) orp->loadCalibration(calibrationData);
    AppConfig& cfg = configManager.get();
    phPumpCtrl = new PumpController(relayManager, cfg.phPump.relayChannel, "pH Pump"); phPumpCtrl->begin();
    chlorinePumpCtrl = new PumpController(relayManager, cfg.chlorinePump.relayChannel, "Chlorine Pump"); chlorinePumpCtrl->begin();
    filterPumpCtrl = new PumpController(relayManager, cfg.filterPump.relayChannel, "Filter Pump"); filterPumpCtrl->begin();
    applyPumpConfig();
    filterPumpLogic = new FilterPumpLogic(configManager, *filterPumpCtrl); filterPumpLogic->begin();
    filterPumpCtrl->addDependent(phPumpCtrl); filterPumpCtrl->addDependent(chlorinePumpCtrl);
    chemistryController = new PoolChemistryController(configManager, *sensorManager, *phPumpCtrl, *chlorinePumpCtrl); chemistryController->begin();
    setupWiFi(); setupWebServer();
    if (wifiConnected) { initNTP(7200, 3600); waitForNTPSync(15); }
    mqttManager = new MQTTManager(configManager); mqttManager->begin(); mqttManager->setCommandCallback(handleMQTTCommand);
    systemReady = true; log_i("System ready (%lu ms)", millis());
}

void loop() {
    unsigned long ls = millis();
    feedWatchdog(); maintainWiFi();
    if (mqttManager) mqttManager->loop(); webServer.handleClient();
    if (sensorManager) sensorManager->update();
    if (chemistryController && systemReady && !manualMode) chemistryController->update();
    if (filterPumpLogic && sensorManager && !manualMode) filterPumpLogic->update(sensorManager->getWaterTemperature());
    AppConfig& cfg = configManager.get();
    if (mqttManager && mqttManager->isConnected() && systemReady) {
        unsigned long now = millis();
        if (now - lastSensorPublish >= SENSOR_PUBLISH_INTERVAL) {
            lastSensorPublish = now;
            if (sensorManager) mqttManager->publish("ph", String(sensorManager->getPH(),2), false);
            if (sensorManager) mqttManager->publish("sensors", sensorManager->getAllStateJSON(), false);
            if (chemistryController) mqttManager->publish("chemistry", chemistryController->getStateJSON(), false);
            if (filterPumpLogic) mqttManager->publish("filter", filterPumpLogic->getStateJSON(), false);
            StaticJsonDocument<256> pd;
            if (phPumpCtrl) { JsonObject p=pd.createNestedObject("ph_pump");p["on"]=phPumpCtrl->isOn();p["runtime_today_min"]=phPumpCtrl->getRuntimeMinutes(); }
            if (chlorinePumpCtrl) { JsonObject c=pd.createNestedObject("chlorine_pump");c["on"]=chlorinePumpCtrl->isOn();c["runtime_today_min"]=chlorinePumpCtrl->getRuntimeMinutes(); }
            if (filterPumpCtrl) { JsonObject f=pd.createNestedObject("filter_pump");f["on"]=filterPumpCtrl->isOn();f["runtime_today_min"]=filterPumpCtrl->getRuntimeMinutes(); }
            String ps; serializeJson(pd,ps); mqttManager->publish("pumps",ps,false);
            StaticJsonDocument<192> td;
            JsonObject tp=td.createNestedObject("ph");tp["minOn"]=cfg.phPump.minOnTimeSec;tp["minOff"]=cfg.phPump.minOffTimeSec;tp["maxDailyMin"]=cfg.phPump.maxDailyRuntimeMin;
            JsonObject tc=td.createNestedObject("chlorine");tc["minOn"]=cfg.chlorinePump.minOnTimeSec;tc["minOff"]=cfg.chlorinePump.minOffTimeSec;tc["maxDailyMin"]=cfg.chlorinePump.maxDailyRuntimeMin;
            JsonObject tf=td.createNestedObject("filter");tf["minOn"]=cfg.filterPump.minOnTimeSec;tf["minOff"]=cfg.filterPump.minOffTimeSec;tf["maxDailyMin"]=cfg.filterPump.maxDailyRuntimeMin;tf["preRunDelay"]=cfg.filterPump.filterPreRunDelayMin;
            String tj; serializeJson(td,tj); mqttManager->publish("pump_config",tj,false);
            StaticJsonDocument<256> cd;
            JsonObject phc=cd.createNestedObject("ph");phc["slope"]=calibrationData.phSlope;phc["intercept"]=calibrationData.phIntercept;
            if(calibrationData.phCalibratedAt>0)phc["calibratedAt"]=calibrationData.phCalibratedAt;
            JsonObject orpc=cd.createNestedObject("orp");orpc["offset"]=calibrationData.orpOffset;
            if(calibrationData.orpCalibratedAt>0)orpc["calibratedAt"]=calibrationData.orpCalibratedAt;
            String cj; serializeJson(cd,cj); mqttManager->publish("calibration",cj,false);
        }
    }
    unsigned long el = millis() - ls;
    int d = cfg.loopDelayMs - (int)el;
    if (d > 0) delay(d);
    else if (d < -100) { static unsigned long lw=0; if(millis()-lw>60000){log_w("Loop overrun: %lu ms",el);lw=millis();} }
}
