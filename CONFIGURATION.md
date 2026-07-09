# Konfigurationshandbuch

> Dieses Dokument beschreibt alle Parameter der zentralen JSON-Konfigurationsdatei
> `data/config.json`, die via LittleFS auf den ESP32 geladen wird. Änderungen
> werden nach einem Neustart des Controllers wirksam.

---

## 📋 Inhaltsverzeichnis

- [Übersicht](#-übersicht)
- [WiFi-Konfiguration](#-wifi-konfiguration)
- [MQTT-Konfiguration](#-mqtt-konfiguration)
- [PID-Parameter – pH-Regelung](#-pid-parameter--ph-regelung)
- [PID-Parameter – Chlor-Regelung](#-pid-parameter--chlor-regelung)
- [Sensor-Konfiguration](#-sensor-konfiguration)
- [Pumpen-Konfiguration](#-pumpen-konfiguration)
- [Filterpumpe](#-filterpumpe)
- [Relais-Mapping](#-relais-mapping)
- [Logging & System](#-logging--system)
- [Vollständiges Beispiel](#-vollständiges-beispiel)

---

## 📖 Übersicht

Die gesamte Konfiguration wird zentral in einer einzigen `config.json`-Datei
verwaltet. Das Format ist JSON (`ArduinoJson 6.x`). Die Datei wird im LittleFS
des ESP32 abgelegt und beim Start geladen.

**Wichtige Regeln:**

- Die Datei **muss** gültiges JSON sein (Syntax-Fehler führen zum Abbruch)
- Nach Änderungen muss der Controller **neu gestartet** werden
- Fehlende Felder werden mit Standardwerten belegt (beim ersten Start)
- Das Web-Interface zeigt die aktuell geladene Konfiguration an (Read-only)
- Bei fehlender oder korrupter `config.json` wird ein Fallback verwendet
- Änderungen können auch via MQTT (`pool/config/set`) vorgenommen werden

---

## 📶 WiFi-Konfiguration

```json
{
  "wifi": {
    "ssid": "YourWiFi",
    "password": "YourPassword",
    "hostname": "poolcontroller",
    "fallbackAP": false,
    "apSSID": "PoolController-AP",
    "apPassword": "12345678"
  }
}
```

| Parameter | Typ | Standard | Beschreibung |
|-----------|-----|----------|-------------|
| `ssid` | String | — | **Erforderlich.** SSID des WLAN-Netzwerks |
| `password` | String | — | WLAN-Passwort (leer bei offenen Netzwerken) |
| `hostname` | String | `poolcontroller` | Hostname für DHCP und mDNS |
| `fallbackAP` | Boolean | `false` | Fallback Access Point bei Verbindungsfehler aktivieren |
| `apSSID` | String | `PoolController-AP` | SSID des Fallback-Access-Points |
| `apPassword` | String | `12345678` | Passwort des Fallback-AP (min. 8 Zeichen) |

### Fallback AP – Verhalten

Der Controller versucht beim Start, sich mit dem konfigurierten WLAN zu verbinden:

1. **Verbindungsversuch**: Maximal 15 Sekunden Wartezeit
2. **Bei Erfolg**: Normaler Betrieb, Fallback-AP bleibt deaktiviert
3. **Bei Fehlschlag**: Fallback-AP wird aktiviert (sofern `fallbackAP: true`)
4. **Laufende Überwachung**: Bei Verbindungsverlust während des Betriebs wird
   automatisch neu verbunden (Auto-Reconnect)

Im Fallback-Modus ist der Controller nur über den Access Point erreichbar (IP:
`192.168.4.1`). Das Web-Interface zur Konfiguration ist dann lokal aufrufbar.

---

## 🔗 MQTT-Konfiguration

```json
{
  "mqtt": {
    "broker": "192.168.178.223",
    "port": 1883,
    "clientId": "poolcontroller",
    "username": "",
    "password": "",
    "baseTopic": "pool",
    "keepAliveSec": 60
  }
}
```

| Parameter | Typ | Standard | Beschreibung |
|-----------|-----|----------|-------------|
| `broker` | String | — | **Erforderlich.** MQTT-Broker (IP oder Hostname) |
| `port` | Integer | `1883` | MQTT-Port (1883 = unverschlüsselt, 8883 = TLS) |
| `clientId` | String | `poolcontroller` | Eindeutige Client-ID für die MQTT-Verbindung |
| `username` | String | `""` | MQTT-Benutzername (leer = keine Authentifizierung) |
| `password` | String | `""` | MQTT-Passwort (leer = keine Authentifizierung) |
| `baseTopic` | String | `pool` | Basis-Topic für alle Nachrichten |
| `keepAliveSec` | Integer | `60` | Keep-Alive-Intervall (Sekunden) |

### Topic-Struktur

Der Controller verwendet JSON-Blöcke auf wenigen Topics:

```
{baseTopic}/status/LWT            → Online/Offline (Last Will, retained)
{baseTopic}/sensors               → Alle Sensorwerte (JSON-Block)
{baseTopic}/chemistry             → PID-Status, Sollwerte (JSON-Block)
{baseTopic}/filter                → Filterpumpe Status (JSON-Block)
{baseTopic}/pumps                 → Pumpen-Laufzeiten (JSON-Block)
{baseTopic}/ph                    → pH-Wert (numeric, für HA discovery)
{baseTopic}/command/ph_setpoint   → pH-Sollwert setzen
{baseTopic}/command/orp_setpoint  → ORP-Sollwert setzen
{baseTopic}/command/ph_set_enabled → pH-Regelung ein/aus
{baseTopic}/command/cl_set_enabled → Chlor-Regelung ein/aus
{baseTopic}/command/relay         → Relais direkt steuern
{baseTopic}/command/all_off       → Alle Relais AUS
{baseTopic}/command/reset_config  → Konfiguration zurücksetzen
{baseTopic}/command/restart       → ESP32 Neustart
{baseTopic}/config/set            → Konfiguration aktualisieren (JSON)
```

### Home Assistant Auto-Discovery

Der Controller sendet beim Start automatisch Discovery-Nachrichten an
`homeassistant/`. Registriert werden **16 Entities**:
Sensoren (pH, ORP, Wassertemperatur, Lufttemperatur, Filterdruck, Rückspül-Alarm),
PID-Outputs, Sollwerte (pH, ORP), Enable-Schalter (pH, Chlor),
und Pumpen-Laufzeiten (pH, Chlor, Filter).

Discovery kann nicht deaktiviert werden. Bei Bedarf manuell in HA ausblenden.

---

## 🎛️ PID-Parameter – pH-Regelung

```json
{
  "phPID": {
    "kp": 1.2,
    "ki": 0.08,
    "kd": 0.04,
    "setpoint": 7.2,
    "outputMin": 0.0,
    "outputMax": 100.0,
    "minOnTimeSec": 15,
    "minOffTimeSec": 60,
    "reverseAction": true
  }
}
```

| Parameter | Typ | Standard | Beschreibung |
|-----------|-----|----------|-------------|
| `kp` | Float | `1.2` | Proportionalverstärkung (P-Anteil) |
| `ki` | Float | `0.08` | Integralverstärkung (I-Anteil) |
| `kd` | Float | `0.04` | Differenzialverstärkung (D-Anteil) |
| `setpoint` | Float | `7.2` | pH-Sollwert (Ziel-pH) |
| `outputMin` | Float | `0.0` | Minimale Stellgröße (Pumpenleistung %) |
| `outputMax` | Float | `100.0` | Maximale Stellgröße (Pumpenleistung %) |
| `minOnTimeSec` | Integer | `15` | **Minimale Einschaltdauer** (Sekunden) |
| `minOffTimeSec` | Integer | `60` | **Minimale Ausschaltdauer** (Sekunden) |
| `reverseAction` | Boolean | `true` | **Reverse Acting**: pH-Minus-Dosierung senkt pH-Wert |

> **Hinweis:** `reverseAction: true` bedeutet: Wenn der pH-Wert ÜBER dem Sollwert
> liegt, wird die Dosierung AKTIV. Dies ist korrekt für pH-Minus (Säure).

### Tuning-Guide pH

Die PID-Regelung für pH-Werte ist relativ träge – Änderungen benötigen Zeit,
da das Wasser umgewälzt werden muss.

| Situation | Kp | Ki | Kd | Effekt |
|-----------|-----|-----|-----|--------|
| Standard (Start) | 1.2 | 0.08 | 0.04 | Ausgewogen, gut für typische Pools |
| Aggressiv (schnell) | 2.0 | 0.15 | 0.08 | Schnellere Korrektur, Überschwingen möglich |
| Konservativ (vorsichtig) | 0.8 | 0.04 | 0.02 | Langsamer, aber sicher (weniger Überdosierung) |
| Großer Pool (>80m³) | 1.5 | 0.10 | 0.05 | Höheres Wasservolumen benötigt mehr Nachdruck |

**Faustregeln:**
- **Kp** bestimmt die Reaktionsgeschwindigkeit: zu hoch → Überschwingen/Oszillation
- **Ki** eliminiert die Regelabweichung: zu hoch → Windup/Überschwingen
- **Kd** dämpft Überschwingen: zu hoch → Nervösität/Rauschanfälligkeit

### Anti-Windup

Der I-Anteil wird an den Ausgangsgrenzen (`outputMin`/`outputMax`) begrenzt
(clamping), um Integral-Windup zu verhindern.

---

## 🎛️ PID-Parameter – Chlor-Regelung

```json
{
  "chlorinePID": {
    "kp": 0.8,
    "ki": 0.05,
    "kd": 0.02,
    "setpoint": 650,
    "outputMin": 0.0,
    "outputMax": 100.0,
    "minOnTimeSec": 30,
    "minOffTimeSec": 120,
    "reverseAction": false
  }
}
```

| Parameter | Typ | Standard | Beschreibung |
|-----------|-----|----------|-------------|
| `kp` | Float | `0.8` | Proportionalverstärkung (P-Anteil) |
| `ki` | Float | `0.05` | Integralverstärkung (I-Anteil) |
| `kd` | Float | `0.02` | Differenzialverstärkung (D-Anteil) |
| `setpoint` | Float | `650` | ORP-Sollwert (mV, typisch 650-750) |
| `outputMin` | Float | `0.0` | Minimale Stellgröße |
| `outputMax` | Float | `100.0` | Maximale Stellgröße |
| `minOnTimeSec` | Integer | `30` | **Minimale Einschaltdauer** (Sekunden) |
| `minOffTimeSec` | Integer | `120` | **Minimale Ausschaltdauer** (Sekunden) |
| `reverseAction` | Boolean | `false` | **Direct Acting**: Chlor-Dosierung erhöht ORP |

> **Hinweis:** `reverseAction: false` (direct acting) bedeutet: Wenn ORP UNTER dem
> Sollwert liegt, wird die Dosierung AKTIV. Korrekt für Chlor.

### Tuning-Guide ORP

ORP-Änderungen sind **deutlich träger** als pH-Änderungen. Chlor reagiert
langsamer, daher sind niedrigere PID-Werte und längere Mindestzeiten sinnvoll.

| Situation | Kp | Ki | Kd | Effekt |
|-----------|-----|-----|-----|--------|
| Standard (Start) | 0.8 | 0.05 | 0.02 | Empfohlene Startwerte |
| Schnelle Korrektur | 1.2 | 0.08 | 0.03 | Risiko der Überdosierung |
| Konservativ | 0.5 | 0.02 | 0.01 | Sehr langsam, aber sicher |

### ORP ↔ Freies Chlor – Zusammenhang

ORP (Oxidation-Reduction Potential) korreliert mit dem Desinfektionsmittelgehalt,
ist aber **nicht linear** proportional zum freien Chlor. Der ORP-Wert wird
beeinflusst durch:

- **Freies Chlor**: Direkter Zusammenhang (höherer ORP = mehr Desinfektion)
- **pH-Wert**: Stark invers – bei pH 7.0 ist ORP höher als bei pH 7.6 (für gleiches Chlor)
- **Temperatur**: Wärmeres Wasser reduziert ORP (bei gleicher Chlorkonzentration)
- **Cyanursäure (Stabilisator)**: Reduziert ORP deutlich (gebundenes Chlor)

**Richtwerte:**
| ORP (mV) | Desinfektionswirkung |
|----------|---------------------|
| <450 | Unzureichend – Bakterienwachstum möglich |
| 450-600 | Geringe Desinfektion |
| 600-700 | Akzeptabel für Privatpools |
| 700-800 | Gute Desinfektion (empfohlen für öffentliche Bäder) |
| >800 | Sehr starke Desinfektion (Überdosierung möglich) |

---

## 🌡️ Sensor-Konfiguration

Jeder Sensor hat einen eigenen Konfigurationsblock mit folgenden Parametern:

```json
{
  "phSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 2000,
    "simMin": 6.8,
    "simMax": 7.6,
    "simDriftPerHour": 0.05
  },
  "orpSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 2000,
    "simMin": 200,
    "simMax": 800,
    "simDriftPerHour": 10
  },
  "tempWaterSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 2000,
    "simMin": 5.0,
    "simMax": 35.0,
    "simDriftPerHour": 0.5
  },
  "tempAirSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 2000,
    "simMin": 10.0,
    "simMax": 40.0,
    "simDriftPerHour": 2.0
  },
  "pressureSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 2000,
    "simMin": 0.0,
    "simMax": 2.5,
    "simDriftPerHour": 0.1
  }
}
```

| Parameter | Typ | Standard | Beschreibung |
|-----------|-----|----------|-------------|
| `enabled` | Boolean | `true` | Sensor aktivieren/deaktivieren |
| `simulate` | Boolean | `false` | Simulationsmodus aktivieren (kein HW-Sensor nötig) |
| `updateIntervalMs` | Integer | `2000` | Abtastintervall in Millisekunden |
| `simMin` | Float | *Sensorabhängig* | Minimaler Simulationswert (Random Walk) |
| `simMax` | Float | *Sensorabhängig* | Maximaler Simulationswert (Random Walk) |
| `simDriftPerHour` | Float | *Sensorabhängig* | Maximale Änderung pro Stunde (Drift) |

### Simulationsdetails

Die Simulation verwendet einen **Random-Walk-Algorithmus** mit
**pumpenabhängiger Drift**:

- **pH-Simulation**: Natürliche Drift +0.15/h (pH steigt ohne Dosierung),
  Pumpen-Dosierung −0.8/h (pH-Minus senkt den Wert)
- **ORP-Simulation**: Natürlicher Zerfall −8 mV/h (Chlor baut ab),
  Pumpen-Dosierung +40 mV/h (Chlor erhöht ORP)
- Bei `simulate: true` werden keine Hardware-Sensoren benötigt
- Simulation überschreibt **NIEMALS** einen angeschlossenen Sensor
- Fällt ein echter Sensor aus, wird NICHT automatisch simuliert

---

## ⚙️ Pumpen-Konfiguration

```json
{
  "phPump": {
    "relayChannel": 1,
    "minOnTimeSec": 30,
    "minOffTimeSec": 120,
    "maxDailyRuntimeMin": 1440.0
  },
  "chlorinePump": {
    "relayChannel": 2,
    "minOnTimeSec": 30,
    "minOffTimeSec": 120,
    "maxDailyRuntimeMin": 1440.0
  }
}
```

| Parameter | Typ | Standard | Beschreibung |
|-----------|-----|----------|-------------|
| `relayChannel` | Integer | *siehe unten* | PCF8574-Relais-Kanal (0-7) |
| `minOnTimeSec` | Integer | `30` | Minimale Einschaltdauer (Sekunden) |
| `minOffTimeSec` | Integer | `120` | Minimale Ausschaltdauer (Sekunden) |
| `maxDailyRuntimeMin` | Float | `1440.0` | **Maximale Tageslaufzeit** (Minuten, Sicherheit!) |

### Sicherheitsfeatures

- **Maximale Tageslaufzeit**: Wird `maxDailyRuntimeMin` überschritten, stoppt die Pumpe
  für den Rest des Tages (Hardware-Schutz)
- **Mindest-Ausschaltzeit**: Verhindert Kurzzyklen (über `minOffTimeSec`)
- **Filterpumpen-Interlock**: Chemie-Dosierung NUR bei laufender Filterpumpe
  (via PumpController dependents)
- **Extremwert-Sperre**: Bei pH < 6,5 oder pH > 8,0 wird die Dosierung gestoppt

---

## 🔄 Filterpumpe

```json
{
  "filterPump": {
    "relayChannel": 0,
    "tempSlope": 8.0,
    "tempIntercept": -40.0,
    "windowStart": "07:00",
    "windowEnd": "21:00",
    "minCycleMinutes": 60,
    "maxCycleMinutes": 480
  }
}
```

| Parameter | Typ | Standard | Beschreibung |
|-----------|-----|----------|-------------|
| `relayChannel` | Integer | `0` | PCF8574-Relais-Kanal (0-basiert) |
| `tempSlope` | Float | `8.0` | Steigung der Temperatur-Laufzeit-Funktion [min/°C] |
| `tempIntercept` | Float | `-40.0` | Achsenabschnitt der Laufzeit-Funktion [min] |
| `windowStart` | String | `"07:00"` | Früheste Einschaltzeit (24h-Format) |
| `windowEnd` | String | `"21:00"` | Späteste Ausschaltzeit (24h-Format) |
| `minCycleMinutes` | Integer | `60` | Minimale Filterlaufzeit [min] |
| `maxCycleMinutes` | Integer | `480` | Maximale Filterlaufzeit [min] |

### Laufzeitberechnung

Die Filterpumpen-Laufzeit wird **linear** aus der Wassertemperatur berechnet:

```
Laufzeit [min] = tempSlope × Wassertemperatur [°C] + tempIntercept
Laufzeit = clamp(Laufzeit, minCycleMinutes, maxCycleMinutes)
```

**Beispiele (tempSlope=8.0, tempIntercept=-40):**

| Wassertemperatur | Berechnete Laufzeit |
|-----------------|-------------------|
| 25 °C | 8.0 × 25 − 40 = 160 min |
| 30 °C | 8.0 × 30 − 40 = 200 min |
| 20 °C | 8.0 × 20 − 40 = 120 min |
| 10 °C | 8.0 × 10 − 40 = 40 min → begrenzt auf 60 min (minCycleMinutes) |

### Zeitfenster

Die Filterpumpe läuft nur innerhalb des konfigurierten Zeitfensters
(`windowStart` bis `windowEnd`). Die berechnete Laufzeit wird innerhalb dieses
Fensters abgedeckt. Bei Überschneidung mit dem Fensterende wird die Laufzeit
entsprechend verkürzt.

---

## 🔌 Relais-Mapping

Das KC868-A8 verwendet einen **PCF8574 I2C I/O-Expander** (Adresse 0x24).
Es gibt **keine direkten GPIO-Pins** für Relais — alle 8 Relais werden
über den PCF8574-Bus angesteuert (active-LOW: 0 = ON, 1 = OFF).

```json
{
  "relays": [
    {"channel": 0, "name": "Filter Pumpe", "normallyOpen": true, "maxOnTimeSec": 0},
    {"channel": 1, "name": "pH Pumpe", "normallyOpen": true, "maxOnTimeSec": 0},
    {"channel": 2, "name": "Chlor Pumpe", "normallyOpen": true, "maxOnTimeSec": 0},
    {"channel": 3, "name": "Relay 4", "normallyOpen": true, "maxOnTimeSec": 0},
    {"channel": 4, "name": "Relay 5", "normallyOpen": true, "maxOnTimeSec": 0},
    {"channel": 5, "name": "Relay 6", "normallyOpen": true, "maxOnTimeSec": 0},
    {"channel": 6, "name": "Relay 7", "normallyOpen": true, "maxOnTimeSec": 0},
    {"channel": 7, "name": "Relay 8", "normallyOpen": true, "maxOnTimeSec": 0}
  ],
  "relayCount": 8
}
```

| Parameter | Typ | Beschreibung |
|-----------|-----|-------------|
| `channel` | Integer | PCF8574-Kanal (0-7) |
| `name` | String | Anzeigename für Web-Interface und HA |
| `normallyOpen` | Boolean | `true` = Schließer (NO), `false` = Öffner (NC) |
| `maxOnTimeSec` | Integer | Maximale Einschaltdauer (0 = unbegrenzt) |

**PCF8574-Bit-Zuordnung:**

| Kanal | PCF8574-Bit | Funktion (Standard) |
|-------|------------|---------------------|
| 0 | 0 | Filterpumpe |
| 1 | 1 | pH-Pumpe |
| 2 | 2 | Chlor-Pumpe |
| 3 | 3 | Reserve 1 |
| 4 | 4 | Reserve 2 |
| 5 | 5 | Reserve 3 |
| 6 | 6 | Reserve 4 |
| 7 | 7 | Reserve 5 |

---

## 📝 Logging & System

```json
{
  "logLevel": 1,
  "loopDelayMs": 100
}
```

| Parameter | Typ | Standard | Beschreibung |
|-----------|-----|----------|-------------|
| `logLevel` | Integer | `1` | Log-Level: 0=ERROR, 1=WARN/INFO |
| `loopDelayMs` | Integer | `100` | Hauptschleifen-Verzögerung (ms) |

---

## 📄 Vollständiges Beispiel

```json
{
  "configVersion": 4,
  "wifi": {
    "ssid": "YourWiFi",
    "password": "YourPassword",
    "hostname": "poolcontroller",
    "fallbackAP": false,
    "apSSID": "PoolController-AP",
    "apPassword": "12345678"
  },
  "mqtt": {
    "broker": "192.168.178.223",
    "port": 1883,
    "clientId": "poolcontroller",
    "username": "",
    "password": "",
    "baseTopic": "pool",
    "keepAliveSec": 60
  },
  "phPID": {
    "kp": 1.2,
    "ki": 0.08,
    "kd": 0.04,
    "setpoint": 7.2,
    "outputMin": 0.0,
    "outputMax": 100.0,
    "minOnTimeSec": 15,
    "minOffTimeSec": 60,
    "reverseAction": true
  },
  "chlorinePID": {
    "kp": 0.8,
    "ki": 0.05,
    "kd": 0.02,
    "setpoint": 650,
    "outputMin": 0.0,
    "outputMax": 100.0,
    "minOnTimeSec": 30,
    "minOffTimeSec": 120,
    "reverseAction": false
  },
  "phSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 2000,
    "simMin": 6.8,
    "simMax": 7.6,
    "simDriftPerHour": 0.05
  },
  "orpSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 2000,
    "simMin": 200,
    "simMax": 800,
    "simDriftPerHour": 10
  },
  "tempWaterSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 2000,
    "simMin": 5.0,
    "simMax": 35.0,
    "simDriftPerHour": 0.5
  },
  "tempAirSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 2000,
    "simMin": 10.0,
    "simMax": 40.0,
    "simDriftPerHour": 2.0
  },
  "pressureSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 2000,
    "simMin": 0.0,
    "simMax": 2.5,
    "simDriftPerHour": 0.1
  },
  "phPump": {
    "relayChannel": 1,
    "minOnTimeSec": 30,
    "minOffTimeSec": 120,
    "maxDailyRuntimeMin": 1440.0
  },
  "chlorinePump": {
    "relayChannel": 2,
    "minOnTimeSec": 30,
    "minOffTimeSec": 120,
    "maxDailyRuntimeMin": 1440.0
  },
  "filterPump": {
    "relayChannel": 0,
    "tempSlope": 8.0,
    "tempIntercept": -40.0,
    "windowStart": "07:00",
    "windowEnd": "21:00",
    "minCycleMinutes": 60,
    "maxCycleMinutes": 480
  },
  "relays": [
    {"channel": 0, "name": "Filter Pumpe", "normallyOpen": true, "maxOnTimeSec": 0},
    {"channel": 1, "name": "pH Pumpe", "normallyOpen": true, "maxOnTimeSec": 0},
    {"channel": 2, "name": "Chlor Pumpe", "normallyOpen": true, "maxOnTimeSec": 0},
    {"channel": 3, "name": "Relay 4", "normallyOpen": true, "maxOnTimeSec": 0},
    {"channel": 4, "name": "Relay 5", "normallyOpen": true, "maxOnTimeSec": 0},
    {"channel": 5, "name": "Relay 6", "normallyOpen": true, "maxOnTimeSec": 0},
    {"channel": 6, "name": "Relay 7", "normallyOpen": true, "maxOnTimeSec": 0},
    {"channel": 7, "name": "Relay 8", "normallyOpen": true, "maxOnTimeSec": 0}
  ],
  "relayCount": 8,
  "logLevel": 1,
  "loopDelayMs": 100
}
```
