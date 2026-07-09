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
- **e(t)** = Regelabweichung (Sollwert − Istwert, mit Richtungsumkehr via `reverseAction`)
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

Reagiert auf die **Änderungsgeschwindigkeit** des Messwerts (nicht des Fehlers!).
Wirkt wie ein "Dämpfer". **Derivative-on-Measurement** verhindert
"derivative kick" bei Sollwert-Änderungen.

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
| **Regelstrategie** | Bang-Bang mit PID-Schwelle | Kontinuierlich |

> **Wichtig:** Der Pool-Controller verwendet einen **Bang-Bang-Ansatz** mit
> PID-Stellgröße: Die Pumpe wird EINgeschaltet, wenn die PID-Stellgröße > 5%
> beträgt und alle Sicherheitsbedingungen erfüllt sind. Die PID-Stellgröße
> selbst ist kontinuierlich (0-100%), aber der Ausgang ist binär.

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

### Reverse Action

Die pH-Regelung verwendet **`reverseAction: true`**:

- **Reverse Acting**: Wenn der Istwert ÜBER dem Sollwert liegt → POSITIVE Stellgröße
- **Grund**: pH-Minus (Säure) SENKT den pH-Wert → mehr Dosierung = niedrigerer pH

```
error = input − setpoint   (reverseAction = true)
→ pH 7.4 > Sollwert 7.2 → error = +0.2 → positive Stellgröße → Pumpe EIN
→ pH 7.0 < Sollwert 7.2 → error = −0.2 → Stellgröße = 0     → Pumpe AUS
```

### Standard-Parameter

| Parameter | Wert | Bedeutung |
|-----------|------|-----------|
| `kp` | 1.2 | Proportionalverstärkung |
| `ki` | 0.08 | Integralverstärkung |
| `kd` | 0.04 | Differentialverstärkung |
| `setpoint` | 7.2 | pH-Sollwert |
| `outputMin` | 0.0 % | Minimale Stellgröße |
| `outputMax` | 100.0 % | Maximale Stellgröße |
| `minOnTimeSec` | 15 s | Kürzester Einschaltzyklus |
| `minOffTimeSec` | 60 s | Kürzester Ausschaltzyklus |
| `reverseAction` | true | Reverse acting (pH-Minus) |

### Bang-Bang-Logik

```
PID-Ausgang > 5% UND minOffTime abgelaufen UND Tageslimit nicht erreicht
    → Pumpe EIN

PID-Ausgang ≤ 5% UND minOnTime abgelaufen
    → Pumpe AUS
```

---

## 🧪 Chlor-Regelung (ORP-basiert)

### Zielsetzung

Die Desinfektionsleistung des Poolwassers soll auf einem sicheren Niveau
gehalten werden. Da freies Chlor direkt nicht einfach messbar ist, wird
der **ORP-Wert (Oxidation-Reduction Potential)** als Indikator verwendet.

**Typischer ORP-Sollwert: 650–750 mV**

### Direct Action

Die Chlor-Regelung verwendet **`reverseAction: false`** (direct acting):

- **Direct Acting**: Wenn der Istwert UNTER dem Sollwert liegt → POSITIVE Stellgröße
- **Grund**: Chlor-Dosierung ERHÖHT den ORP-Wert → mehr Dosierung = höherer ORP

```
error = setpoint − input   (reverseAction = false)
→ ORP 600 < Sollwert 650 → error = +50 → positive Stellgröße → Pumpe EIN
→ ORP 700 > Sollwert 650 → error = −50 → Stellgröße = 0      → Pumpe AUS
```

### Standard-Parameter

| Parameter | Wert | Bedeutung |
|-----------|------|-----------|
| `kp` | 0.8 | Proportionalverstärkung |
| `ki` | 0.05 | Integralverstärkung |
| `kd` | 0.02 | Differentialverstärkung |
| `setpoint` | 650 mV | ORP-Sollwert |
| `outputMin` | 0.0 % | Minimale Stellgröße |
| `outputMax` | 100.0 % | Maximale Stellgröße |
| `minOnTimeSec` | 30 s | Kürzester Einschaltzyklus |
| `minOffTimeSec` | 120 s | Kürzester Ausschaltzyklus |
| `reverseAction` | false | Direct acting (Chlor) |

### Zusammenhang ORP ↔ Freies Chlor

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

1. Starte mit **Kp = 0.5**
2. Beobachte das Verhalten über 2–4 h
3. Erhöhe Kp schrittweise um 0.2
4. Sobald der pH-Wert oszilliert, Kp um 30 % zurücknehmen
5. Optimal: pH erreicht Sollwert, bleibt aber etwas darunter (Regelabweichung)

#### Schritt 2: I-Anteil hinzufügen

```
Kp = optimal, Ki = 0.02, Kd = 0
```

1. Starte mit **Ki = 0.02**
2. Beobachte über 4–8 h
3. Der I-Anteil beseitigt die Regelabweichung
4. Erhöhe Ki schrittweise (0.02 → 0.05 → 0.08)
5. Bei Überschwingen: Ki reduzieren

#### Schritt 3: D-Anteil zur Stabilisierung

```
Kp = optimal, Ki = optimal, Kd = 0.02
```

1. Starte mit **Kd = 0.02**
2. Der D-Anteil dämpft Überschwingen (Derivative-on-Measurement)
3. Bei Rauschanfälligkeit: Kd reduzieren

#### Empfohlene Werte pH

| Parameter | Standard | Aggressiv | Konservativ |
|-----------|----------|-----------|-------------|
| Kp | 1.2 | 2.0 | 0.8 |
| Ki | 0.08 | 0.15 | 0.04 |
| Kd | 0.04 | 0.08 | 0.02 |
| setpoint | 7.2 | 7.0 | 7.4 |
| minOnTime | 15 s | 10 s | 30 s |
| minOffTime | 60 s | 30 s | 120 s |

#### Empfohlene Werte ORP

| Parameter | Standard | Aggressiv | Konservativ |
|-----------|----------|-----------|-------------|
| Kp | 0.8 | 1.2 | 0.5 |
| Ki | 0.05 | 0.08 | 0.02 |
| Kd | 0.02 | 0.03 | 0.01 |
| setpoint | 650 mV | 700 mV | 600 mV |
| minOnTime | 30 s | 20 s | 60 s |
| minOffTime | 120 s | 90 s | 180 s |

### Tuning-Richtlinie nach Poolgröße

| Poolgröße | pH Kp | pH Ki | ORP Kp | ORP Ki | Empfehlung |
|-----------|-------|-------|--------|--------|------------|
| <20 m³ | 1.0 | 0.05 | 0.6 | 0.03 | Vorsichtig dosieren |
| 20–50 m³ | 1.2 | 0.08 | 0.8 | 0.05 | Normal (Standard) |
| 50–80 m³ | 1.5 | 0.10 | 1.0 | 0.07 | Etwas mehr |
| >80 m³ | 1.8 | 0.15 | 1.2 | 0.10 | Höhere Durchsatzrate |

### Praxis-Tipps

1. **Störgrößen erkennen**: Nach einer Badebelastung oder Regen steigt der pH
   (und ORP sinkt). Das ist normal – PID reagiert automatisch.
2. **Zeit geben**: ORP-Änderungen brauchen bis zu 2 Stunden. Nicht zu früh
   nachjustieren!
3. **Wetter beachten**: Bei Hitze steigt der Chlorverbrauch → ORP sinkt.
4. **Cyanursäure messen**: Hohe Werte (>50 ppm) machen ORP-Regelung schwierig.
5. **Dokumentation**: Werte in Home Assistant tracken → Muster erkennen

---

## 🔒 Sicherheitsmechanismen

### Kritische Sicherheitsfunktionen

| Mechanismus | Beschreibung | Auswirkung |
|-------------|-------------|-----------|
| Filterpumpen-Interlock | Chemie-Dosierung NUR bei laufender Filterpumpe (via PumpController dependents) | Verhindert Chemie-Stau |
| Max. Tageslaufzeit | `phPump.maxDailyRuntimeMin` / `chlorinePump.maxDailyRuntimeMin` | Hardware-Schutz pro Tag |
| Min. Ein-/Ausschaltzeit | `minOnTimeSec` / `minOffTimeSec` in PID-Params | Schützt Dosierpumpen |
| PID-Anti-Windup | I-Anteil auf `outputMin`/`outputMax` begrenzt (Clamping) | Verhindert I-Windup |
| Bang-Bang-Schwelle | Pumpe nur EIN wenn PID-Output > 5% | Verhindert Dauer-Takten |
| dt-Begrenzung | Maximal 5s zwischen PID-Berechnungen | Verhindert derivative kick |
| Sensor-Fallback | Simulation übernimmt bei Sensorausfall | Kein Stillstand |

### Filterpumpen-Interlock (Details)

Die Chemie-Dosierung (pH und Chlor) ist **NUR aktiv**, wenn die Filterpumpe
läuft. Dies wird über das `PumpController`-Dependent-System realisiert:

- `phPump` und `chlorinePump` sind als Dependents der Filterpumpe registriert
- Wenn die Filterpumpe stoppt, werden alle Dependents per `forceOff()` gestoppt
- Die PID-Regler laufen weiter (I-Anteil wird NICHT verändert)

### Maximale Tageslaufzeit

Jede Dosierpumpe hat eine maximale Tageslaufzeit (`maxDailyRuntimeMin`).
Wird dieser Wert überschritten, wird die Pumpe per `forceOff()` gestoppt
und erst am nächsten Tag wieder freigegeben.

### Anti-Windup (Clamping)

```cpp
// Integral clamping an den Ausgangsgrenzen:
_integral += _ki * error * dtSec;
if (_integral > _outputMax) _integral = _outputMax;
if (_integral < _outputMin) _integral = _outputMin;
```

Der I-Anteil wird an `outputMin` und `outputMax` begrenzt — kein
bedingtes Clamping, sondern hartes Clamping bei jedem Schritt.

---

## 📊 Diagnose & Monitoring

### PID-Werte über MQTT

Der Controller sendet Diagnose-JSON auf `pool/chemistry`:

```json
{
  "ph": {
    "enabled": true,
    "setpoint": 7.2,
    "pid_output": 35.2,
    "pump_on": true,
    "p": 0.24,
    "i": 0.016,
    "d": 0.008
  },
  "chlorine": {
    "enabled": true,
    "setpoint": 650,
    "pid_output": 45.0,
    "pump_on": false,
    "p": 40.0,
    "i": 15.0,
    "d": 2.5
  }
}
```

| Feld | Beschreibung |
|------|-------------|
| `enabled` | Regelkreis aktiv? |
| `setpoint` | Aktueller Sollwert |
| `pid_output` | PID-Stellgröße (0–100 %) |
| `pump_on` | Dosierpumpe aktuell EIN? |
| `p` | P-Anteil (aktuell) |
| `i` | I-Anteil (aktuell) |
| `d` | D-Anteil (aktuell) |

### Pumpen-Statistiken

Pumpen-Laufzeiten werden auf `pool/pumps` publiziert:

```json
{
  "ph_pump": {
    "name": "pH Pump",
    "on": false,
    "runtime_today_min": 12.5,
    "runtime_minutes": 345,
    "last_on_duration": 45
  },
  "chlorine_pump": { ... },
  "filter_pump": { ... }
}
```

### Debug-Logging

Bei `logLevel: 1` werden PID-Statusmeldungen ausgegeben:

```
[I] Pool chemistry controller initialized
[I]   pH PID: Kp=1.20 Ki=0.0800 Kd=0.0400 setpoint=7.2 reverse=true
[I]   Cl PID: Kp=0.80 Ki=0.0500 Kd=0.0200 setpoint=650 mV reverse=false
[I] pH pump ON (output=35.2%, pH=7.35, setpoint=7.20)
[I] pH pump OFF (output=2.1%, pH=7.21)
[I] Chlorine pump ON (output=45.0%, ORP=620, setpoint=650)
```

### Grafische Auswertung (Home Assistant)

In Home Assistant können die PID-Verläufe visualisiert werden:

```yaml
# pH-Verlauf
type: history-graph
title: pH-Regelung
entities:
  - entity: sensor.pool_ph
  - entity: number.pool_ph_setpoint
  - entity: sensor.pool_ph_pid_output

# ORP-Verlauf
type: history-graph
title: Chlor-Regelung (ORP)
entities:
  - entity: sensor.pool_orp
  - entity: number.pool_orp_setpoint
  - entity: sensor.pool_chlorine_pid_output
```

---

## 🔍 Fehlersuche

### Häufige Probleme

| Problem | Ursache | Lösung |
|---------|---------|--------|
| pH steigt trotz Dosierung | Filterpumpe läuft nicht | Interlock prüfen |
| pH sinkt nicht | Säure leer / Tageslimit erreicht | Behälter füllen, `maxDailyRuntimeMin` prüfen |
| ORP fällt trotz Dosierung | Cyanursäure zu hoch | Teilwasserwechsel |
| ORP schwankt stark | pH-Regelung instabil | Zuerst pH tunen! |
| Pumpe taktet zu schnell | minOffTime zu niedrig | Erhöhen |
| Pumpe läuft nicht | PID-Output < 5% / minOffTime nicht abgelaufen | Parameter prüfen |
| Keine Reaktion auf PID-Änderung | PID-Werte zu niedrig | Kp/Ki schrittweise erhöhen |

### Typische PID-Fehlersymptome

**Oszillation (Schwingung)**: Der Istwert pendelt um den Sollwert.
- → Kp zu hoch → reduzieren
- → Ki zu hoch → reduzieren

**Bleibende Regelabweichung**: Der Istwert erreicht den Sollwert nicht.
- → Ki zu niedrig → erhöhen
- → Bang-Bang-Schwelle (5%) zu hoch → Toleranz prüfen

**Träge Reaktion**: Der Istwert ändert sich kaum.
- → Kp zu niedrig → erhöhen
- → Ki zu niedrig → erhöhen

**Dauerlauf**: Die Pumpe läuft ununterbrochen.
- → Sensor offline? → Simulations-Fallback prüfen
- → Tageslimit nicht gesetzt? → `maxDailyRuntimeMin` konfigurieren
- → I-Windup → Anti-Windup-Clamping prüfen

### Checkliste bei Inbetriebnahme

- [ ] pH-Sensor kalibriert? (pH 4.0 + 7.0 via `setCalibration()`)
- [ ] ORP-Sensor geprüft?
- [ ] Filterpumpe läuft? (ohne Interlock keine Dosierung!)
- [ ] `config.json` auf ESP32 hochgeladen (LittleFS)?
- [ ] MQTT-Verbindung steht?
- [ ] PID-Setpoints sinnvoll? (pH 7.2, ORP 650)
- [ ] `reverseAction` korrekt? (pH: true, Chlor: false)
- [ ] `minOnTimeSec`/`minOffTimeSec` plausibel?
- [ ] `maxDailyRuntimeMin` (Sicherheit) gesetzt?
- [ ] Home Assistant: Werden Werte empfangen?

> **Empfehlung:** Nach der ersten Inbetriebnahme den Controller
> 24–48 Stunden im Beobachtungsmodus laufen lassen, bevor PID-Parameter
> optimiert werden. Notiere die anfänglichen Werte als Baseline.