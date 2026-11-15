"""Run with the ESPHome Python environment; no device connection is used."""

import importlib.util
from pathlib import Path
import sys
import unittest

import esphome.config_validation as cv
from esphome.core import CORE
from esphome.schema_extractors import SCHEMA_EXTRACT

COMPONENT = Path(__file__).resolve().parents[1] / "components" / "temperature_encoding_bridge"
SPEC = importlib.util.spec_from_file_location(
    "esphome.components.temperature_encoding_bridge",
    COMPONENT / "__init__.py",
    submodule_search_locations=[str(COMPONENT)],
)
bridge = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = bridge
SPEC.loader.exec_module(bridge)


class CalibrationConfigTests(unittest.TestCase):
    def setUp(self):
        CORE.reset()

    def test_presence_only_and_generated_controls(self):
        for value in (None, {}):
            CORE.reset()
            config = bridge.CONFIG_SCHEMA({"calibration_reset": value})
            controls = config["calibration_reset"]
            self.assertEqual(controls["start"]["name"], "Start Calibration Reference")
            self.assertEqual(controls["cancel"]["name"], "Cancel Calibration Reference")
            self.assertEqual(controls["active"]["name"], "Calibration Reference Active")
            self.assertEqual(controls["start"]["entity_category"], "config")
            self.assertEqual(controls["active"]["entity_category"], "diagnostic")
            self.assertIn("button", bridge.AUTO_LOAD(config))
            self.assertIn("binary_sensor", bridge.AUTO_LOAD(config))

    def test_omission_has_no_controls_or_entity_dependencies(self):
        config = bridge.CONFIG_SCHEMA({})
        self.assertNotIn("calibration_reset", config)
        self.assertEqual(bridge.AUTO_LOAD(config), ["output", "sensor"])

    def test_reject_boolean_scalar_and_options(self):
        for value in (True, False, 22, "", "true", [], {"temperature": 22}, {"timeout": "2min"}):
            with self.subTest(value=value), self.assertRaises(cv.Invalid):
                bridge.CONFIG_SCHEMA({"calibration_reset": value})

    def test_schema_extraction_exposes_an_empty_declaration(self):
        schema = bridge.calibration_reset.validate_calibration_reset(SCHEMA_EXTRACT)
        self.assertIsInstance(schema, cv.Schema)
        self.assertEqual(schema.schema, {})


if __name__ == "__main__":
    unittest.main()
