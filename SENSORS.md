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
- [Temperatursensor Luft (DS18B20)](#-temperatursensor-luft-ds18b20)
- [Drucksensor](#-drucksensor)
- [Sensor-Simulation](#-sensor-simulation)
- [ADS1115 Belegung](#-ads1115-belegung)
- [Fehlerbehandlung](#-fehlerbehandlung)

---

## 📊 Übersicht - Alle Sensoren

| Sensor | Typ | Interface | Pin(s) | Aktualisierung | Genauigkeit |
|--------|-----|-----------|--------|---------------|-------------|
| pH | pH-Elektrode + BNC → ADC | ADS1115 (I2C), Kanal A0 | SDA=4, SCL=5 | 2 s | ±0.1 pH |
| ORP | ORP-Elektrode + BNC → ADC | ADS1115 (I2C), Kanal A1 | SDA=4, SCL=5 | 2 s | ±10 mV |
| Wassertemperatur | DS18B20 (wasserdicht) | 1-Wire (OneWire) | GPIO 14 | 2 s | ±0.5 °C |
| Lufttemperatur | DS18B20 | 1-Wire (OneWire) | GPIO 13 | 2 s | ±0.5 °C |
| Filterdruck | Drucksensor (analog) | ADS1115 (I2C), Kanal A2 | SDA=4, SCL=5 | 2 s | ±0.05 bar |

> **Hinweis:** Beide Temperatursensoren sind DS18B20 — es gibt keinen DHT22
> im aktuellen Code. Die Lufttemperatur wird über einen zweiten DS18B20
> auf einem separaten OneWire-Bus (GPIO 13) gemessen. Keine Luftfeuchte-Messung.

---

## 🧪 pH-Sensor

### Funktionsprinzip

Eine pH-Elektrode erzeugt eine Spannungsdifferenz proportional zum pH-Wert der
Lösung. Die Spannung wird vom ADS1115 (16-Bit ADC) digitalisiert und in einen
pH-Wert umgerechnet.

- **Messprinzip**: Potentiometrisch (Glasmembranelektrode)
- **Messbereich**: 0–14 pH
- **Genauigkeit**: ±0.1 pH (bei ordnungsgemäßer Kalibrierung)
- **Ausgangsspannung**: ca. -0,4 V (pH 14) bis +0,4 V (pH 0) typisch
- **Temperaturabhängigkeit**: Ja (kompensiert durch DS18B20 Wassertemperatur)

### Hardware-Anbindung

```
pH-Elektrode (BNC) ──→ BNC-Adapter (Impedanzwandler) ──→ ADS1115 A0 (0x48)
                                                              │
                                                         I2C (SDA=4, SCL=5)
                                                              │
                                                           ESP32
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
pH = calSlope × voltage + calIntercept
```

Wobei `calSlope` und `calIntercept` aus den Kalibrierungswerten berechnet werden:

```
calSlope     = (7.0 - 4.0) / (voltagePH7 - voltagePH4)
calIntercept = 7.0 - (calSlope × voltagePH7)
```

### Kalibrierung

Die PHSensor-Klasse unterstützt `setCalibration(voltageAtPH7, voltageAtPH4)`.

**Kalibrierungs-Schritte:**

1. **Vorbereitung**: Elektrode mit destilliertem Wasser spülen, trocken tupfen
2. **pH 7.0**: Elektrode in pH 7.0 Pufferlösung tauchen, 30 s warten
3. **Messung**: Spannung notieren (Mittelwert über 10 Messungen)
4. **Spülen**: Elektrode erneut spülen und trocken tupfen
5. **pH 4.0**: Elektrode in pH 4.0 Pufferlösung tauchen, 30 s warten
6. **Messung**: Spannung notieren
7. **Setzen**: `sensor.setCalibration(voltagePH7, voltagePH4)` aufrufen

> **Hinweis:** Die Kalibrierung sollte **monatlich** wiederholt werden.

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

### Hardware-Anbindung

```
ORP-Elektrode (BNC) ──→ BNC-Adapter (Impedanzwandler) ──→ ADS1115 A1 (0x48)
                                                               │
                                                          I2C (SDA=4, SCL=5)
                                                               │
                                                             ESP32
```

---

## 🌡️ Temperatursensor Wasser (DS18B20)

### Funktionsprinzip

Der DS18B20 ist ein digitaler Temperatursensor mit 1-Wire-Interface.
Durch die wasserdichte Edelstahlkapselung ist er für den direkten Einsatz
im Poolwasser geeignet.

- **Messbereich**: −55 °C bis +125 °C
- **Genauigkeit**: ±0.5 °C (−10 °C bis +85 °C)
- **Auflösung**: 9–12 Bit (Standard 12 Bit = 0,0625 °C)
- **Interface**: 1-Wire
- **Pin**: **GPIO 14**

### Hardware-Anbindung

```
DS18B20 (wasserdicht, Edelstahl)
  ├── Rot   → 3,3 V
  ├── Blau/Schwarz → GND
  └── Gelb/Weiß → GPIO 14 (ESP32)
                  └── 4,7 kΩ Pull-up-Widerstand nach 3,3 V
```

---

## 🌡️ Temperatursensor Luft (DS18B20)

Die Lufttemperatur wird über einen **zweiten DS18B20** auf einem separaten
OneWire-Bus gemessen.

- **Interface**: 1-Wire
- **Pin**: **GPIO 13**
- **Label**: `"Air Temperature"`
- **Keine Luftfeuchte-Messung** im aktuellen Code

### Hardware-Anbindung

```
DS18B20
  ├── Rot   → 3,3 V
  ├── Blau/Schwarz → GND
  └── Gelb/Weiß → GPIO 13 (ESP32)
                  └── 4,7 kΩ Pull-up-Widerstand nach 3,3 V
```

---

## 🔧 Drucksensor

### Funktionsprinzip

Ein Drucksensor mit analogem Spannungsausgang wird am Filter installiert.
Er misst den Druck vor dem Sandfilter – steigender Druck deutet auf
Rückspülbedarf hin.

- **Messbereich**: 0–2,5 bar (konfigurierbar)
- **Ausgangssignal**: 0.5–4.5 V (ratiometrisch)
- **Interface**: Analog → ADS1115 (Kanal A2)
- **Rückspül-Schwelle**: über `setBackwashThreshold(bar)` konfigurierbar

### Hardware-Anbindung

```
Drucksensor (0.5-4.5V)
├── Rot   → 5 V (Versorgung)
├── Schwarz → GND
└── Gelb/Weiß → Spannungsteiler → ADS1115 A2
```

### Spannungsteiler (5 V → 3,3 V)

Der Drucksensor liefert 0.5–4.5 V, der ADS1115 arbeitet mit 3,3 V.
Ein **Spannungsteiler 2:1** ist erforderlich:

```
Sensor-Ausgang ──╮
                 ├── 10 kΩ ──→ ADS1115 A2
GND ─────────────┤          │
                 └── 10 kΩ ──┘
```

### Kalibrierung

Die PressureSensor-Klasse unterstützt:
```cpp
sensor.setPressureRange(voltageAt0Bar, voltageAtMaxBar, maxPressureBar);
```

---

## 🎲 Sensor-Simulation

### Zweck

Der Simulationsmodus ermöglicht den Betrieb des Pool-Controllers **ohne
angeschlossene Sensoren**. Im aktuellen Code ist die Simulation immer aktiv
als Fallback, wenn kein echter Sensor erkannt wird.

### Algorithmus

Die Simulation verwendet einen **Random-Walk mit pumpenabhängiger Drift**:

- **pH-Simulation** (pumpenabhängig):
  - Natürliche Drift: +0.15/h (pH steigt ohne Dosierung → CO₂-Entzug)
  - Pumpen-Effekt: −0.8/h (pH-Minus-Dosierung senkt pH)
- **ORP-Simulation** (pumpenabhängig):
  - Natürlicher Zerfall: −8 mV/h (Chlor baut ab)
  - Pumpen-Effekt: +40 mV/h (Chlor-Dosierung erhöht ORP)

```
Algorithmus SIMULATE():
    driftRate = pumpeAktiv ? pumpDriftPerHour : naturalDriftPerHour
    aenderung = driftRate × (delta_s / 3600.0)
    neu = alt + aenderung
    return CLAMP(neu, min, max)
```

### Aktivierung

Die Simulation wird pro Sensor in `config.json` aktiviert:

```json
{
  "phSensor": {
    "enabled": true,
    "simulate": true,
    "simMin": 6.8,
    "simMax": 7.6,
    "simDriftPerHour": 0.05
  }
}
```

Setze `simulate: true` für den gewünschten Sensor.

### Fallback bei Sensorausfall

Fällt ein Sensor während des Betriebs aus:
1. Der Simulations-Fallback liefert weiter Werte (Random Walk)
2. Die PID-Regelung läuft weiter (mit simulierten Werten!)
3. `isConnected()` des echten Sensors liefert `false`

---

## 📍 ADS1115 Belegung

### I2C-Adresse

Standard: **0x48** (ADDR-Pin = GND).

| ADDR-Verbindung | I2C-Adresse |
|----------------|-------------|
| GND | 0x48 (Standard) |
| VDD | 0x49 |
| SDA | 0x4A |
| SCL | 0x4B |

### Kanalbelegung

| Kanal | Sensor | Klasse |
|-------|--------|--------|
| A0 | pH-Elektrode | `PHSensor(0x48, 0)` |
| A1 | ORP-Elektrode | `ORPSensor(0x48, 1)` |
| A2 | Drucksensor | `PressureSensor(0x48, 2)` |
| A3 | Frei | — |

### ADC-Konfiguration

```cpp
// I2C-Pins (SensorManager.cpp):
#define I2C_SDA 4
#define I2C_SCL 5

// Bibliothek: Adafruit ADS1X15 (v2.4.0)
Adafruit_ADS1115 ads;
ads.begin(0x48);
int16_t adc0 = ads.readADC_SingleEnded(0);  // pH
int16_t adc1 = ads.readADC_SingleEnded(1);  // ORP
int16_t adc2 = ads.readADC_SingleEnded(2);  // Druck
```

---

## ⚠️ Fehlerbehandlung

### Sensor offline

| Symptom | Mögliche Ursache | Lösung |
|---------|-----------------|--------|
| pH = NaN oder max | ADC übersteuert / kein Signal | BNC-Adapter prüfen |
| ORP konstant 0 mV | Elektrode trocken / kein Kontakt | Elektrode in Wasser tauchen |
| DS18B20: −127 °C | Kurzschluss / kein Sensor | Verdrahtung prüfen |
| Druck konstant 0 bar | Spannungsteiler defekt | Spannung an A2 messen |

### Verhalten bei Ausfall

- **Echter Sensor offline** → Simulations-Fallback übernimmt automatisch
- **PID-Regelung läuft weiter** mit simulierten Werten
- **Filterpumpe läuft** mit letzter berechneter Laufzeit
- Bei Log-Level 1 werden Warnungen ausgegeben
- Kein MQTT-Alarm bei Sensorausfall im aktuellen Code

### Debug-Ausgabe

Bei `logLevel: 1` werden Statusmeldungen der Sensoren ausgegeben:
```
[I] pH sensor initialized
[I] ORP sensor initialized
[I] Water temp sensor initialized
[I] Air temp sensor initialized
[I] Pressure sensor initialized
```