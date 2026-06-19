from __future__ import annotations

import esphome.codegen as cg
from esphome.components import output, uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["api", "uart"]
CODEOWNERS = []

CONF_AGGREGATION = "aggregation"
CONF_FALLBACK_ZONE_COUNT = "fallback_zone_count"
CONF_GROUP = "group"
CONF_MODULE_ADDRESS = "module_address"
CONF_RAW_LOGGING = "raw_logging"
CONF_TEMPERATURE_ENTITIES = "temperature_entities"
CONF_TEMPERATURE_LED = "temperature_led"
CONF_TEMPERATURE_REPORTING = "temperature_reporting"
CONF_ZONES = "zones"

bridge_ns = cg.esphome_ns.namespace("temperature_encoding_bridge")
TemperatureEncodingBridge = bridge_ns.class_(
    "TemperatureEncodingBridge", cg.Component, uart.UARTDevice
)
Aggregation = bridge_ns.enum("Aggregation", is_class=True)

AGGREGATIONS = {
    "average": Aggregation.AVERAGE,
    "min": Aggregation.MINIMUM,
    "max": Aggregation.MAXIMUM,
}

ZONE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_GROUP): cv.int_range(min=1, max=16),
        cv.Optional(CONF_AGGREGATION, default="average"): cv.one_of(
            *AGGREGATIONS, lower=True
        ),
        cv.Required(CONF_TEMPERATURE_ENTITIES): cv.All(
            cv.ensure_list(cv.entity_id),
            cv.Length(min=1, max=3),
        ),
    }
)


def _validate_zones(zones):
    groups = [zone[CONF_GROUP] for zone in zones]
    duplicates = sorted({group for group in groups if groups.count(group) > 1})
    if duplicates:
        raise cv.Invalid(
            "Each zone may only be configured once; duplicates: "
            + ", ".join(str(group) for group in duplicates)
        )
    return zones

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TemperatureEncodingBridge),
            cv.Optional(
                CONF_MODULE_ADDRESS,
                default=[0x34, 0x65, 0x72, 0x30, 0x03, 0x19, 0x00, 0x79],
            ): cv.All([cv.hex_uint8_t], cv.Length(min=8, max=8)),
            cv.Optional(CONF_FALLBACK_ZONE_COUNT, default=1): cv.int_range(
                min=1, max=16
            ),
            cv.Optional(CONF_TEMPERATURE_REPORTING, default=True): cv.boolean,
            cv.Optional(CONF_RAW_LOGGING, default=False): cv.boolean,
            cv.Optional(CONF_TEMPERATURE_LED): cv.use_id(output.BinaryOutput),
            cv.Optional(CONF_ZONES, default=[]): cv.All(
                cv.ensure_list(ZONE_SCHEMA),
                cv.Length(max=16),
                _validate_zones,
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "temperature_encoding_bridge",
    baud_rate=115200,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    cg.add_define("USE_API_HOMEASSISTANT_STATES")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_module_address(config[CONF_MODULE_ADDRESS]))
    cg.add(var.set_fallback_zone_count(config[CONF_FALLBACK_ZONE_COUNT]))
    cg.add(var.set_temperature_reporting(config[CONF_TEMPERATURE_REPORTING]))
    cg.add(var.set_raw_logging(config[CONF_RAW_LOGGING]))

    if CONF_TEMPERATURE_LED in config:
        temperature_led = await cg.get_variable(config[CONF_TEMPERATURE_LED])
        cg.add(var.set_temperature_led(temperature_led))

    for zone in config[CONF_ZONES]:
        group = zone[CONF_GROUP]
        cg.add(var.add_zone(group, AGGREGATIONS[zone[CONF_AGGREGATION]]))

        for entity_id in zone[CONF_TEMPERATURE_ENTITIES]:
            cg.add(var.add_temperature_source(group, entity_id))
