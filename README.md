# Pool Controller — ESP32 (Kincony KC868-A8)

## Overview
Intelligent pool water chemistry and filtration controller based on ESP32.

### Features
- **3 Dosing Pumps**: Filter, pH, Chlorine (Relays 0-2)
- **PID Control**: pH and ORP-based chlorine dosing
- **Sensor Support**: pH probe, ORP probe, DS18B20 water temp, DS18B20 air temp, pressure sensor
- **Sensor Simulation**: Automatic fallback with pump-aware drift when real sensors are missing
- **Sensor Calibration**: Web-based calibration wizard with live voltage monitoring and auto-stability detection for pH (2-point) and ORP (single-point)
- **MQTT/HA**: Home Assistant auto-discovery via MQTT (27 entities)
- **Config via LittleFS**: JSON config stored in flash (ArduinoJson 6.x)
- **WiFi AP Fallback**: Configurable fallback access point
- **Web Dashboard**: Status page on port 80
- **Filter Pre-Run Delay**: Chemistry pumps wait for filter circulation before dosing

## Hardware: KC868-A8

| Relay | Function |
|-------|----------|
| 1 | Filter Pumpe |
| 2 | pH Pumpe |
| 3 | Chlor Pumpe |
| 4-8 | Spare |

| Sensor | Interface | Pin(s) |
|--------|-----------|--------|
| pH Probe | ADS1115 ch0 (I2C@0x48) | SDA=4, SCL=5 |
| ORP Probe | ADS1115 ch1 (I2C@0x48) | SDA=4, SCL=5 |
| Pressure | ADS1115 ch2 (I2C@0x48) | SDA=4, SCL=5 |
| DS18B20 Water | OneWire | GPIO 14 |
| DS18B20 Air | OneWire | GPIO 13 |

## MQTT Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `pool/status/LWT` | Out | Online/offline (Last Will) |
| `pool/sensors` | Out | All sensor values (JSON) |
| `pool/chemistry` | Out | PID state, setpoints, enabled |
| `pool/filter` | Out | Filter pump state, required runtime |
| `pool/pumps` | Out | Dosing pump runtime stats |
| `pool/pump_config` | Out | Pump timing config incl. pre-run delay (JSON) |
| `pool/calibration` | Out | Calibration data: slope, offset, timestamps (JSON) |
| `pool/ph` | Out | Raw pH value (dedicated topic) |
| `pool/command/ph_setpoint` | In | Set pH target (6.0-8.0) |
| `pool/command/orp_setpoint` | In | Set ORP target (200-900mV) |
| `pool/command/ph_set_enabled` | In | Enable/disable pH control |
| `pool/command/cl_set_enabled` | In | Enable/disable chlorine control |
| `pool/command/cal_ph_start` | In | Start pH 2-point calibration |
| `pool/command/cal_ph_lock7` | In | Lock pH 7.00 reading |
| `pool/command/cal_ph_lock4` | In | Lock pH 4.01 reading + save |
| `pool/command/cal_orp_start` | In | Start ORP calibration |
| `pool/command/cal_orp_lock` | In | Lock ORP reading + save (payload: mV) |
| `pool/command/cal_reset` | In | Reset to factory calibration |
| `pool/command/filter_prerun_delay` | In | Filter pre-run delay (1-60 min) |
| `pool/command/pump_timing` | In | Bulk pump timing config (JSON) |
| `pool/command/*_pump_min_on/off` | In | Individual pump timing settings |
| `pool/config/set` | In | Update config JSON at runtime |

## Calibration

### pH (2-point)
1. Place probe in pH 7.00 buffer → wait for STABLE → Lock
2. Rinse, place in pH 4.01 buffer → wait for STABLE → Lock & Save
3. Slope and intercept saved to `calibration.json` (survives config reset)

### ORP (single-point)
1. Place probe in ORP calibration solution (e.g. 220 mV)
2. Enter known value → wait for STABLE → Lock & Save
3. Offset saved to `calibration.json`

Calibration data persists in separate `calibration.json` on LittleFS.
Temperature compensation (Nernst equation) applied to pH readings.

## Building & Flashing

```bash
pio lib install && pio run && pio run --target upload && pio run --target uploadfs
pio device monitor -b 115200
```
