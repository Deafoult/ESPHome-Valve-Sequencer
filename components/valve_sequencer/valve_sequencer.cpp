#include "valve_sequencer.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"

namespace esphome {
namespace valve_sequencer {

static const char *const TAG = "valve_sequencer";

void ValveSequencer::setup() {
  ESP_LOGI(TAG, "ValveSequencer component setup complete.");
  // Set initial state for all valves (e.g., close all)
  for (auto &circuit : this->circuits_) {
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
                                 binary_sensor::BinarySensor *status, binary_sensor::BinarySensor *moving) {
  this->circuits_.push_back({
      .control_switch = sw,
      .valve_output = out,
      .status_sensor = status,
      .moving_sensor = moving,
  });
}

void ValveSequencer::loop() {
  // Prüfen, ob eine Verbindung zu Home Assistant besteht.
  bool is_connected = api::global_api_server != nullptr && api::global_api_server->is_connected();
  uint32_t now = millis();

  // Hauptschleife: Gehe jeden Heizkreis durch und verarbeite seine Logik.
  for (auto &c : this->circuits_) {
    // Zustand 1: Timer ist abgelaufen, Öffnungsvorgang beenden
    if (c.is_changing && (now - c.timer_start_time > this->open_time_ms_)) {
      ESP_LOGI(TAG, "Circuit '%s': Open process finished.", c.control_switch->get_name().c_str());
      c.is_changing = false;
      c.is_open = true;
      c.moving_sensor->publish_state(false);
      c.status_sensor->publish_state(true);
      // Das Relais bleibt absichtlich eingeschaltet.
    }
    // Nur auf neue Befehle vom Switch reagieren, wenn eine API-Verbindung besteht.
    if (is_connected) {
      bool desired_state = c.control_switch->state;

      // Zustand 2: Nutzer will schließen (desired=OFF), während das Ventil offen ist oder gerade öffnet.
      if (!desired_state && (c.is_open || c.is_changing)) {
        ESP_LOGI(TAG, "Circuit '%s': Closing valve (command from HA).", c.control_switch->get_name().c_str());
        c.is_open = false;
        c.is_changing = false;  // Stoppt auch einen eventuellen Öffnungsvorgang
        c.valve_output->turn_off();
        c.status_sensor->publish_state(false);
        c.moving_sensor->publish_state(false);
        // Der Switch-Zustand wurde bereits von HA auf 'false' gesetzt, hier ist keine Aktion nötig.
      }
      // Zustand 3: Nutzer will öffnen (desired=ON, is_open=OFF, is_changing=OFF)
      else if (desired_state && !c.is_open && !c.is_changing) {
        // Zähle erneut, wie viele gerade öffnen, um die aktuellste Zahl zu haben.
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
          c.valve_output->turn_on();
          c.moving_sensor->publish_state(true);
        }
      }
    }
  }

  // Nach der Verarbeitung aller Zustände, aktualisiere den globalen Sensor.
  bool any_circuit_open = false;
  for (auto &c : this->circuits_) {
    // Prüfen, ob irgendein Kreislauf aktiv ist (offen oder öffnend) für den globalen Sensor.
    if (c.is_open) { // Nur prüfen, ob der Kreislauf vollständig offen ist
      any_circuit_open = true;
      break; // Ein offener Kreislauf reicht aus.
    }
  }

  // Nach der Verarbeitung aller Kreise den Gesamtstatus-Sensor aktualisieren
  if (this->global_status_sensor_ != nullptr) {
    // Der globale Sensor ist an, wenn mindestens ein Ventil vollständig offen ist.
    this->global_status_sensor_->publish_state(any_circuit_open);
  }
}

}  // namespace valve_sequencer
}  // namespace esphome