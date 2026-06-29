# Konfigurationshandbuch

> Dieses Dokument beschreibt alle Parameter der zentralen JSON-Konfigurationsdatei
> `data/config.json`, die via SPIFFS auf den ESP32 geladen wird. Änderungen
> werden nach einem Neustart des Controllers wirksam.

---

## 📋 Inhaltsverzeichnis

- [Übersicht](#-übersicht)
- [WiFi-Konfiguration](#-wifi-konfiguration)
- [MQTT-Konfiguration](#-mqtt-konfiguration)
- [PID-Parameter – pH-Regelung](#-pid-parameter--ph-regelung)
- [PID-Parameter – Chlor-Regelung (ORP)](#-pid-parameter--chlor-regelung-orp)
- [Sensor-Konfiguration](#-sensor-konfiguration)
- [Pumpen-Konfiguration](#-pumpen-konfiguration)
- [Filterpumpe](#-filterpumpe)
- [Relais-Mapping](#-relais-mapping)
- [Logging & System](#-logging--system)
- [Vollständiges Beispiel](#-vollständiges-beispiel)

---

## 📖 Übersicht

Die gesamte Konfiguration wird zentral in einer einzigen `config.json`-Datei
verwaltet. Das Format ist JSON (`ArduinoJson 7.x`). Die Datei wird im SPIFFS
(SPIFFS = SPI Flash File System) des ESP32 abgelegt und beim Start geladen.

**Wichtige Regeln:**

- Die Datei **muss** gültiges JSON sein (Syntax-Fehler führen zum Abbruch)
- Nach Änderungen muss der Controller **neu gestartet** werden
- Fehlende Felder werden mit Standardwerten belegt (beim ersten Start)
- Die Web-Interface zeigt die aktuell geladene Konfiguration an (Read-only)
- Bei fehlender oder korrupter `config.json` wird ein Fallback verwendet

---

## 📶 WiFi-Konfiguration

```json
{
  "wifi": {
    "ssid": "YourWiFi",
    "password": "YourPassword",
    "hostname": "pool-controller",
    "fallbackAP": true,
    "apSSID": "Pool-Config",
    "apPassword": "poolconfig2024"
  }
}
```

| Parameter | Typ | Standard | Beschreibung |
|-----------|-----|----------|-------------|
| `ssid` | String | — | **Erforderlich.** SSID des WLAN-Netzwerks |
| `password` | String | — | WLAN-Passwort (leer bei offenen Netzwerken) |
| `hostname` | String | `pool-controller` | Hostname für DHCP und mDNS (unterstützt `.local`) |
| `fallbackAP` | Boolean | `true` | Fallback Access Point bei Verbindungsfehler aktivieren |
| `apSSID` | String | `Pool-Config` | SSID des Fallback-Access-Points |
| `apPassword` | String | `poolconfig2024` | Passwort des Fallback-AP (min. 8 Zeichen) |

### Fallback AP – Verhalten

Der Controller versucht beim Start, sich mit dem konfigurierten WLAN zu verbinden:

1. **Verbindungsversuch**: Maximal 15 Sekunden Wartezeit
2. **Bei Erfolg**: Normaler Betrieb, Fallback-AP bleibt deaktiviert
3. **Bei Fehlschlag**: Fallback-AP wird aktiviert
4. **Laufende Überwachung**: Bei Verbindungsverlust während des Betriebs wird
   automatisch neu verbunden (Auto-Reconnect)
5. **Nach erfolgreicher Verhandlung**: Der AP wird deaktiviert und der Controller
   startet den normalen Betrieb

Im Fallback-Modus ist der Controller nur über den Access Point erreichbar (IP:
`192.168.4.1`). Das Web-Interface zur Konfiguration ist dann lokal aufrufbar.

---

## 🔗 MQTT-Konfiguration

```json
{
  "mqtt": {
    "broker": "192.168.178.100",
    "port": 1883,
    "clientId": "pool-controller",
    "username": "",
    "password": "",
    "baseTopic": "pool",
    "keepAliveSec": 60,
    "discoveryPrefix": "homeassistant",
    "discoveryEnabled": true
  }
}
```

| Parameter | Typ | Standard | Beschreibung |
|-----------|-----|----------|-------------|
| `broker` | String | — | **Erforderlich.** MQTT-Broker (IP oder Hostname) |
| `port` | Integer | `1883` | MQTT-Port (1883 = unverschlüsselt, 8883 = TLS) |
| `clientId` | String | `pool-controller` | Eindeutige Client-ID für die MQTT-Verbindung |
| `username` | String | `""` | MQTT-Benutzername (leer = keine Authentifizierung) |
| `password` | String | `""` | MQTT-Passwort (leer = keine Authentifizierung) |
| `baseTopic` | String | `pool` | Basis-Topic für alle Nachrichten |
| `keepAliveSec` | Integer | `60` | Keep-Alive-Intervall (Sekunden) |
| `discoveryPrefix` | String | `homeassistant` | HA Discovery Prefix |
| `discoveryEnabled` | Boolean | `true` | HA Auto-Discovery aktivieren/deaktivieren |

### Topic-Struktur

Der Controller verwendet folgende Topic-Struktur:

```
{baseTopic}/status               → Online/Offline (Last Will)
{baseTopic}/ph/value             → pH-Messwert
{baseTopic}/ph/setpoint          → pH-Sollwert (auch als Command-Topic)
{baseTopic}/ph/pump              → pH-Pumpenstatus
{baseTopic}/orp/value            → ORP-Messwert (mV)
{baseTopic}/orp/setpoint         → ORP-Sollwert (auch als Command-Topic)
{baseTopic}/orp/pump             → Chlor-Pumpenstatus
{baseTopic}/temp/water           → Wassertemperatur
{baseTopic}/temp/air             → Lufttemperatur
{baseTopic}/humidity             → Luftfeuchte
{baseTopic}/filter/state         → Filterpumpenstatus
{baseTopic}/filter/runtime       → Berechnete Laufzeit (h)
{baseTopic}/filter/remaining     → Verbleibende Laufzeit (min)
{baseTopic}/pressure             → Filterdruck
{baseTopic}/backwash             → Rückspül-Alarm
{baseTopic}/pid/ph               → pH-PID-Diagnose (JSON)
{baseTopic}/pid/orp              → ORP-PID-Diagnose (JSON)
{baseTopic}/cmd/reboot           → Neustart-Befehl
{baseTopic}/cmd/reset            → Reset-Befehl
```

### Home Assistant Auto-Discovery

Der Controller sendet beim Start automatisch Discovery-Nachrichten an das Topic
`{discoveryPrefix}/` (standardmäßig `homeassistant/`). Jeder Sensor/Schalter wird
als eigenes Entity registriert mit vollständigen Metadaten (Name, Einheit,
Gerätezuordnung, Icons).

Bei `discoveryEnabled: false` müssen die Entities manuell in der
`configuration.yaml` konfiguriert werden.

---

## 🎛️ PID-Parameter – pH-Regelung

```json
{
  "phPID": {
    "kp": 2.0,
    "ki": 0.1,
    "kd": 0.5,
    "setpoint": 7.2,
    "outputMin": 0.0,
    "outputMax": 100.0,
    "minOnTimeSec": 30,
    "minOffTimeSec": 120,
    "deadband": 0.05
  }
}
```

| Parameter | Typ | Standard | Beschreibung |
|-----------|-----|----------|-------------|
| `kp` | Float | `2.0` | Proportionalverstärkung (P-Anteil) |
| `ki` | Float | `0.1` | Integralverstärkung (I-Anteil) |
| `kd` | Float | `0.5` | Differenzialverstärkung (D-Anteil) |
| `setpoint` | Float | `7.2` | pH-Sollwert (Ziel-pH) |
| `outputMin` | Float | `0.0` | Minimale Stellgröße (Pumpenleistung %) |
| `outputMax` | Float | `100.0` | Maximale Stellgröße (Pumpenleistung %) |
| `minOnTimeSec` | Integer | `30` | **Minimale Einschaltdauer** (Sekunden) |
| `minOffTimeSec` | Integer | `120` | **Minimale Ausschaltdauer** (Sekunden) |
| `deadband` | Float | `0.05` | Totband um den Sollwert (±pH) |

### Tuning-Guide pH

Die PID-Regelung für pH-Werte ist relativ träge – Änderungen benötigen Zeit,
da das Wasser umgewälzt werden muss.

| Situation | Kp | Ki | Kd | Effekt |
|-----------|-----|-----|-----|--------|
| Standard (Start) | 2.0 | 0.1 | 0.5 | Ausgewogen, gut für typische Pools |
| Aggressiv (schnell) | 3.0 | 0.2 | 0.8 | Schnellere Korrektur, Überschwingen möglich |
| Konservativ (vorsichtig) | 1.0 | 0.05 | 0.2 | Langsamer, aber sicher (weniger Überdosierung) |
| Großer Pool (>80m³) | 2.5 | 0.15 | 0.6 | Höheres Wasservolumen benötigt mehr Nachdruck |

**Faustregeln:**
- **Kp** bestimmt die Reaktionsgeschwindigkeit: zu hoch → Überschwingen/Oszillation
- **Ki** eliminiert die Regelabweichung: zu hoch → Windup/Überschwingen
- **Kd** dämpft Überschwingen: zu hoch → Nervösität/Rauschanfälligkeit
- **Totband** vor allem bei der pH-Regelung nützlich, um Kurzzyklen zu vermeiden

### Anti-Windup

Der I-Anteil wird an den Ausgangsgrenzen begrenzt (clamping), um Integral-Windup
zu verhindern. Der Integrator wird während der minimalen Ausschaltzeit nicht verändert.

---

## 🎛️ PID-Parameter – Chlor-Regelung (ORP)

```json
{
  "orpPID": {
    "kp": 1.0,
    "ki": 0.05,
    "kd": 0.2,
    "setpoint": 650,
    "outputMin": 0.0,
    "outputMax": 100.0,
    "minOnTimeSec": 60,
    "minOffTimeSec": 180,
    "deadband": 10
  }
}
```

| Parameter | Typ | Standard | Beschreibung |
|-----------|-----|----------|-------------|
| `kp` | Float | `1.0` | Proportionalverstärkung (P-Anteil) |
| `ki` | Float | `0.05` | Integralverstärkung (I-Anteil) |
| `kd` | Float | `0.2` | Differenzialverstärkung (D-Anteil) |
| `setpoint` | Integer | `650` | ORP-Sollwert (mV, typisch 650-750) |
| `outputMin` | Float | `0.0` | Minimale Stellgröße |
| `outputMax` | Float | `100.0` | Maximale Stellgröße |
| `minOnTimeSec` | Integer | `60` | **Minimale Einschaltdauer** (Sekunden) |
| `minOffTimeSec` | Integer | `180` | **Minimale Ausschaltdauer** (Sekunden) |
| `deadband` | Integer | `10` | Totband um den Sollwert (±mV) |

### Tuning-Guide ORP

ORP-Änderungen sind **deutlich träger** als pH-Änderungen. Chlor reagiert
langsamer, daher sind niedrigere PID-Werte und längere Mindestzeiten sinnvoll.

| Situation | Kp | Ki | Kd | Effekt |
|-----------|-----|-----|-----|--------|
| Standard (Start) | 1.0 | 0.05 | 0.2 | Empfohlene Startwerte |
| Schnelle Korrektur | 1.5 | 0.10 | 0.3 | Risiko der Überdosierung |
| Konservativ | 0.5 | 0.02 | 0.1 | Sehr langsam, aber sicher |

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
    "updateIntervalMs": 5000,
    "simMin": 6.8,
    "simMax": 7.6,
    "simDriftPerHour": 0.1
  },
  "orpSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 5000,
    "simMin": 500,
    "simMax": 800,
    "simDriftPerHour": 20
  },
  "tempWaterSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 10000,
    "simMin": 20.0,
    "simMax": 35.0,
    "simDriftPerHour": 0.5
  },
  "tempAirSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 10000,
    "simMin": 10.0,
    "simMax": 40.0,
    "simDriftPerHour": 2.0
  },
  "pressureSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 15000,
    "simMin": 0.2,
    "simMax": 0.8,
    "simDriftPerHour": 0.0
  }
}
```

| Parameter | Typ | Standard | Beschreibung |
|-----------|-----|----------|-------------|
| `enabled` | Boolean | `true` | Sensor aktivieren/deaktivieren |
| `simulate` | Boolean | `false` | Simulationsmodus aktivieren (kein HW-Sensor nötig) |
| `updateIntervalMs` | Integer | `5000` | Abtastintervall in Millisekunden |
| `simMin` | Float | *Sensorabhängig* | Minimaler Simulationswert (Random Walk) |
| `simMax` | Float | *Sensorabhängig* | Maximaler Simulationswert (Random Walk) |
| `simDriftPerHour` | Float | *Sensorabhängig* | Maximale Änderung pro Stunde (Drift) |

### Simulationsdetails

Die Simulation verwendet einen **Random-Walk-Algorithmus**:
```
wert_neu = wert_alt + random(-drift, +drift) * (delta_zeit / 3600)
wert_neu = clamp(wert_neu, simMin, simMax)
```

- Ohne `simulate: true` werden reale Sensordaten verwendet
- Simulation überschreibt **NIEMALS** einen angeschlossenen Sensor
- Die Drift-Rate steuert die maximale Änderungsgeschwindigkeit
- Bei `simDriftPerHour: 0` bleibt der Wert konstant (nützlich für Druck-Tests)

### Kalibrierung

Für pH- und ORP-Sensoren werden Kalibrierungswerte in einem eigenen Block gespeichert:

```json
{
  "calibration": {
    "phCalibration": {
      "ph4Voltage": 1.86,
      "ph7Voltage": 2.06
    },
    "orpCalibration": {
      "factor": 1.0,
      "offset": 0
    }
  }
}
```

| Parameter | Beschreibung |
|-----------|-------------|
| `ph4Voltage` | Gemessene ADC-Spannung [V] bei pH 4.0 Pufferlösung |
| `ph7Voltage` | Gemessene ADC-Spannung [V] bei pH 7.0 Pufferlösung |
| `orpCalibration.factor` | Multiplikator zur Spannungs-ORP-Umrechnung |
| `orpCalibration.offset` | Offset in mV (für Abweichungskorrektur) |

---

## ⚙️ Pumpen-Konfiguration

### pH-Pumpe

```json
{
  "phPump": {
    "relayChannel": 1,
    "maxOnTimeSec": 300,
    "invertLogic": false,
    "label": "pH-Dosierpumpe"
  }
}
```

### Chlor-Pumpe

```json
{
  "chlorinePump": {
    "relayChannel": 2,
    "maxOnTimeSec": 300,
    "invertLogic": false,
    "label": "Chlor-Dosierpumpe"
  }
}
```

### Parameter

| Parameter | Typ | Standard | Beschreibung |
|-----------|-----|----------|-------------|
| `relayChannel` | Integer | siehe | Relais-Kanal (0-basiert, siehe Relais-Mapping) |
| `maxOnTimeSec` | Integer | `300` | **Maximale Einschaltdauer (Sicherheit!)** |
| `invertLogic` | Boolean | `false` | Logik invertieren (NC statt NO) |
| `label` | String | — | Anzeigename für Web-Interface |

### Sicherheitsfeatures

- **Maximale Einschaltdauer**: Überschreitet die Dosierung diesen Wert, wird
  die Pumpe zwangsweise abgeschaltet (Hardware-Timeout)
- **Mindest-Ausschaltzeit**: Verhindert Kurzzyklen (über PID konfiguriert)
- **Filterpumpen-Interlock**: Chemie-Dosierung NUR bei laufender Filterpumpe
- **Extremwert-Sperre**: Bei pH < 6,5 oder pH > 8,0 wird die Dosierung gestoppt
  (auch bei laufender Filterpumpe)

---

## 🔄 Filterpumpe

```json
{
  "filterPump": {
    "relayChannel": 0,
    "tempSlope": 0.5,
    "tempIntercept": 2.0,
    "windowStart": "08:00",
    "windowEnd": "22:00",
    "minRuntimeHours": 2,
    "maxRuntimeHours": 12,
    "backwashThreshold": 0.8
  }
}
```

| Parameter | Typ | Standard | Beschreibung |
|-----------|-----|----------|-------------|
| `relayChannel` | Integer | `0` | Relais-Kanal (0-basiert) |
| `tempSlope` | Float | `0.5` | Steigung der Temperatur-Laufzeit-Funktion |
| `tempIntercept` | Float | `2.0` | Achsenabschnitt der Laufzeit-Funktion [h] |
| `windowStart` | String | `"08:00"` | Früheste Einschaltzeit (24h-Format) |
| `windowEnd` | String | `"22:00"` | Späteste Ausschaltzeit (24h-Format) |
| `minRuntimeHours` | Float | `2.0` | Minimale Laufzeit [h] (Override) |
| `maxRuntimeHours` | Float | `12.0` | Maximale Laufzeit [h] (Override) |
| `backwashThreshold` | Float | `0.8` | Rückspül-Schwelle in bar |

### Laufzeitberechnung

Die Filterpumpen-Laufzeit wird **linear** aus der Wassertemperatur berechnet:

```
Laufzeit [h] = tempSlope × Wassertemperatur [°C] + tempIntercept
```

**Beispiele:**

| Wassertemperatur | tempSlope | tempIntercept | Berechnete Laufzeit |
|-----------------|-----------|---------------|-------------------|
| 20 °C | 0.5 | 2.0 | 12 h |
| 25 °C | 0.5 | 2.0 | 14,5 h → begrenzt auf 12 h (maxRuntimeHours) |
| 15 °C | 0.5 | 2.0 | 9,5 h |
| 10 °C | 0.5 | 2.0 | 7 h |

**Berechnungslogik:**

```
runtime = tempSlope * waterTemp + tempIntercept
runtime = clamp(runtime, minRuntimeHours, maxRuntimeHours)
```

Je wärmer das Wasser, desto mehr wird gefiltert – das ist die Empfehlung der
meisten Hersteller, da warmes Wasser Algen- und Bakterienwachstum begünstigt.

### Zeitfenster

Die Filterpumpe läuft nur innerhalb des konfigurierten Zeitfensters
(`windowStart` bis `windowEnd`). Die berechnete Laufzeit wird innerhalb dieses
Fensters abgedeckt (sofern möglich). Bei Überschneidung mit dem Fensterende
wird die Laufzeit entsprechend verkürzt.

---

## 🔌 Relais-Mapping

```json
{
  "relays": [
    {"channel": 0, "gpio": 16, "name": "Filterpumpe"},
    {"channel": 1, "gpio": 15, "name": "pH-Pumpe"},
    {"channel": 2, "gpio": 14, "name": "Chlor-Pumpe"},
    {"channel": 3, "gpio": 27, "name": "Beleuchtung"},
    {"channel": 4, "gpio": 26, "name": "Solarpumpe"},
    {"channel": 5, "gpio": 25, "name": "Wärmepumpe"},
    {"channel": 6, "gpio": 33, "name": "Reserve 1"},
    {"channel": 7, "gpio": 32, "name": "Reserve 2"}
  ]
}
```

| Parameter | Typ | Beschreibung |
|-----------|-----|-------------|
| `channel` | Integer | Relais-Kanal (0-7) |
| `gpio` | Integer | GPIO-Pin am ESP32 |
| `name` | String | Anzeigename für Web-Interface und HA |

**Vorgabe-Relais-Belegung des KC868-A8:**

| Kanal | GPIO | Funktion |
|-------|------|----------|
| 0 | 16 | Relais 1 |
| 1 | 15 | Relais 2 |
| 2 | 14 | Relais 3 |
| 3 | 27 | Relais 4 |
| 4 | 26 | Relais 5 |
| 5 | 25 | Relais 6 |
| 6 | 33 | Relais 7 |
| 7 | 32 | Relais 8 |

---

## 📝 Logging & System

```json
{
  "logging": {
    "logLevel": 2,
    "serialBaud": 115200
  },
  "system": {
    "loopDelayMs": 1000,
    "watchdogTimeoutSec": 300,
    "ntpServer": "pool.ntp.org",
    "timezone": "CET-1CEST,M3.5.0/2,M10.5.0/3"
  }
}
```

| Parameter | Typ | Standard | Beschreibung |
|-----------|-----|----------|-------------|
| `logLevel` | Integer | `2` | Log-Level: 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG |
| `serialBaud` | Integer | `115200` | Baudrate des seriellen Monitors |
| `loopDelayMs` | Integer | `1000` | Hauptschleifen-Verzögerung (ms) |
| `watchdogTimeoutSec` | Integer | `300` | Timeout für Hardware-Watchdog (Sekunden) |
| `ntpServer` | String | `pool.ntp.org` | NTP-Server für Zeit-Synchronisation |
| `timezone` | String | `CET-1CEST...` | Zeitzonen-String für NTP (POSIX) |

### Log-Level

| Level | Wert | Ausgabe |
|-------|------|---------|
| ERROR | 0 | Nur kritische Fehler |
| WARN | 1 | Fehler + Warnungen |
| INFO | 2 | Betriebsinformationen (Standard) |
| DEBUG | 3 | Detaillierte Debug-Ausgaben (PID-Werte, Sensor-Rohdaten) |

### System-Parameter

- **loopDelayMs**: Je niedriger, desto häufiger wird die Hauptschleife durchlaufen
  (höhere CPU-Last). Für den Normalbetrieb sind 1000 ms ausreichend.
- **watchdogTimeoutSec**: Wenn der ESP32 länger als dieser Wert keinen
  Loop-Durchlauf schafft, wird ein Hardware-Reset ausgelöst.

---

## 📄 Vollständiges Beispiel

```json
{
  "wifi": {
    "ssid": "YourWiFi",
    "password": "YourPassword",
    "hostname": "pool-controller",
    "fallbackAP": true,
    "apSSID": "Pool-Config",
    "apPassword": "poolconfig2024"
  },
  "mqtt": {
    "broker": "192.168.178.100",
    "port": 1883,
    "clientId": "pool-controller",
    "username": "",
    "password": "",
    "baseTopic": "pool",
    "keepAliveSec": 60,
    "discoveryPrefix": "homeassistant",
    "discoveryEnabled": true
  },
  "phPID": {
    "kp": 2.0,
    "ki": 0.1,
    "kd": 0.5,
    "setpoint": 7.2,
    "outputMin": 0.0,
    "outputMax": 100.0,
    "minOnTimeSec": 30,
    "minOffTimeSec": 120,
    "deadband": 0.05
  },
  "orpPID": {
    "kp": 1.0,
    "ki": 0.05,
    "kd": 0.2,
    "setpoint": 650,
    "outputMin": 0.0,
    "outputMax": 100.0,
    "minOnTimeSec": 60,
    "minOffTimeSec": 180,
    "deadband": 10
  },
  "phSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 5000,
    "simMin": 6.8,
    "simMax": 7.6,
    "simDriftPerHour": 0.1
  },
  "orpSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 5000,
    "simMin": 500,
    "simMax": 800,
    "simDriftPerHour": 20
  },
  "tempWaterSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 10000,
    "simMin": 20.0,
    "simMax": 35.0,
    "simDriftPerHour": 0.5
  },
  "tempAirSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 10000,
    "simMin": 10.0,
    "simMax": 40.0,
    "simDriftPerHour": 2.0
  },
  "pressureSensor": {
    "enabled": true,
    "simulate": false,
    "updateIntervalMs": 15000,
    "simMin": 0.2,
    "simMax": 0.8,
    "simDriftPerHour": 0.0
  },
  "calibration": {
    "phCalibration": {
      "ph4Voltage": 1.86,
      "ph7Voltage": 2.06
    },
    "orpCalibration": {
      "factor": 1.0,
      "offset": 0
    }
  },
  "phPump": {
    "relayChannel": 1,
    "maxOnTimeSec": 300,
    "invertLogic": false,
    "label": "pH-Dosierpumpe"
  },
  "chlorinePump": {
    "relayChannel": 2,
    "maxOnTimeSec": 300,
    "invertLogic": false,
    "label": "Chlor-Dosierpumpe"
  },
  "filterPump": {
    "relayChannel": 0,
    "tempSlope": 0.5,
    "tempIntercept": 2.0,
    "windowStart": "08:00",
    "windowEnd": "22:00",
    "minRuntimeHours": 2,
    "maxRuntimeHours": 12,
    "backwashThreshold": 0.8
  },
  "relays": [
    {"channel": 0, "gpio": 16, "name": "Filterpumpe"},
    {"channel": 1, "gpio": 15, "name": "pH-Pumpe"},
    {"channel": 2, "gpio": 14, "name": "Chlor-Pumpe"},
    {"channel": 3, "gpio": 27, "name": "Beleuchtung"},
    {"channel": 4, "gpio": 26, "name": "Solarpumpe"},
    {"channel": 5, "gpio": 25, "name": "Wärmepumpe"},
    {"channel": 6, "gpio": 33, "name": "Reserve 1"},
    {"channel": 7, "gpio": 32, "name": "Reserve 2"}
  ],
  "logging": {
    "logLevel": 2,
    "serialBaud": 115200
  },
  "system": {
    "loopDelayMs": 1000,
    "watchdogTimeoutSec": 300,
    "ntpServer": "pool.ntp.org",
    "timezone": "CET-1CEST,M3.5.0/2,M10.5.0/3"
  }
}
```