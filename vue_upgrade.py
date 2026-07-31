#!/usr/bin/env python3
"""Replace handleRoot() in main.cpp with Vue.js 3 SPA version."""
import re

with open('/tmp/pool-controller-kc868/src/main.cpp', 'r') as f:
    content = f.read()

# Find old handleRoot() function — everything between 'void handleRoot() {' and the closing '}' before 'void handleAPI()'
pattern = r'void handleRoot\(\) \{.*?\n\}\n\nvoid handleAPI\(\)'
match = re.search(pattern, content, re.DOTALL)
if not match:
    print("ERROR: Could not find handleRoot() function")
    exit(1)

print(f"Found handleRoot() at positions {match.start()}-{match.end()}")
print(f"Old function length: {len(match.group())} chars")

# New Vue.js-based handleRoot() function
new_func = r'''void handleRoot() {
    AppConfig& cfg = configManager.get();
    String html = "<!DOCTYPE html><html><head><title>Pool Controller</title>";
    html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<meta http-equiv='refresh' content='30'>";
    html += "<script src='https://unpkg.com/vue@3/dist/vue.global.prod.js'></script>";
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
    // ── Header ──
    html += "<div class='card'><h1> Pool Controller</h1>";
    html += "<p><span class='status-dot' :class=\"data.wifi?'dot-ok':'dot-bad'\"></span>{{ data.wifi?'WiFi ✅':'WiFi ❌' }} | ";
    html += "<span class='status-dot' :class=\"data.mqtt?'dot-ok':'dot-bad'\"></span>{{ data.mqtt?'MQTT ✅':'MQTT ❌' }} | ";
    html += "IP: {{ data.ip || '—' }} | <span class='status-dot' :class=\"data.manual_mode?'dot-bad':'dot-ok'\"></span>{{ data.manual_mode?'MANUAL':'AUTO' }}</p>";
    html += "</div>";
    // ── System / Sensors ──
    html += "<div class='card'><h2>System</h2>";
    html += "<p>Uptime: {{ Math.floor(data.uptime_ms/60000) }} min</p>";
    html += "<p>Free Heap: {{ data.free_heap }} bytes</p>";
    html += "</div>";
    html += "<div class='card'><h2>Sensors</h2><p>";
    html += "pH: <span class='value'>{{ formatNum(data.ph, 2) }}</span> pH | ";
    html += "ORP: <span class='value'>{{ formatNum(data.orp, 0) }}</span> mV | ";
    html += "Water: <span class='value'>{{ formatNum(data.water_temp, 1) }}</span>°C | ";
    html += "Air: <span class='value'>{{ formatNum(data.air_temp, 1) }}</span>°C | ";
    html += "Pressure: <span class='value'>{{ formatNum(data.filter_pressure, 2) }}</span> bar";
    html += "</p></div>";
    // ── Chemistry Setpoints ──
    html += "<div class='card'><h2>Chemistry Setpoints</h2>";
    html += "<div class='sp-row'><span class='sp-label'>pH Target:</span>";
    html += "<input type='range' class='sp-range' min='6.0' max='8.0' step='0.1' v-model.number='setpoints.ph'>";
    html += "<span class='sp-val'>{{ setpoints.ph.toFixed(1) }}</span> pH</div>";
    html += "<div class='sp-row'><span class='sp-label'>ORP Target:</span>";
    html += "<input type='range' class='sp-range' min='200' max='900' step='10' v-model.number='setpoints.orp'>";
    html += "<span class='sp-val'>{{ setpoints.orp }} mV</span></div>";
    html += "<button class='sp-apply' @click='applySetpoints'>Apply</button>";
    html += "<span class='sp-saved' v-show='savedMsg'>{{ savedMsg }}</span></div>";
    // ── Control Mode ──
    html += "<div class='card'><h2>Control Mode</h2><p>";
    html += "<button class='btn' :class=\"data.manual_mode?'btn-off':'btn-on'\" @click='setMode(false)>AUTO Mode</button> ";
    html += "<button class='btn' :class=\"data.manual_mode?'btn-on':'btn-off'\" @click='setMode(true>MANUAL Mode</button>";
    html += "</p></div>";
    // ── Pump Control ──
    html += "<div class='card'><h2>Pump Control</h2><p>";
    html += "<div v-if='pumps.filter'><button class='btn' :class=\"pumps.filter.on?'btn-on':'btn-off'\" @click='togglePump('filter\')">Filter {{ pumps.filter.on?'ON':'OFF' }}</button> ";
    html += "<span class='runtime'>({{ pumps.filter.current_min }}m / {{ pumps.filter.today_min }}m today)</span></div>";
    html += "<div v-if='pumps.ph'><button class='btn' :class=\"pumps.ph.on?'btn-on':'btn-off'\" @click='togglePump('ph\">pH {{ pumps.ph.on?'ON':'OFF' }}</button> ";
    html += "<span class='runtime'>({{ pumps.ph.current_min }}m / {{ pumps.ph.today_min }}m today)</span></div>";
    html += "<div v-if='pumps.chlorine'><button class='btn' :class=\"pumps.chlorine.on?'btn-on':'btn-off'\" @click='togglePump('chlorine\">Cl {{ pumps.chlorine.on?'ON':'OFF' }}</button> ";
    html += "<span class='runtime'>({{ pumps.chlorine.current_min }}m / {{ pumps.chlorine.today_min }}m today)</span></div>";
    html += "</p></div>";
    // ── Filter Pump Runtime ──
    html += "<div class='card'><h2>Filter Pump Runtime</h2>";
    html += "<p style='font-size:0.8em;color:#aaa'>Required: T/2 x 60 min</p><p>";
    html += "<div class='rt-row'><span class='rt-label'>Required:</span><span class='rt-value'>{{ requiredRuntime }} min</span></div>";
    html += "<div class='rt-row'><span class='rt-label'>Today:</span><span class='rt-value'>{{ actualRuntime }} min</span></div>";
    html += "<div class='rt-row'><span class='rt-label'>Progress:</span>";
    html += "<span class='rt-value' :class=\"progressPct >= 100 ? '' : 'rt-bad'\">{{ progressPct }}%</span></div>";
    html += "<div class='progress-bg'><div class='progress-fill' :class=\"progressPct >= 100 ? 'ok' : 'low'\" :style=\"{ width: progressPct + '%', minWidth: actualRuntime > 0 ? '4px' : '0' }\"></div></div>";
    html += "</p></div>";
    // ── Relay Test ──
    html += "<div class='card'><h2>Relay Test</h2><p>";
    html += "<div v-for='(state, i) in relays' :key='i'>";
    html += "<button class='btn' :class=\"state?'btn-on':'btn-off'\" @click='toggleRelay(i)\">R{{ i }} {{ state?'ON':'OFF' }}</button></div>";
    html += "</p></div>";
    // ── Chemistry Status ──
    html += "<div class='card'><h2>Chemistry Status</h2><p>";
    html += "pH Control: <span :class=\"phEnabled?'value':'bad'\">{{ phEnabled?'ON':'OFF' }}</span> | ";
    html += "Chlorine Control: <span :class=\"clEnabled?'value':'bad'\">{{ clEnabled?'ON':'OFF' }}</span></p></div>";
    // ── Quick Actions ──
    html += "<div class='card'><h2>Quick Actions</h2><p>";
    html += "<a href='/api/alloff' class='bad' style='text-decoration:none'>All Off</a></p></div>";
    html += "<p style='color:#444;font-size:0.7em'>v2.0 Vue SPA | ESP32 KC868-A8</p>";
    html += "</div>";
    // ── Vue App ──
    html += "<script>";
    html += "const api = Vue.createApp({";
    html += "data(){return {";
    html += "data:null,";
    html += "setpoints:{ph:7.2,orp:750},";
    html += "savedMsg:''";
    html += "}}";
    html += ",computed:{";
    html += "pumps(){return this.data?.pumps || {}},";
    html += "relays(){return this.data?.relays || []},";
    html += "phEnabled(){return this.data?.ph_enabled || false},";
    html += "clEnabled(){return this.data?.cl_enabled || false},";
    html += "requiredRuntime(){return this.data?.pumps?.filter?.required_runtime_min || 0},";
    html += "actualRuntime(){return this.data?.pumps?.filter?.today_min || 0},";
    html += "progressPct(){const r=this.requiredRuntime,a=this.actualRuntime;return r>0?Math.max(1,Math.round((a/r)*100)):0}";
    html += "}";
    html += ",methods:{";
    html += "async fetchApi(){const r=await fetch('/api');this.data=await r.json();this.setpoints.ph=this.data.ph_setpoint;this.setpoints.orp=this.data.orp_setpoint;},";
    html += "formatNum(v,d){return v!=null?v.toFixed(d):'—'},";
    html += "async applySetpoints(){this.savedMsg='Saving...';await fetch('/api/setpoint?ph='+this.setpoints.ph+'&orp='+this.setpoints.orp);this.savedMsg='Saved!';setTimeout(()=>this.savedMsg='',2000);this.fetchApi();},";
    html += "async setMode(m){await fetch('/api/manual?mode='+(m?1:0));this.fetchApi();},";
    html += "async togglePump(id){const p=this.pumps[id];await fetch('/api/pump/set?id='+id+'&state='+(p.on?0:1));this.fetchApi();},";
    html += "async toggleRelay(ch){await fetch('/api/relay/set?channel='+ch+'&state='+(!this.relays[ch]?1:0));this.fetchApi();},";
    html += "async allOff(){await fetch('/api/alloff');this.fetchApi();}";
    html += "}";
    html += ",mounted(){this.fetchApi();setInterval(()=>this.fetchApi(),15000)}";
    html += "});";
    html += "api.mount('#app');";
    html += "</script>";
    html += "</body></html>";
    webServer.send(200, "text/html", html);
}

void handleAPI() {'''

# Replace the old function
new_content = content[:match.start()] + new_func + content[match.end():]

with open('/tmp/pool-controller-kc868/src/main.cpp', 'w') as f:
    f.write(new_content)

print("SUCCESS: handleRoot() replaced with Vue.js version")
print(f"New file size: {len(new_content)} bytes")
