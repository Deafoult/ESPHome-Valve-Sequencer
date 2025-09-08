import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, binary_sensor, output
from esphome.const import (
    CONF_ID,
    CONF_OUTPUT,
    CONF_NAME,
    CONF_INVERTED,
    CONF_API,
    DEVICE_CLASS_OPENING,
    DEVICE_CLASS_MOVING,
)

# Namespace für unseren C++ Code
valve_sequencer_ns = cg.esphome_ns.namespace("valve_sequencer")

CONF_GLOBAL_STATUS_SENSOR = "global_status_sensor"

# Unsere Haupt-Komponenten-Klasse
ValveSequencer = valve_sequencer_ns.class_("ValveSequencer", cg.Component)

# Schema für einen einzelnen Heizkreis
CIRCUIT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
        cv.Required(CONF_NAME): cv.string,
        cv.Optional(CONF_ID): cv.string,
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)

# Das Haupt-Konfigurationsschema für unsere Komponente in der YAML
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ValveSequencer),
        cv.Required("circuits"): cv.All(cv.ensure_list(CIRCUIT_SCHEMA)),
        cv.Optional("max_concurrent", default=5): cv.int_,
        cv.Optional("open_time", default="5min"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_GLOBAL_STATUS_SENSOR): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_OPENING
        ),
    }
    .extend(
        cv.require_component("output")
    ) # Stellt sicher, dass die Output-Komponente geladen wird
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    # Erstelle die Haupt-Instanz unserer C++ Klasse
    hub = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(hub, config)

    # Setze die Konfigurationswerte
    cg.add(hub.set_max_concurrent(config["max_concurrent"]))
    cg.add(hub.set_open_time(config["open_time"]))

    if CONF_GLOBAL_STATUS_SENSOR in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_GLOBAL_STATUS_SENSOR])
        cg.add(hub.set_global_status_sensor(sens))

    # Gehe durch alle konfigurierten "circuits" und erstelle die Entitäten
    for i, circuit_config in enumerate(config["circuits"]):
        # Hole die bereits erstellte Output-Instanz
        out = await cg.get_variable(circuit_config[CONF_OUTPUT])

        # Erstelle den Template Switch
        sw = cg.new_Pvariable(f"valve_sequencer_switch_{i}")
        await switch.register_switch(sw, circuit_config)

        # Erstelle die zwei Binary Sensors
        status_sensor_conf = binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_OPENING
        )({CONF_NAME: f"{circuit_config[CONF_NAME]} Status"})
        if CONF_ID in circuit_config:
            status_sensor_conf[CONF_ID] = f"{circuit_config[CONF_ID]}_status"

        moving_sensor_conf = binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_MOVING
        )({CONF_NAME: f"{circuit_config[CONF_NAME]} Moving"})
        if CONF_ID in circuit_config:
            moving_sensor_conf[CONF_ID] = f"{circuit_config[CONF_ID]}_moving"

        status_sensor = await binary_sensor.new_binary_sensor(status_sensor_conf)
        moving_sensor = await binary_sensor.new_binary_sensor(moving_sensor_conf)

        # Rufe die C++ Methode auf, um diesen Kreis zur Hauptkomponente hinzuzufügen
        cg.add(hub.add_circuit(sw, out, status_sensor, moving_sensor))