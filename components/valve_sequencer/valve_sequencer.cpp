#include "valve_sequencer.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"

namespace esphome {
namespace valve_sequencer {

static const char *const TAG = "valve_sequencer";

void ValveSequencer::setup() {
  ESP_LOGI(TAG, "ValveSequencer component setup complete.");
  // Set initial state for all valves (e.g., close all)
  for (auto &circuit : this->circuits_) { // NOLINT(readability-identifier-naming)
    circuit.valve_output->turn_off();
    circuit.status_sensor->publish_state(false);
    circuit.moving_sensor->publish_state(false);
  }
}

void ValveSequencer::dump_config() {
  ESP_LOGCONFIG(TAG, "ValveSequencer:");
  ESP_LOGCONFIG(TAG, "  Number of circuits: %zu", this->circuits_.size());
  ESP_LOGCONFIG(TAG, "  Max concurrent valves: %d", this->max_concurrent_);
  ESP_LOGCONFIG(TAG, "  Valve open time: %u ms", this->open_time_ms_);
}

void ValveSequencer::add_circuit(switch_::TemplateSwitch *sw, output::BinaryOutput *out,
                                 binary_sensor::BinarySensor *status, binary_sensor::BinarySensor *moving,
                                 bool is_inverted) {
  this->circuits_.push_back({
      .control_switch = sw,
      .valve_output = out,
      .status_sensor = status,
      .moving_sensor = moving,
      .is_inverted = is_inverted,
  });
}

void ValveSequencer::loop() {
  // Check if there is a connection to Home Assistant.
  bool is_connected = api::global_api_server != nullptr && api::global_api_server->is_connected();
  uint32_t now = millis();

  // Main loop: Iterate through each heating circuit and process its logic.
  for (auto &c : this->circuits_) { // NOLINT(readability-identifier-naming)
    // State 1: Timer has expired, finish the opening process
    if (c.is_changing && (now - c.timer_start_time > this->open_time_ms_)) {
      ESP_LOGI(TAG, "Circuit '%s': Open process finished.", c.control_switch->get_name().c_str());
      c.is_changing = false;
      c.is_open = true;
      c.moving_sensor->publish_state(false);
      c.status_sensor->publish_state(true);
      // The relay intentionally remains on.
    }
    // Only react to new commands from the switch if an API connection exists.
    if (is_connected) {
      bool desired_state = c.control_switch->state;

      // State 2: User wants to close (desired=OFF) while the valve is open or opening.
      if (!desired_state && (c.is_open || c.is_changing)) {
        ESP_LOGI(TAG, "Circuit '%s': Closing valve (command from HA).", c.control_switch->get_name().c_str());
        c.is_open = false;
        c.is_changing = false;  // Also stops any potential opening process
        c.valve_output->set_state(c.is_inverted); // OFF state
        c.status_sensor->publish_state(false);
        c.moving_sensor->publish_state(false);
        // The switch state has already been set to 'false' by HA, no action needed here.
      }
      // State 3: User wants to open (desired=ON, is_open=OFF, is_changing=OFF)
      else if (desired_state && !c.is_open && !c.is_changing) {
        // Count again how many are currently opening to have the most up-to-date number.
        int changing_to_open = 0;
        for (const auto &circuit_check : this->circuits_) {
          if (circuit_check.is_changing) {
            changing_to_open++;
          }
        }

        if (changing_to_open < this->max_concurrent_) {
          ESP_LOGI(TAG, "Circuit '%s': Starting open process.", c.control_switch->get_name().c_str());
          c.is_changing = true;
          c.timer_start_time = now;
          c.valve_output->set_state(!c.is_inverted); // ON state
          c.moving_sensor->publish_state(true);
        }
      }
    }
  }

  // After processing all states, update the global sensor.
  bool any_circuit_open = false;
  for (auto &c : this->circuits_) { // NOLINT(readability-identifier-naming)
    // Check if any circuit is active (open) for the global sensor.
    if (c.is_open) { // Only check if the circuit is fully open
      any_circuit_open = true;
      break; // One open circuit is enough.
    }
  }

  // After processing all circuits, update the global status sensor
  if (this->global_status_sensor_ != nullptr) {
    // The global sensor is on if at least one valve is fully open.
    this->global_status_sensor_->publish_state(any_circuit_open);
  }
}

}  // namespace valve_sequencer
}  // namespace esphome