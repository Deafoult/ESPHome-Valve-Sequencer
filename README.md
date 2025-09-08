# ESPHome Valve Sequencer

This is a custom component for [ESPHome](https://esphome.io/) that provides intelligent control for multiple heating circuits (e.g., underfloor heating). Its main feature is to limit the number of simultaneously opening valves to prevent high inrush currents that could overload the power supply or trip circuit breakers.

## Features

- **Multi-Circuit Control:** Easily configure any number of heating circuits in a single YAML file.
- **Inrush Current Limiting:** A configurable `max_concurrent` setting prevents too many valves from opening simultaneously.
- **Configurable Opening Time:** The `open_time` for the thermal actuators can be set globally.
- **Detailed Status Sensors:** For each heating circuit, three entities are automatically created in Home Assistant:
    - `switch`: To turn the heating circuit on and off.
    - `binary_sensor (moving)`: Indicates if the valve is currently in the process of opening.
    - `binary_sensor (status)`: Indicates if the valve is fully open.
- **Global Status Sensor:** An optional sensor that indicates if at least one heating circuit in the system is active (fully open).
- **Autonomous Operation:** The entire control logic runs independently on the ESP device. If the connection to Home Assistant is lost, ongoing processes (like opening a valve) will complete correctly. New commands are only accepted once the connection is restored.

## Project Structure

The project consists of an `external_component` for ESPHome that encapsulates all the logic.

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

## Functionality

1. A heating circuit is turned on via the `switch` in Home Assistant.
2. The component checks if the number of valves currently opening (`is_changing`) is less than `max_concurrent`.
3. If a "slot" is available, the relay of the valve is activated and the `moving_sensor` is set to `true`.
4. After the `open_time` has elapsed, the `moving_sensor` is set to `false` and the `status_sensor` to `true`. The relay remains activated to keep the valve open (according to current logic).
5. When the heating circuit is turned off in Home Assistant, the component turns off the relay and sets all sensors to `false`.
6. If no "slot" is available for opening, the heating circuit waits and tries again in the next iteration.

---

## License

This project is licensed under the [MIT License](LICENSE).

---
*This project was developed with the assistance of Gemini Code Assist.*
