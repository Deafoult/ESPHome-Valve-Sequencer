import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, binary_sensor, output
from esphome.components.template import switch as template_switch
from esphome.const import (
    CONF_ID,
    CONF_OUTPUT,
    CONF_NAME,
    CONF_INVERTED,
    CONF_OPTIMISTIC,
    DEVICE_CLASS_OPENING,
    DEVICE_CLASS_MOVING,
)


# Namespace for our C++ code
valve_sequencer_ns = cg.esphome_ns.namespace("valve_sequencer")

CONF_GLOBAL_STATUS_SENSOR = "global_status_sensor"

# Our main component class
ValveSequencer = valve_sequencer_ns.class_("ValveSequencer", cg.Component)

# Schema for a single heating circuit
CIRCUIT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
        cv.Required(CONF_NAME): cv.string,
        cv.Optional(CONF_ID): cv.string,
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)

# The main configuration schema for our component in YAML
CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ValveSequencer),
            cv.Required("circuits"): cv.All(cv.ensure_list(CIRCUIT_SCHEMA)),
            cv.Optional("max_concurrent", default=5): cv.int_,
            cv.Optional("open_time", default="5min"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_GLOBAL_STATUS_SENSOR): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_OPENING
            ),
        }
    ),
    cv.requires_component("output"),
    cv.requires_component("switch"),
    cv.requires_component("binary_sensor"),
)


async def to_code(config):
    # Create the main instance of our C++ class
    hub = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(hub, config)

    # Set the configuration values
    cg.add(hub.set_max_concurrent(config["max_concurrent"]))
    cg.add(hub.set_open_time(config["open_time"]))

    if CONF_GLOBAL_STATUS_SENSOR in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_GLOBAL_STATUS_SENSOR])
        cg.add(hub.set_global_status_sensor(sens))

    # Go through all configured "circuits" and create the entities
    for i, circuit_config in enumerate(config["circuits"]):
        # Get the already created Output instance
        out = await cg.get_variable(circuit_config[CONF_OUTPUT])

        # Create the Template Switch by building a config for it and calling the helper.
        switch_config = {
            CONF_NAME: circuit_config[CONF_NAME],
            CONF_ID: circuit_config.get(CONF_ID) or f"valve_sequencer_switch_{i}",
            CONF_OPTIMISTIC: True,
        }
        switch_config = template_switch.CONFIG_SCHEMA(switch_config)
        sw = await switch.new_switch(switch_config)

        # Create the two Binary Sensors
        status_sensor_schema = binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_OPENING
        )
        status_sensor_conf = {CONF_NAME: f"{circuit_config[CONF_NAME]} Status"}
        if CONF_ID in circuit_config:
            status_sensor_conf[CONF_ID] = f"{circuit_config[CONF_ID]}_status"
        status_sensor_conf = status_sensor_schema(status_sensor_conf)
        status_sensor = await binary_sensor.new_binary_sensor(status_sensor_conf)

        moving_sensor_schema = binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_MOVING
        )
        moving_sensor_conf = {CONF_NAME: f"{circuit_config[CONF_NAME]} Moving"}
        if CONF_ID in circuit_config:
            moving_sensor_conf[CONF_ID] = f"{circuit_config[CONF_ID]}_moving"
        moving_sensor_conf = moving_sensor_schema(moving_sensor_conf)
        moving_sensor = await binary_sensor.new_binary_sensor(moving_sensor_conf)

        # Call the C++ method to add this circuit to the main component
        is_inverted = circuit_config[CONF_INVERTED]
        cg.add(
            hub.add_circuit(sw, out, status_sensor, moving_sensor, is_inverted)
        )
