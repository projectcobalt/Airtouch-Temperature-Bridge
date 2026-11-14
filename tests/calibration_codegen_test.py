"""Generate all maintenance/LED variants with installed ESPHome and inspect wiring."""

from pathlib import Path
import subprocess
import sys

configs = Path(__file__).resolve().parent / "configs"
source = configs / ".esphome" / "build" / "bridge-calibration-test" / "src"
for enabled in (False, True):
    for led in (False, True):
        name = "calibration_" + ("enabled" if enabled else "disabled") + ("_led" if led else "")
        subprocess.run(
            [sys.executable, "-m", "esphome", "compile", str(configs / f"{name}.yaml"), "--only-generate"],
            check=True,
        )
        main = (source / "main.cpp").read_text()
        defines = (source / "esphome" / "core" / "defines.h").read_text()
        assert ("USE_TEMPERATURE_BRIDGE_CALIBRATION_RESET" in defines) == enabled
        assert ("CalibrationResetButton(bridge, false)" in main) == enabled
        assert ("CalibrationResetButton(bridge, true)" in main) == enabled
        assert ("bridge->set_calibration_reset_active_sensor(" in main) == enabled
        assert ("Calibration Reference Active" in main) == enabled
        assert ("bridge->set_temperature_led(temperature_update_led)" in main) == led
        assert (source / "esphome" / "components" / "output" / "binary_output.h").exists()
        assert (source / "esphome" / "components" / "button" / "button.h").exists() == enabled
        assert (source / "esphome" / "components" / "binary_sensor" / "binary_sensor.h").exists() == enabled
print("PASS: four real ESPHome codegen variants, optional controls/dependencies and LED wiring")
