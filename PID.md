# PID-Regelungen – Poolchemie

> Dieses Dokument beschreibt die beiden PID-Regelkreise des Pool-Controllers
> – für pH-Wert und Chlor (ORP-basiert) – sowie die zugehörigen
> Sicherheitsmechanismen, Tuning-Strategien und Diagnosemöglichkeiten.

---

## 📋 Inhaltsverzeichnis

- [Grundlagen](#-grundlagen)
- [PID-Regler im Pool-Kontext](#-pid-regler-im-pool-kontext)
- [pH-Regelung](#-ph-regelung)
- [Chlor-Regelung (ORP-basiert)](#-chlor-regelung-orp-basiert)
- [Tuning-Guide](#-tuning-guide)
- [Sicherheitsmechanismen](#-sicherheitsmechanismen)
- [Diagnose & Monitoring](#-diagnose--monitoring)
- [Fehlersuche](#-fehlersuche)

---

## 📐 Grundlagen

### Was ist ein PID-Regler?

Ein **PID-Regler** (Proportional-Integral-Differential-Regler) ist ein
Regelkreis-Algorithmus, der kontinuierlich einen Sollwert mit einem
Istwert vergleicht und eine Stellgröße berechnet, um die Differenz
(=Regelabweichung) zu minimieren.

```
Stellgröße(t) = Kp × e(t) + Ki × ∫e(t) dt + Kd × de(t)/dt
```

Wobei:
- **e(t)** = Regelabweichung (Sollwert − Istwert)
- **Kp** = Proportionalverstärkung
- **Ki** = Integralverstärkung
- **Kd** = Differentialverstärkung

### Die drei Anteile

#### P-Anteil (Proportional)

Reagiert **sofort** auf die aktuelle Abweichung. Je größer die Abweichung,
desto stärker die Reaktion.

- **Positiv**: Schnelle Anfangsreaktion
- **Negativ**: Kann nicht allein den Sollwert erreichen (bleibende Regelabweichung)
- **Zu hoch**: Oszillation (Überschwingen)

#### I-Anteil (Integral)

Summiert die Abweichung über die Zeit auf. **Beseitigt die bleibende
Regelabweichung** des P-Anteils.

- **Positiv**: Erreicht den Sollwert exakt
- **Negativ**: Kann "windup" verursachen (I-Anteil schießt über)
- **Zu hoch**: Langsames Überschwingen

#### D-Anteil (Differential)

Reagiert auf die **Änderungsgeschwindigkeit** der Abweichung. Wirkt wie ein
"Dämpfer".

- **Positiv**: Reduziert Überschwingen, stabilisiert
- **Negativ**: Empfindlich gegenüber Rauschen
- **Zu hoch**: Nervöses Verhalten, Instabilität

### Anwendung auf Poolchemie

Die Poolchemie-Regelung unterscheidet sich von industriellen PID-Anwendungen:

| Eigenschaft | Poolchemie | Industriell typisch |
|------------|-----------|-------------------|
| **Totzeit** | Minuten (Umwälzung) | Millisekunden |
| **Reaktionszeit** | 30–120 min | 1–10 s |
| **Störgrößen** | Wetter, Nutzung, pH→ORP-Kopplung | Konstant |
| **Messrauschen** | Gering (Mittelwert) | Variabel |
| **Stellglied** | Relais (Ein/Aus) | Analog (0-100%) |
| **Regelstrategie** | Zeitproportional | Kontinuierlich |

> **Wichtig:** Die PID-Regler im Pool-Controller arbeiten als
> **zeitproportionale Regler** – die Stellgröße (0-100 %) wird in Ein/Aus-Zyklen
> der Dosierpumpe übersetzt. Eine Stellgröße von 50 % bedeutet: Pumpe läuft
> 50 % der Zykluszeit.

---

## 🧪 pH-Regelung

### Zielsetzung

Der pH-Wert des Poolwassers soll auf einem vorgegebenen Sollwert gehalten
werden (typisch **pH 7,2**). Die Dosierung erfolgt ausschließlich **mit Säure**
(pH-Minus), da der pH-Wert im Pool natürlicherweise steigt (CO₂-Entzug).

**Warum pH 7,2?**

- **Komfort**: Optimaler pH-Wert für Badegäste (Augen-/Hautreizung minimal)
- **Wirksamkeit**: Chlor desinfiziert am besten bei pH 7,0–7,4
- **Materialschutz**: Korrosion von Metallteilen minimal
- **Flockung**: Aluminiumflockungsmittel arbeiten optimal bei pH 7,0–7,4

### Regelkreis

```
                         Störgrößen
                            ↓
    Sollwert ──→ ┌─────┐    ↓    ┌─────────┐    Istwert
    (pH 7.2) ──→ │ PID │───→│ Pumpe │───→│ Pool │───→ pH
                  └─────┘    └─────────┘    └───────┘
                     ↑                       │
                     └───── pH-Sensor ←──────┘
```

### Prozessbeschreibung

1. **pH-Sensor** misst den aktuellen pH-Wert (alle 5 s)
2. **PID-Regler** berechnet Stellgröße aus Soll-/Istwert-Differenz
3. **Stellgröße** wird in Einschaltdauer der Dosierpumpe übersetzt
4. **Säure wird dosiert** → pH-Wert sinkt
5. **Filterpumpe** sorgt für Durchmischung
6. **Nächster Messzyklus** beginnt

### Regelstrategie

- **Nur Absenken**: Die pH-Pumpe dosiert ausschließlich Säure (pH-Minus)
- **Kein Anheben**: Ein zu niedriger pH-Wert wird nicht durch Base korrigiert
  (sondern sinkt durch CO₂-Austritt natürlicherweise)
- **Deadband**: ±0,05 pH um den Sollwert (verhindert Kurzzyklen)
- **Anti-Windup**: Der I-Anteil wird begrenzt, damit er nicht "aufläuft"

### Stellgrößen-Begrenzung

| Parameter | Wert | Bedeutung |
|-----------|------|-----------|
| `outputMin` | 0,0 % | Minimale Pumpenleistung (de facto Aus) |
| `outputMax` | 100,0 % | Maximale Pumpenleistung |
| `minOnTimeSec` | 30 s | **Kürzester Einschaltzyklus** (Sicherheit) |
| `minOffTimeSec` | 120 s | **Kürzester Ausschaltzyklus** (Sicherheit) |

Die zeitproportionale Umsetzung:
```
Stellgröße = 50 %, Zykluszeit = 300 s
→ Pumpe EIN für 150 s, dann AUS für 150 s
→ Aber minOnTime = 30 s, minOffTime = 120 s
→ Tatsächlich: EIN 150 s, AUS 150 s (beide > Minimum → OK)
```

---

## 🧪 Chlor-Regelung (ORP-basiert)

### Zielsetzung

Die Desinfektionsleistung des Poolwassers soll auf einem sicheren Niveau
gehalten werden. Da freies Chlor direkt nicht einfach messbar ist, wird
der **ORP-Wert (Oxidation-Reduction Potential)** als Indikator verwendet.

**Typischer ORP-Sollwert: 650–750 mV**

### Zusammenhang ORP ↔ Freies Chlor

Der ORP-Wert korreliert mit der Desinfektionswirkung, ist aber **nicht linear**
zum freien Chlorgehalt:

| ORP (mV) | Freies Chlor (ca.)* | Desinfektion |
|----------|--------------------|-------------|
| <450 | <0,3 ppm | ❌ Unzureichend |
| 450–600 | 0,3–1,0 ppm | ⚠️ Gering |
| 600–700 | 1,0–2,0 ppm | ✅ Akzeptabel |
| 700–800 | 2,0–4,0 ppm | ✅ Gut |
| >800 | >4,0 ppm | ⚠️ Überdosiert |

*Abhängig von pH, Temperatur und Cyanursäure – Richtwerte bei pH 7,2, 25°C, ohne CYA

**Einflussfaktoren auf ORP:**

| Faktor | Änderung | ORP-Effekt |
|--------|----------|-----------|
| pH ↑ (alkalischer) | +0,1 | ORP ↓ ca. 30 mV |
| Temperatur ↑ | +5 °C | ORP ↓ ca. 20 mV |
| Cyanursäure ↑ | +30 ppm | ORP ↓ ca. 100 mV |
| Freies Chlor ↑ | +1 ppm | ORP ↑ ca. 50 mV |

### Regelkreis

Der ORP-Regelkreis ist analog zum pH-Regelkreis aufgebaut, jedoch mit
**trägerem Verhalten** und niedrigeren PID-Werten:

```
                         Störgrößen (pH, Temperatur, CYA, Nutzung)
                            ↓
    Sollwert ──→ ┌─────┐    ↓    ┌───────────┐    Istwert
    (650 mV) ──→ │ PID │───→│ Chlor-Pumpe │───→│ Pool │───→ ORP
                  └─────┘    └───────────┘    └───────┘
                     ↑                          │
                     └── ORP-Sensor ←───────────┘
```

### Besonderheiten

- **Längere Totzeit**: ORP-Änderungen brauchen 30–120 min (Chlor braucht Zeit)
- **Gekoppelt mit pH**: pH-Änderungen beeinflussen ORP
- **Nichtlinear**: ORP reagiert im unteren Bereich empfindlicher
- **Cyanursäure**: Reduziert ORP massiv (Chlor liegt gebunden vor)
- **Temperatur**: Warmes Wasser reduziert ORP bei gleichem Chlorgehalt

---

## 🎛️ Tuning-Guide

### Grundregeln für das PID-Tuning

1. **Eine Regelgröße nach der anderen tunen**: Erst pH, dann ORP
2. **Änderungen brauchen Zeit**: Zwischen Änderungen 2–4 Stunden warten
3. **Mit Kp starten**: P-Anteil zuerst, dann I, zuletzt D
4. **Aufschreiben**: Alle Änderungen protokollieren
5. **Sicherheit geht vor**: Extreme Werte vermeiden

### Schritt-für-Schritt-Tuning pH

#### Schritt 1: P-Anteil einstellen

```
Ki = 0, Kd = 0, Kp variieren
```

1. Starte mit **Kp = 1.0**
2. Beobachte das Verhalten über 2–4 h
3. Erhöhe Kp schrittweise um 0,5
4. Sobald der pH-Wert oszilliert, Kp um 30 % zurücknehmen
5. Optimal: pH erreicht Sollwert, bleibt aber etwas darunter (Regelabweichung)

#### Schritt 2: I-Anteil hinzufügen

```
Kp = optimal (o. g.), Ki = 0.02, Kd = 0
```

1. Starte mit **Ki = 0.02**
2. Beobachte über 4–8 h
3. Der I-Anteil beseitigt die Regelabweichung
4. Erhöhe Ki schrittweise (0.02 → 0.05 → 0.1)
5. Bei Überschwingen: Ki reduzieren

#### Schritt 3: D-Anteil zur Stabilisierung

```
Kp = optimal, Ki = optimal, Kd = 0.2
```

1. Starte mit **Kd = 0.2**
2. Der D-Anteil dämpft Überschwingen
3. Bei Rauschanfälligkeit: Kd reduzieren

#### Empfohlene Werte

| Parameter | Standard | Aggressiv | Konservativ |
|-----------|----------|-----------|-------------|
| Kp | 2,0 | 3,0 | 1,0 |
| Ki | 0,1 | 0,2 | 0,05 |
| Kd | 0,5 | 0,8 | 0,2 |
| setpoint | 7,2 | 7,0 | 7,4 |
| minOnTime | 30 s | 20 s | 60 s |
| minOffTime | 120 s | 60 s | 180 s |
| deadband | 0,05 | 0,02 | 0,10 |

### Schritt-für-Schritt-Tuning ORP

ORP-Tuning folgt dem gleichen Schema, ist aber **träger**:

#### Empfohlene Werte

| Parameter | Standard | Aggressiv | Konservativ |
|-----------|----------|-----------|-------------|
| Kp | 1,0 | 1,5 | 0,5 |
| Ki | 0,05 | 0,10 | 0,02 |
| Kd | 0,2 | 0,3 | 0,1 |
| setpoint | 650 mV | 700 mV | 600 mV |
| minOnTime | 60 s | 30 s | 120 s |
| minOffTime | 180 s | 120 s | 300 s |
| deadband | 10 mV | 5 mV | 20 mV |

### Tuning-Richtlinie nach Poolgröße

| Poolgröße | pH Kp | pH Ki | ORP Kp | ORP Ki | pH-Minus pro Zyklus |
|-----------|-------|-------|--------|--------|-------------------|
| <20 m³ | 1,5 | 0,05 | 0,8 | 0,03 | Vorsichtig dosieren |
| 20–50 m³ | 2,0 | 0,10 | 1,0 | 0,05 | Normal |
| 50–80 m³ | 2,5 | 0,15 | 1,2 | 0,08 | Etwas mehr |
| >80 m³ | 3,0 | 0,20 | 1,5 | 0,10 | Höhere Durchsatzrate |

### Praxis-Tipps

1. **Störgrößen erkennen**: Nach einer Badebelastung oder Regen steigt der pH
   (und ORP sinkt). Das ist normal – PID reagiert automatisch.
2. **Zeit geben**: ORP-Änderungen brauchen bis zu 2 Stunden. Nicht zu früh
   nachjustieren!
3. **Wetter beachten**: Bei Hitze steigt der Chlorverbrauch → ORP sinkt.
   Die Temperaturen erhöhen die Filterlaufzeit → mehr Umwälzung → bessere Regelung.
4. **Cyanursäure messen**: Hohe Werte (>50 ppm) machen ORP-Regelung schwierig.
   Abhilfe: Teilwasserwechsel oder auf nicht-stabilisiertes Chlor umsteigen.
5. **Dokumentation**: Werte in Home Assistant tracken → Muster erkennen

---

## 🔒 Sicherheitsmechanismen

### Kritische Sicherheitsfunktionen

Der Pool-Controller verfügt über mehrere, redundant ausgelegte
Sicherheitsmechanismen:

| Mechanismus | Beschreibung | Auswirkung |
|-------------|-------------|-----------|
| Filterpumpen-Interlock | Dosierung NUR bei laufender Filterpumpe | Verhindert Chemie-Stau |
| Max. Einschaltdauer | Jede Pumpe hat `maxOnTimeSec` (Default 300 s) | Hardware-Timeout |
| Min. Ausschaltzeit | `minOffTimeSec` verhindert Kurzzyklen | Schützt Dosierpumpen |
| Extremwert-Sperre | pH < 6,5 oder pH > 8,0 → Stop | Verhindert Chemie-Unfälle |
| Sensor-Offline | Kein Sensorwert → PID stoppt | Keine Dosierung blind |
| Watchdog | Zeitüberschreitung → Hardware-Reset | System bleibt lauffähig |
| MQTT-Status | `pool/status` = `alarm` bei Fehler | Benachrichtigung über HA |

### Filterpumpen-Interlock (Details)

Die Chemie-Dosierung (pH und Chlor) ist **NUR aktiv**, wenn die Filterpumpe
läuft. Dies ist der wichtigste Sicherheitsmechanismus:

```
if (filterPumpe läuft) {
    pH-PID darf dosieren
    Chlor-PID darf dosieren
} else {
    ALLE Dosierpumpen AUS
    PID-Regler pausiert (I-Anteil wird NICHT verändert)
}
```

**Begründung:**
- Ohne Umwälzung sammelt sich Chemie lokal → Materialschäden
- Keine gleichmäßige Verteilung im Becken
- Gefahr von lokalen Überkonzentrationen

### Extremwert-Sperre

```cpp
if (ph < 6.5 || ph > 8.0) {
    pH-Pumpe AUS (sofort)
    Chlor-Pumpe AUS (sofort)
    Alarm auslösen (MQTT)
    Warte auf manuellen Reset ODER automatische Rückkehr in 7.0–7.6
}
```

### Maximale Einschaltdauer

Jeder Dosierpumpe ist eine maximale Einschaltdauer pro Zyklus zugewiesen
(`maxOnTimeSec`). Wird dieser Wert überschritten, wird die Pumpe
**zwangsweise abgeschaltet** – unabhängig von der PID-Stellgröße.

Dies verhindert:
- **Festkleben eines Relais**: Max. 5 Minuten Dosierung → Schaden minimiert
- **Leerer Chemikalienbehälter**: Erkennbar an dauerhaftem Dosierbedarf ohne Wirkung
- **PID-Windup**: Schutz vor aufgelaufenem I-Anteil

### Visualisierung der Sicherheitslogik

```
                    ┌─────────────────────────────────┐
                    │         PID-Berechnung           │
                    │  Stellgröße = PID_Wert           │
                    └──────────┬──────────────────────┘
                               │
                    ┌──────────▼──────────────────────┐
                    │  Filterpumpe läuft?              │
                    │  → NEIN → Pumpe AUS, PID pausiert│
                    │  → JA → Weiter                   │
                    └──────────┬──────────────────────┘
                               │
                    ┌──────────▼──────────────────────┐
                    │  pH im Bereich 6.5–8.0?         │
                    │  → NEIN → Pumpe AUS, ALARM      │
                    │  → JA → Weiter                   │
                    └──────────┬──────────────────────┘
                               │
                    ┌──────────▼──────────────────────┐
                    │  Stellgröße > 0?                │
                    │  → NEIN → Pumpe AUS             │
                    │  → JA → Weiter                   │
                    └──────────┬──────────────────────┘
                               │
                    ┌──────────▼──────────────────────┐
                    │  Pumpe einschalten:              │
                    │  - minOffTime abgelaufen?        │
                    │  - maxOnTime nicht überschritten │
                    └─────────────────────────────────┘
```

---

## 📊 Diagnose & Monitoring

### PID-Werte über MQTT

Der Controller sendet regelmäßig Diagnose-JSON auf:

```
pool/ph/pid   → { "setpoint": 7.2, "input": 7.15, "output": 35.2, "p": 0.1, "i": 0.05, "d": 0.0 }
pool/orp/pid  → { "setpoint": 650, "input": 620, "output": 45.0, "p": 30.0, "i": 10.0, "d": 5.0 }
```

| Feld | Beschreibung |
|------|-------------|
| `setpoint` | Aktueller Sollwert |
| `input` | Aktueller Istwert (letzte Messung) |
| `output` | Stellgröße (0–100 %) |
| `p` | P-Anteil (aktuell) |
| `i` | I-Anteil (aktuell) |
| `d` | D-Anteil (aktuell) |

### Pumpen-Statistiken

```
pool/ph/pump/stats → { "runtime_today": 340, "cycles_today": 5, "last_on": "2024-06-15T14:30:22" }
```

| Feld | Beschreibung |
|------|-------------|
| `runtime_today` | Laufzeit heute (Sekunden) |
| `cycles_today` | Anzahl Einschaltzyklen heute |
| `last_on` | Letzter Einschaltzeitpunkt (ISO 8601) |
| `last_off` | Letzter Ausschaltzeitpunkt (ISO 8601) |

### Debug-Logging

Bei `logLevel: 3` (DEBUG) werden detaillierte PID-Diagnosedaten auf der
seriellen Schnittstelle ausgegeben:

```
[D] pH-PID: sp=7.20, in=7.15, err=0.05, P=0.10, I=0.05, D=0.00, out=35.2%
[D] pH-PID: out=35.2% → 300s Zyklus → ON 105.6s (minOn=30s ✅, minOff=120s ✅)
[D] pH-Pumpe: EIN (Kanal 1, GPIO 15)
[D] pH-Pumpe: Timer gestartet: 105s verbleibend

[D] Chlor-PID: sp=650, in=620, err=30, P=30.0, I=10.0, D=5.0, out=45.0%
[D] Chlor-PID: out=45.0% → 300s Zyklus → ON 135.0s (minOn=60s ✅, minOff=180s ✅)
[D] Chlor-Pumpe: EIN (Kanal 2, GPIO 14)
```

### Grafische Auswertung (Home Assistant)

In Home Assistant können die PID-Verläufe wunderbar visualisiert werden:

```yaml
# Beispiel: pH-Verlauf als Liniendiagramm
type: history-graph
title: pH-Regelung
entities:
  - entity: sensor.pool_ph
    name: pH-Istwert
  - entity: number.pool_ph_setpoint
    name: pH-Sollwert

---

# Beispiel: ORP-Verlauf
type: history-graph
title: Chlor-Regelung (ORP)
entities:
  - entity: sensor.pool_orp
    name: ORP-Istwert
  - entity: number.pool_orp_setpoint
    name: ORP-Sollwert
```

---

## 🔍 Fehlersuche

### Häufige Probleme

| Problem | Ursache | Lösung |
|---------|---------|--------|
| pH steigt trotz Dosierung | Filterpumpe läuft nicht | Interlock prüfen |
| pH sinkt nicht | Säure leer | Behälter füllen |
| ORP fällt trotz Dosierung | Cyanursäure zu hoch | Teilwasserwechsel |
| ORP schwankt stark | pH-Regelung instabil | Zuerst pH tunen! |
| Pumpe taktet zu schnell | minOffTime zu niedrig | Erhöhen |
| Pumpe läuft dauerhaft | maxOnTime zu hoch / I-Windup | maxOnTime prüfen |
| Keine Reaktion auf PID-Änderung | PID-Werte zu niedrig | Kp/Ki schrittweise erhöhen |

### Typische PID-Fehlersymptome

**Oszillation (Schwingung)**: Der Istwert pendelt um den Sollwert.
- → Kp zu hoch → reduzieren
- → Ki zu hoch → reduzieren
- → Deadband zu niedrig → erhöhen

**Bleibende Regelabweichung**: Der Istwert erreicht den Sollwert nicht.
- → Ki zu niedrig → erhöhen
- → I-Anteil durch minOffTime blockiert → prüfen

**Träge Reaktion**: Der Istwert ändert sich kaum.
- → Kp zu niedrig → erhöhen
- → Ki zu niedrig → erhöhen
- → Sensor-UpdateIntervall zu lang → verkürzen

**Dauerlauf**: Die Pumpe läuft ununterbrochen.
- → Sensor offline? → prüfen
- → maxOnTime zu niedrig? → erhöhen
- → I-Windup (nach langer Pause) → Anti-Windup prüfen

### Checkliste bei Inbetriebnahme

- [ ] pH-Sensor kalibriert? (pH 4.0 + 7.0)
- [ ] ORP-Sensor kalibriert? (Pufferlösung)
- [ ] Filterpumpe läuft? (ohne Interlock keine Dosierung!)
- [ ] config.json auf ESP32 hochgeladen?
- [ ] MQTT-Verbindung steht?
- [ ] PID-Setpoints sinnvoll? (pH 7.2, ORP 650)
- [ ] minOnTime/minOffTime plausibel?
- [ ] maxOnTime (Sicherheit) gesetzt?
- [ ] Home Assistant: Werden Werte empfangen?

> **Empfehlung:** Nach der ersten Inbetriebnahme den Controller
> 24–48 Stunden im Beobachtungsmodus laufen lassen, bevor PID-Parameter
> optimiert werden. Notiere die anfänglichen Werte als Baseline.