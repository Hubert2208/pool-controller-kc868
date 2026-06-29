# Sensor-Anbindung & Simulation

> Dieses Dokument beschreibt alle unterstützten Sensoren, ihre Anbindung an den
> KC868-A8, die elektrische Verdrahtung, Kalibrierung der chemischen Sensoren
> sowie den Simulationsmodus für Entwicklungs- und Testzwecke.

---

## 📋 Inhaltsverzeichnis

- [Übersicht - Alle Sensoren](#-übersicht---alle-sensoren)
- [pH-Sensor](#-ph-sensor)
- [ORP-Sensor](#-orp-sensor)
- [Temperatursensor Wasser (DS18B20)](#-temperatursensor-wasser-ds18b20)
- [Temperatursensor Luft (DHT22)](#-temperatursensor-luft-dht22)
- [Drucksensor](#-drucksensor)
- [Sensor-Simulation](#-sensor-simulation)
- [ADS1115 Belegung](#-ads1115-belegung)
- [Fehlerbehandlung](#-fehlerbehandlung)

---

## 📊 Übersicht - Alle Sensoren

| Sensor | Typ | Interface | GPIO/ADC | Aktualisierung | Genauigkeit |
|--------|-----|-----------|----------|---------------|-------------|
| pH | pH-Elektrode + BNC → ADC | ADS1115 (I2C), Kanal A0 | GPIO 4/5 (SDA/SCL) | 5 s | ±0.1 pH |
| ORP | ORP-Elektrode + BNC → ADC | ADS1115 (I2C), Kanal A1 | GPIO 4/5 (SDA/SCL) | 5 s | ±10 mV |
| Wassertemperatur | DS18B20 | 1-Wire (OneWire) | GPIO 32 | 10 s | ±0.5 °C |
| Lufttemperatur | DHT22 | Digital | GPIO 33 | 10 s | ±0.5 °C |
| Luftfeuchte | DHT22 (integriert) | Digital | GPIO 33 | 10 s | ±2 % |
| Filterdruck | Drucksensor (0.5-4.5V) | ADS1115 (I2C), Kanal A2 | GPIO 4/5 (SDA/SCL) | 15 s | ±0.05 bar |

---

## 🧪 pH-Sensor

### Funktionsprinzip

Eine pH-Elektrode erzeugt eine Spannungsdifferenz proportional zum pH-Wert der
Lösung. Die Spannung wird vom ADS1115 (16-Bit ADC) digitalisiert und in einen
pH-Wert umgerechnet.

- **Messprinzip**: Potentiometrisch (Glasmembranelektrode)
- **Messbereich**: 0–14 pH
- **Genauigkeit**: ±0.1 pH (bei ordnungsgemäßer Kalibrierung)
- **Reaktionszeit**: 90 % in <10 s
- **Ausgangsspannung**: ca. -0,4 V (pH 14) bis +0,4 V (pH 0) typisch
- **Temperaturabhängigkeit**: Ja (kompensiert durch DS18B20)

### Hardware-Anbindung

```
pH-Elektrode (BNC) ──→ BNC-Adapter (Impedanzwandler) ──→ ADS1115 A0
                                                              │
                                                         I2C 0x48
                                                              │
                                                   ESP32 (GPIO 4/5)
```

**Wichtige Hinweise:**

- Die pH-Elektrode hat eine **sehr hohe Innenimpedanz** (~10⁹–10¹² Ω)
- Ein **Impedanzwandler (BNC-Adapter)** ist zwingend erforderlich
- Der BNC-Adapter benötigt 3,3 V Versorgungsspannung
- Das Kabel zwischen Elektrode und Adapter sollte möglichst kurz sein
- pH-Elektroden sind **Verschleißteile** (Lebensdauer ~1–2 Jahre)
- Elektrode muss **feucht gelagert** werden (3M KCl-Lösung)

### Umrechnung

Die Umrechnung von ADC-Spannung in pH-Wert erfolgt über eine **lineare
Kalibrierfunktion** (2-Punkt-Kalibrierung):

```
pH = slope × voltage + intercept
```

Wobei `slope` und `intercept` aus den Kalibrierungswerten berechnet werden:

```
slope     = (7.0 - 4.0) / (ph7Voltage - ph4Voltage)
intercept = 7.0 - (slope × ph7Voltage)
```

| Pufferlösung | Erwartete Spannung | Typischer ADC-Wert (16-Bit) |
|-------------|-------------------|---------------------------|
| pH 4.0 | ~1.86 V | ~23100 |
| pH 7.0 | ~2.06 V | ~25600 |
| pH 10.0 | ~2.26 V | ~28100 |

### Kalibrierung

Eine **2-Punkt-Kalibrierung** mit pH 4.0 und pH 7.0 Pufferlösungen wird empfohlen.

**Kalibrierungs-Schritte:**

1. **Vorbereitung**: Elektrode mit destilliertem Wasser spülen, trocken tupfen
2. **pH 7.0**: Elektrode in pH 7.0 Pufferlösung tauchen, 30 s warten
3. **Messung**: Spannung `ph7Voltage` notieren (Mittelwert über 10 Messungen)
4. **Spülen**: Elektrode erneut spülen und trocken tupfen
5. **pH 4.0**: Elektrode in pH 4.0 Pufferlösung tauchen, 30 s warten
6. **Messung**: Spannung `ph4Voltage` notieren
7. **Eintragen**: Werte in `config.json` unter `calibration.phCalibration` speichern
8. **Upload**: `config.json` neu auf den ESP32 hochladen und neustarten

> **Hinweis:** Die Kalibrierung sollte **monatlich** wiederholt werden.
> Die Werte `ph4Voltage` und `ph7Voltage` sind gerätespezifisch und variieren
> je nach Elektrode und BNC-Adapter.

---

## ⚡ ORP-Sensor

### Funktionsprinzip

Ein ORP-Sensor (Oxidation-Reduction Potential) misst die Fähigkeit einer
Lösung, Elektronen aufzunehmen oder abzugeben. Dies korreliert mit der
Desinfektionsmittelkonzentration im Wasser.

- **Messprinzip**: Potentiometrisch (Edelmetallelektrode)
- **Messbereich**: −2000 mV bis +2000 mV (praktisch: 0–900 mV)
- **Genauigkeit**: ±10 mV
- **Reaktionszeit**: 90 % in <30 s
- **Ausgangsspannung**: 0–2,0 V typisch (nach Impedanzwandler)

### Hardware-Anbindung

```
ORP-Elektrode (BNC) ──→ BNC-Adapter (Impedanzwandler) ──→ ADS1115 A1
                                                               │
                                                          I2C 0x48
                                                               │
                                                    ESP32 (GPIO 4/5)
```

### Umrechnung

ORP wird direkt in Millivolt (mV) gemessen. Die Umrechnung ist:

```
ORP [mV] = ((adc_raw × ADS1115_GAIN) / ADC_MAX) × 1000 × calibration_factor
```

oder vereinfacht (mit 16-Bit ADC bei ±4.096 V Bereich):

```
ORP [mV] = voltage [V] × 1000 × factor
```

### Kalibrierung

ORP-Sensoren sind **relativ stabil** und benötigen weniger häufige
Kalibrierung. Eine ORP-Pufferlösung (z. B. 475 mV) wird zur Überprüfung
verwendet.

**Kalibrierungs-Schritte:**

1. ORP-Elektrode spülen, trocken tupfen
2. In ORP-Pufferlösung tauchen, 60 s warten
3. Gemessenen Wert notieren
4. `factor = erwartet / gemessen` berechnen
5. Wert in `config.json` unter `calibration.orpCalibration.factor` eintragen

---

## 🌡️ Temperatursensor Wasser (DS18B20)

### Funktionsprinzip

Der DS18B20 ist ein digitaler Temperatursensor mit 1-Wire-Interface.
Durch die wasserdichte Edelstahlkapselung ist er für den direkten Einsatz
im Poolwasser geeignet.

- **Messbereich**: −55 °C bis +125 °C
- **Genauigkeit**: ±0.5 °C (−10 °C bis +85 °C)
- **Auflösung**: 9–12 Bit (konfigurierbar, Standard 12 Bit = 0,0625 °C)
- **Interface**: 1-Wire (Daten + Spannungsversorgung über 2 Leitungen, 3. = GND)
- **Maximale Kabellänge**: 30 m (mit 4,7 kΩ Pull-up)

### Hardware-Anbindung

```
DS18B20 (wasserdicht, Edelstahl)
  ├── Rot   → 3,3 V
  ├── Blau/Schwarz → GND
  └── Gelb/Weiß → GPIO 32 (ESP32)
                  └── 4,7 kΩ Pull-up-Widerstand nach 3,3 V
```

### Anschluss-Details

- **Pull-up-Widerstand**: Ein 4,7 kΩ Widerstand zwischen Datenleitung und 3,3 V
  ist **zwingend erforderlich** (auch bei neueren DS18B20-Varianten)
- **Parasitic Power**: Wird nicht unterstützt – der DS18B20 muss über VDD
  versorgt werden (3 Leiter)
- **Kabelverlängerung**: Bei >10 m Kabellänge ggf. Pull-up auf 2,2 kΩ reduzieren

### 1-Wire-Konfiguration (ESP32)

```cpp
// In config.h oder main.cpp:
#define ONE_WIRE_BUS GPIO_NUM_32

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
```

Der Sensor wird über die **DallasTemperature**-Bibliothek angesteuert.
Pro Bus sind mehrere DS18B20 möglich (jeder hat eine eindeutige 64-Bit-ID).
Bei mehreren Sensoren auf einem Bus muss die ID des Pool-Sensors bekannt sein.

### Leseparameter

- **Abtastintervall**: 10 s (konfigurierbar in `tempWaterSensor.updateIntervalMs`)
- **Wandlungszeit**: 750 ms (12-Bit-Auflösung)
- **Mittelwertbildung**: Letzte 3 Messungen werden gemittelt

---

## 💨 Temperatursensor Luft (DHT22)

### Funktionsprinzip

Der DHT22 (auch AM2302) ist ein digitaler Temperatur- und
Luftfeuchtesensor mit kapazitivem Feuchtesensor und Thermistor.

- **Messbereich Temperatur**: −40 °C bis +80 °C
- **Genauigkeit Temperatur**: ±0.5 °C
- **Messbereich Feuchte**: 0–100 % relative Luftfeuchte
- **Genauigkeit Feuchte**: ±2 % (0–100 %)
- **Abtastrate**: Maximal 0,5 Hz (2 s zwischen Messungen!)

### Hardware-Anbindung

```
DHT22
├── Pin 1 → 3,3 V
├── Pin 2 → GPIO 33 (ESP32)
│           └── 10 kΩ Pull-up nach 3,3 V (optional, bei langen Kabeln)
├── Pin 3 → nicht belegt
└── Pin 4 → GND
```

### Wichtige Einschränkungen

- **Mindestabstand zwischen Messungen**: 2 Sekunden (Sensor-spezifisch!)
  Wird dieser nicht eingehalten, gibt der Sensor fehlerhafte Daten zurück
- **Keine wasserdichte Kapselung**: Der DHT22 gehört NICHT ins Poolwasser
  oder in direkte Nähe von Chemikalien
- **Empfindlichkeit**: Der Sensor kann bei längerer direkter Sonneneinstrahlung
  überhitzen (Gehäuse wird thermisch – Standort wählen!)

### Leseparameter

```cpp
// In config.h oder main.cpp:
#define DHT_PIN GPIO_NUM_33
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);
```

- **Abtastintervall**: 10 s (wegen 2 s Minimum zwischen Leseversuchen)
- **Fehlererkennung**: Bei 3 aufeinanderfolgenden fehlerhaften Leseversuchen
  wird der Sensor als "offline" markiert

---

## 🔧 Drucksensor

### Funktionsprinzip

Ein Drucksensor mit analogem Spannungsausgang (0.5–4.5 V) wird am Filter
installiert. Er misst den Druck vor dem Sandfilter – steigender Druck deutet
auf eine Verschmutzung des Filters und damit Rückspülbedarf hin.

- **Messbereich**: 0–1,2 bar (typisch für Sandfilteranlagen)
- **Ausgangssignal**: 0.5–4.5 V (ratiometrisch zur Versorgungsspannung)
- **Genauigkeit**: ±0,5 % vom Messbereich
- **Interface**: Analog → ADS1115 (Kanal A2)

### Hardware-Anbindung

```
Drucksensor (0.5-4.5V)
├── Rot   → 5 V (Versorgung)
├── Schwarz → GND
└── Gelb/Weiß → Spannungsteiler → ADS1115 A2
```

### Spannungsteiler (5 V → 3,3 V)

Der Drucksensor liefert 0.5–4.5 V, der ADS1115 arbeitet aber mit 3,3 V.
Ein **Spannungsteiler 2:1** ist erforderlich:

```
ADS1115_max = 3.3 V (bei interner Referenz)
Sensor_max  = 4.5 V
Teilerverhältnis = 3.3 / 4.5 ≈ 0.733 → 2:1

R1 (zwischen Sensor-Ausgang und ADC) = 10 kΩ
R2 (ADC-Eingang nach GND)           = 10 kΩ
→ Faktor = R2 / (R1 + R2) = 0.5
```

**Oder einfacher:**
```
Sensor-Ausgang ──╮
                 ├── 10 kΩ ──→ ADS1115 A2
GND ─────────────┤          │
                 └── 10 kΩ ──┘
                         ┌── 100 nF (optional, Entstörung)
                         └── GND
```

### Umrechnung

```
Druck [bar] = (adc_voltage × 2 × (4.5 - 0.5) / 3.3) + 0.5

Vereinfacht:
Druck [bar] = adc_voltage × 2.424 + 0.5
```

### Rückspül-Erkennung

- **Grenzwert**: `filterPump.backwashThreshold` (Standard: 0,8 bar)
- **Alarm bei Überschreitung**: MQTT-Nachricht auf `pool/backwash`
- **Alarm bei Unterschreitung nach Überschreitung**: automatische Entwarnung
- **Totband**: 0,05 bar (verhindert schnelles Umschalten)

---

## 🎲 Sensor-Simulation

### Zweck

Der Simulationsmodus ermöglicht den Betrieb des Pool-Controllers **ohne
angeschlossene Sensoren**. Dies ist nützlich für:

- **Entwicklung und Test**: PID-Verhalten testen ohne Chemie
- **Demonstration**: Funktionen vorführen ohne Pool-Zugang
- **Fehlersuche**: System von Sensor-Problemen isolieren

### Aktivierung

Die Simulation wird pro Sensor in `config.json` aktiviert:

```json
{
  "phSensor": {
    "enabled": true,
    "simulate": true,
    "simMin": 6.8,
    "simMax": 7.6,
    "simDriftPerHour": 0.1
  }
}
```

Setze `simulate: true` für den gewünschten Sensor.

### Algorithmus

Die Simulation verwendet einen **Random-Walk** mit Drift-Begrenzung:

```
Algorithmus SIMULATE(alt, min, max, drift, delta_s):
    // delta_s = Sekunden seit letzter Aktualisierung
    max_aenderung = drift × (delta_s / 3600.0)  // Drift pro Schritt
    aenderung = random(-max_aenderung, +max_aenderung)
    neu = alt + aenderung
    return CLAMP(neu, min, max)
```

### Simulationsparameter

| Sensor | typ. simMin | typ. simMax | typ. Drift | Verhalten |
|--------|-------------|-------------|------------|-----------|
| pH | 6.8 | 7.6 | 0.1/h | Realistische pH-Schwankungen |
| ORP | 500 | 800 | 20/h | Langsame ORP-Änderungen |
| Wassertemperatur | 20 | 35 | 0.5/h | Sehr träge (große Wassermenge) |
| Lufttemperatur | 10 | 40 | 2.0/h | Schnellere Änderungen |
| Luftfeuchte | 40 | 80 | 5.0/h | Tagesabhängig |
| Druck | 0.2 | 0.8 | 0.0 | Konstant (Rückspül-Trigger manuell) |

### Erkennung: Simulation vs. Realität

- **Simulation überschreibt NIE einen angeschlossenen Sensor**
- Bei Verbindung eines echten Sensors wird automatisch umgeschaltet
- Erkennungskriterien:
  - pH/ORP: ADC-Wert ändert sich → Hardware erkannt
  - DS18B20: 1-Wire-ROM erfolgreich gelesen → Hardware erkannt
  - DHT22: Erfolgreiche CRC-Prüfung → Hardware erkannt
- Log-Meldung bei Umschaltung: `"Sensor X: Hardware erkannt, Simulation deaktiviert"`

### Fallback bei Sensorausfall

Fällt ein Sensor während des Betriebs aus (z. B. Kabelbruch), wird
**nicht** automatisch in den Simulationsmodus geschaltet – das wäre
gefährlich (Dosierung ohne echte Messwerte). Stattdessen:

1. Letzter gültiger Wert wird für max. 5 Minuten gehalten
2. Nach 5 Minuten wird der Sensor als "offline" markiert
3. Die PID-Regelung wird **gestoppt** (keine Dosierung ohne gültige Messwerte)
4. Die Filterpumpe läuft mit der letzten berechneten Laufzeit weiter
5. MQTT-Alarm wird gesendet (`pool/status`: `"sensor_offline"`)

---

## 📍 ADS1115 Belegung

### I2C-Adresse

Der ADS1115 hat die **Standard-I2C-Adresse 0x48** (ADDR-Pin = GND).
Alternativadressen:

| ADDR-Verbindung | I2C-Adresse |
|----------------|-------------|
| GND | 0x48 (Standard) |
| VDD | 0x49 |
| SDA | 0x4A |
| SCL | 0x4B |

### Kanalbelegung

| Kanal | Funktion | Signalbereich | Maximalwert (16-Bit) |
|-------|----------|--------------|---------------------|
| A0 | pH-Elektrode | −0,4 V bis +0,4 V (nach BNC-Adapter) | ±32768 |
| A1 | ORP-Elektrode | 0–2,0 V | ±32768 |
| A2 | Drucksensor (0.5–4.5V → Teiler → 0.25–2,25V) | 0,25–2,25 V | ±32768 |
| A3 | Frei (Reserve) | — | — |

### ADC-Konfiguration

| Parameter | Wert |
|-----------|------|
| IC | ADS1115 |
| Auflösung | 16 Bit (±32768) |
| I2C-Adresse | 0x48 |
| PGA (programmable gain) | ±4.096 V (FS=4.096V) |
| Sampling-Rate | 860 SPS (max) |
| Vergleichsspannung | interner Reference |

```cpp
// Initialisierung:
Adafruit_ADS1115 ads;
ads.setGain(GAIN_TWOTHIRDS);  // ±6.144 V (1x = ±4.096 V)
ads.begin(0x48);

// Lesen:
int16_t adc0 = ads.readADC_SingleEnded(0);  // pH
int16_t adc1 = ads.readADC_SingleEnded(1);  // ORP
int16_t adc2 = ads.readADC_SingleEnded(2);  // Druck

// Spannung:
float voltage = ads.computeVolts(adc0);
```

---

## ⚠️ Fehlerbehandlung

### Sensor offline

| Symptom | Mögliche Ursache | Lösung |
|---------|-----------------|--------|
| pH = 0.0 oder max | ADC übersteuert / kein Signal | BNC-Adapter prüfen |
| ORP konstant 0 mV | Elektrode trocken / kein Kontakt | Elektrode in Wasser tauchen |
| DS18B20: -127 °C | Kurzschluss / kein Sensor | Verdrahtung prüfen |
| DHT22: NaN | Timeout / CRC-Fehler | 2s-Abstand einhalten, Pull-up prüfen |
| Druck konstant 0 bar | Spannungsteiler defekt | Spannung an A2 messen |

### Log-Meldungen

Bei `logLevel: 3` (DEBUG) werden detaillierte Sensor-Rohdaten ausgegeben:

```
[D] pH: raw=23100, voltage=1.86V, pH=7.02
[D] ORP: raw=18500, voltage=1.48V, ORP=650mV
[D] DS18B20: addr=0x28a1b2c3d4e5, temp=24.5°C
[D] DHT22: temp=28.3°C, hum=62.4%
[D] Druck: raw=12500, voltage=1.00V, teiler=2.00V, druck=0.52bar
```

### Watchdog

Wenn ein Sensor länger als 5 Minuten keine gültigen Daten liefert:

1. **Log-Warnung**: `[W] Sensor pH: offline seit 300s`
2. **MQTT-Status**: `pool/status` → `{"ph": "offline"}`
3. **PID-Abschaltung**: Nur der betroffene Regelkreis wird gestoppt
4. **Automatische Wiederaufnahme**: Bei erneuter gültiger Messung
