# Pool Controller — ESP32 (Kincony KC868-A8)

## Overview
Intelligent pool water chemistry and filtration controller based on ESP32.

### Features
- **3 Dosing Pumps**: Filter, pH, Chlorine (Relays 0-2)
- **PID Control**: pH and ORP-based chlorine dosing
- **Sensor Support**: pH probe, ORP probe, DS18B20 water temp, DS18B20 air temp, pressure sensor
- **Sensor Simulation**: Automatic fallback with pump-aware drift when real sensors are missing
- **MQTT/HA**: Home Assistant auto-discovery via MQTT (21 entities)
- **Config via LittleFS**: JSON config stored in flash (ArduinoJson 6.x)
- **WiFi AP Fallback**: Configurable fallback access point
- **Web Dashboard**: Status page on port 80

## Hardware: KC868-A8

The KC868-A8 uses a **PCF8574 I2C I/O expander** (address 0x24) to control its 8 relays.
Relay driver is **active-LOW**: writing 0 turns the relay ON, 1 turns it OFF.

| Relay | PCF8574 Bit | Function |
|-------|------------|----------|
| 1     | 0 | Filter Pumpe |
| 2     | 1 | pH Pumpe |
| 3     | 2 | Chlor Pumpe |
| 4     | 3 | Spare |
| 5     | 4 | Spare |
| 6     | 5 | Spare |
| 7     | 6 | Spare |
| 8     | 7 | Spare |

| Sensor | Interface | Pin(s) |
|--------|-----------|--------|
| pH Probe | ADS1115 ch0 (I2C@0x48) | SDA=4, SCL=5 |
| ORP Probe | ADS1115 ch1 (I2C@0x48) | SDA=4, SCL=5 |
| Pressure | ADS1115 ch2 (I2C@0x48) | SDA=4, SCL=5 |
| DS18B20 Water | OneWire | GPIO 14 |
| DS18B20 Air | OneWire | GPIO 13 |

## Project Structure

```
pool-controller/
├── platformio.ini           # Build configuration
├── data/
│   └── config.json          # Default LittleFS configuration
└── src/
    ├── main.cpp             # Application entry point + web server
    ├── config/
    │   ├── ConfigDefaults.h # Default configuration values
    │   ├── ConfigManager.h  # Configuration loading/saving
    │   └── ConfigManager.cpp
    ├── sensors/
    │   ├── SensorBase.h     # Abstract sensor interface
    │   ├── PHSensor.h/.cpp  # pH probe via ADS1115
    │   ├── ORPSensor.h/.cpp # ORP probe via ADS1115
    │   ├── DallasTemperatureSensor.h/.cpp  # DS18B20 (water + air)
    │   ├── PressureSensor.h/.cpp  # Filter pressure
    │   └── SensorManager.h/.cpp    # Sensor coordination
    ├── simulation/
    │   └── SensorSimulator.h/.cpp  # Realistic sensor simulation
    ├── pid/
    │   ├── PIDController.h/.cpp    # Standard PID with anti-windup
    │   └── PoolChemistryController.h/.cpp  # pH/Chlorine orchestration
    ├── actuators/
    │   ├── RelayManager.h/.cpp     # KC868-A8 PCF8574 I2C relay control
    │   └── PumpController.h/.cpp   # Safety-aware pump control
    ├── utils/
    │   ├── FilterPumpLogic.h/.cpp  # Temperature-based filter scheduling
    │   └── TimingUtils.h/.cpp      # NTP and time window helpers
    └── mqtt/
        └── MQTTManager.h/.cpp      # MQTT + HA auto-discovery
```

## MQTT Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `pool/status/LWT` | Out | Online/offline (Last Will) |
| `pool/sensors` | Out | All sensor values (JSON) |
| `pool/chemistry` | Out | PID state, setpoints, enabled |
| `pool/filter` | Out | Filter pump state, required runtime |
| `pool/pumps` | Out | Dosing pump runtime stats |
| `pool/pump_config` | Out | Pump anti-short-cycle timing config (JSON) |
| `pool/ph` | Out | Raw pH value (dedicated topic) |
| `pool/command/ph_setpoint` | In | Set pH target (6.0-8.0) |
| `pool/command/orp_setpoint` | In | Set ORP target (200-900mV) |
| `pool/command/ph_set_enabled` | In | Enable/disable pH control |
| `pool/command/cl_set_enabled` | In | Enable/disable chlorine control |
| `pool/command/all_off` | In | Emergency all relays off |
| `pool/command/reset_config` | In | Reset to defaults (send 'confirm') |
| `pool/command/restart` | In | ESP32 restart (send 'confirm') |
| `pool/command/pump_timing` | In | Bulk pump timing config (JSON) |
| `pool/command/*_pump_min_on/off` | In | Individual pump timing settings |
| `pool/config/set` | In | Update config JSON at runtime |

## Building & Flashing

```bash
# Install dependencies
pio lib install

# Build
pio run

# Upload to ESP32
pio run --target upload

# Upload LittleFS data
pio run --target uploadfs

# Monitor
pio device monitor -b 115200
```

## First Start
1. Flash the firmware
2. Upload LittleFS data (`pio run --target uploadfs`)
3. If WiFi unavailable: connect to `PoolController-AP` WiFi (password: `12345678`)
4. Open `http://192.168.4.1` for status
5. Configure WiFi and MQTT via `config.json` on LittleFS
6. ESP32 will reboot and connect to your network
