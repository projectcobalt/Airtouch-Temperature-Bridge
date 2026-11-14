"""Presence-only maintenance shorthand and its generated entities."""

import esphome.codegen as cg
from esphome.components import binary_sensor, button
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

bridge_ns = cg.esphome_ns.namespace("temperature_encoding_bridge")
CalibrationResetButton = bridge_ns.class_("CalibrationResetButton", button.Button)


@schema_extractor("schema")
def validate_calibration_reset(value: ConfigType | None) -> ConfigType:
    if value is SCHEMA_EXTRACT:
        return cv.Schema({})
    if value is not None and (not isinstance(value, dict) or value):
        raise cv.Invalid("calibration_reset must be an empty key; omit it to disable")
    # These entity configs are generated internally, not user-facing options.
    return {
        "start": button.button_schema(
            CalibrationResetButton, entity_category="config", icon="mdi:thermometer-check"
        )({"name": "Start Calibration Reference"}),
        "cancel": button.button_schema(
            CalibrationResetButton, entity_category="config", icon="mdi:close-circle-outline"
        )({"name": "Cancel Calibration Reference"}),
        "active": binary_sensor.binary_sensor_schema(
            entity_category="diagnostic", icon="mdi:thermometer-check"
        )({"name": "Calibration Reference Active"}),
    }


async def to_code(bridge, config: ConfigType) -> None:
    cg.add_define("USE_TEMPERATURE_BRIDGE_CALIBRATION_RESET")
    for key, cancel in (("start", False), ("cancel", True)):
        var = cg.new_Pvariable(config[key][CONF_ID], bridge, cancel)
        await button.register_button(var, config[key])
    active = await binary_sensor.new_binary_sensor(config["active"])
    cg.add(bridge.set_calibration_reset_active_sensor(active))
