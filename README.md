# Pool Controller — ESP32 (Kincony KC868-A8)

## Overview
Intelligent pool water chemistry and filtration controller based on ESP32.

### Features
- **3 Dosing Pumps**: Filter, pH, Chlorine (Relays 0-2)
- **PID Control**: pH and ORP-based chlorine dosing
- **Sensor Support**: pH probe, ORP probe, DS18B20 water temp, DHT22 air temp, pressure sensor
- **Sensor Simulation**: Automatic fallback when real sensors are missing
- **MQTT/HA**: Home Assistant auto-discovery via MQTT
- **Config via SPIFFS**: JSON config stored in flash
- **WiFi AP Fallback**: Configurable fallback access point
- **Web Dashboard**: Status page on port 80

## Hardware: KC868-A8 Pinout

| Relay | GPIO | Function |
|-------|------|----------|
| 1     | 13   | Filter Pumpe |
| 2     | 14   | pH Pumpe |
| 3     | 15   | Chlor Pumpe |
| 4     | 16   | Spare |
| 5     | 17   | Spare |
| 6     | 18   | Spare |
| 7     | 19   | Spare |
| 8     | 21   | Spare |

| Sensor | Interface | Pin(s) |
|--------|-----------|--------|
| pH Probe | ADS1115 ch0 (I2C@0x48) | SDA=4, SCL=5 |
| ORP Probe | ADS1115 ch1 (I2C@0x48) | SDA=4, SCL=5 |
| Pressure | ADS1115 ch2 (I2C@0x48) | SDA=4, SCL=5 |
| DS18B20 | OneWire | GPIO 32 |
| DHT22 | Digital | GPIO 33 |

## Project Structure

```
pool-controller/
├── platformio.ini           # Build configuration
├── data/
│   └── config.json          # Default SPIFFS configuration
└── src/
    ├── main.cpp             # Application entry point
    ├── config/
    │   ├── ConfigManager.h  # Configuration loading/saving
    │   └── ConfigManager.cpp
    ├── sensors/
    │   ├── SensorBase.h     # Abstract sensor interface
    │   ├── PHSensor.h/.cpp  # pH probe via ADS1115
    │   ├── ORPSensor.h/.cpp # ORP probe via ADS1115
    │   ├── DallasTemperatureSensor.h/.cpp  # DS18B20
    │   ├── DHTSensor.h/.cpp # DHT22 air temp/humidity
    │   ├── PressureSensor.h/.cpp  # Filter pressure
    │   └── SensorManager.h/.cpp    # Sensor coordination
    ├── simulation/
    │   ├── SensorSimulator.h/.cpp  # Realistic sensor simulation
    ├── pid/
    │   ├── PIDController.h/.cpp    # Standard PID with anti-windup
    │   └── PoolChemistryController.h/.cpp  # pH/Chlorine orchestration
    ├── actuators/
    │   ├── RelayManager.h/.cpp     # KC868-A8 relay control
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
| `pool/sensors` | Out | All sensor values (JSON) |
| `pool/chemistry` | Out | PID state, setpoints |
| `pool/filter` | Out | Filter pump state |
| `pool/pumps` | Out | Dosing pump stats |
| `pool/relays` | Out | All relay states |
| `pool/status/LWT` | Out | Online/offline (HA discovery) |
| `pool/command/ph_setpoint` | In | Set pH target (6.0-8.0) |
| `pool/command/orp_setpoint` | In | Set ORP target (200-900mV) |
| `pool/command/ph_set_enabled` | In | Enable/disable pH control |
| `pool/command/cl_set_enabled` | In | Enable/disable chlorine control |
| `pool/command/relay` | In | Direct relay control `CH,STATE` |
| `pool/command/all_off` | In | Emergency all relays off |
| `pool/command/reset_config` | In | Reset to defaults |
| `pool/command/restart` | In | ESP32 restart |

## Building & Flashing

```bash
# Install dependencies
pio lib install

# Build
pio run

# Upload to ESP32
pio run --target upload

# Upload SPIFFS data
pio run --target uploadfs

# Monitor
pio device monitor -b 115200
```

## First Start
1. Flash the firmware
2. Connect to `PoolController-AP` WiFi (password: `12345678`)
3. Open `http://192.168.4.1` for status
4. Configure WiFi and MQTT via config.json on SPIFFS
5. ESP32 will reboot and connect to your network
