# ESPHome Valve Sequencer

This is a custom component for [ESPHome](https://esphome.io/) that provides intelligent control for multiple heating circuits (e.g., underfloor heating). Its main feature is to limit the number of simultaneously opening valves to prevent high inrush currents that could overload the power supply or trip circuit breakers.

## Features

- **Multi-Circuit Control:** Easily configure any number of heating circuits in a single YAML file.
- **Begrenzung des Einschaltstroms:** Eine konfigurierbare `max_concurrent` Einstellung verhindert, dass zu viele Ventile gleichzeitig öffnen.
- **Konfigurierbare Öffnungszeit:** Die `open_time` für die Stellantriebe kann global festgelegt werden.
- **Detaillierte Status-Sensoren:** Für jeden Heizkreis werden automatisch drei Entitäten in Home Assistant erstellt:
    - `switch`: Zum Ein- und Ausschalten des Heizkreises.
    - `binary_sensor (moving)`: Zeigt an, ob das Ventil gerade aktiv öffnet.
    - `binary_sensor (status)`: Zeigt an, ob das Ventil vollständig geöffnet ist.
- **Globaler Status-Sensor:** Ein optionaler Sensor, der anzeigt, ob mindestens ein Heizkreis im System aktiv (vollständig geöffnet) ist.
- **Autonomer Betrieb:** Die gesamte Steuerungslogik läuft autark auf dem ESP-Gerät. Bei einem Verbindungsverlust zu Home Assistant werden laufende Prozesse (wie das Öffnen eines Ventils) korrekt zu Ende geführt. Neue Befehle werden erst bei Wiederherstellung der Verbindung angenommen.

## Projektstruktur

Das Projekt besteht aus einer `external_component` für ESPHome, die die gesamte Logik kapselt.

```plaintext
esphome-valve-sequencer/
├── components/
│   └── valve_sequencer/
│       ├── __init__.py
│       ├── valve_sequencer.cpp
│       └── valve_sequencer.h
├── example.yaml
├── LICENSE
└── README.md
```


## Konfiguration

Configuration is done entirely in your device's YAML file.

### Main Configuration

To use this component, add the following to your device's YAML file.
```yaml
external_components:
  # When using from GitHub
  - source: github://Deafoult/esphome-valve-sequencer@main
  # For local development, you can use:
  # - source: components

valve_sequencer:
  # Maximum number of valves allowed to open SIMULTANEOUSLY.
  max_concurrent: 5

  # Time it takes for a thermal actuator to open completely.
  open_time: 5min

  # Optional: A global sensor that is 'on' when at least one valve is open.
  global_status_sensor:
    name: "Heating System Status"
    id: heating_system_status

  # List of all connected heating circuits.
  circuits:
    # ... see below
```

### Konfiguration der Heizkreise

First, define your output pins (e.g., for your relays). Then, reference these outputs in the `circuits` list.

```yaml
# 1. Define the outputs for your relays
output:
  - platform: gpio
    id: relay_kitchen
    pin: 4
  - platform: gpio
    id: relay_bathroom
    pin: 17
    # Note: 'inverted' is not set here. The inversion is handled below.

# 2. Configure the circuits in the valve_sequencer
valve_sequencer:
  # ... (max_concurrent, open_time etc.)
  circuits:
    - name: "Kitchen Heating Circuit"
      id: kitchen_heating_circuit
      output: relay_kitchen

    - name: "Bathroom Heating Circuit"
      id: bathroom_heating_circuit
      output: relay_bathroom
      inverted: true # This valve is controlled with inverted logic, even if the output itself is not defined as inverted.

    # ... weitere Kreisläufe
```

## Funktionsweise

1.  Ein Heizkreis wird über den `switch` in Home Assistant eingeschaltet.
2.  Die Komponente prüft, ob die Anzahl der bereits öffnenden Ventile (`is_changing`) kleiner ist als `max_concurrent`.
3.  Wenn ein "Slot" frei ist, wird das Relais des Ventils eingeschaltet und der `moving_sensor` wird `true`.
4.  Nach Ablauf der `open_time` wird der `moving_sensor` `false` und der `status_sensor` `true`. Das Relais bleibt weiterhin eingeschaltet, um das Ventil offen zu halten (gemäß aktueller Logik).
5.  Wird der Heizkreis in Home Assistant ausgeschaltet, schaltet die Komponente das Relais aus und setzt alle Sensoren auf `false`.
6.  Wenn kein "Slot" zum Öffnen frei ist, wartet der Heizkreis und versucht es in der nächsten Iteration erneut.

---

## Lizenz

Dieses Projekt ist unter der [MIT-Lizenz](LICENSE) veröffentlicht.

---
*This project was developed with the assistance of Gemini Code Assist.*
