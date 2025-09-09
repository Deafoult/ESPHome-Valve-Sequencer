#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/template_switch.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/output/binary_output.h"

namespace esphome {
namespace valve_sequencer {

// A small helper struct to hold all parts of a circuit together
struct Circuit {
  switch_::TemplateSwitch *control_switch;
  output::BinaryOutput *valve_output;
  binary_sensor::BinarySensor *status_sensor;
  binary_sensor::BinarySensor *moving_sensor;

  bool is_inverted{false};
  bool is_open{false};
  bool is_changing{false};
  uint32_t timer_start_time{0};
};

class ValveSequencer : public Component {
 public:
  // Methods called from the YAML configuration
  void set_max_concurrent(int max) { this->max_concurrent_ = max; }
  void set_open_time(uint32_t ms) { this->open_time_ms_ = ms; }
  void set_global_status_sensor(binary_sensor::BinarySensor *sensor) { this->global_status_sensor_ = sensor; }

  // Method called from __init__.py to register the circuits
  void add_circuit(switch_::TemplateSwitch *sw, output::BinaryOutput *out,
                   binary_sensor::BinarySensor *status, binary_sensor::BinarySensor *moving, bool is_inverted);

  // Standard ESPHome methods
  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  int max_concurrent_;
  uint32_t open_time_ms_;
  std::vector<Circuit> circuits_;
  binary_sensor::BinarySensor *global_status_sensor_{nullptr};
};

}  // namespace valve_sequencer
}  // namespace esphome