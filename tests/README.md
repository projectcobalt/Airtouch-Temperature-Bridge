# Component regression tests

These host tests compile the actual component with minimal ESPHome stubs. They
check temperature encoding and emitted sensor frames, rather than ESP hardware.
With an existing ESPHome Docker container (replace `esphome-builder` with its
name), copy just the source and tests into a temporary directory:

```sh
docker exec esphome-builder mkdir -p /tmp/temperature-bridge-tests/components /tmp/temperature-bridge-tests/tests
docker cp components/. esphome-builder:/tmp/temperature-bridge-tests/components/
docker cp tests/. esphome-builder:/tmp/temperature-bridge-tests/tests/
docker exec esphome-builder g++ -std=c++17 -I/tmp/temperature-bridge-tests/tests/stubs /tmp/temperature-bridge-tests/tests/temperature_encoding_test.cpp -o /tmp/temperature-bridge-tests/regression-test
docker exec esphome-builder /tmp/temperature-bridge-tests/regression-test
```

Alternatively, use a local C++17 compiler (GCC, Clang, or Zig):

```sh
mkdir -p build/tests
c++ -std=c++17 -Itests/stubs tests/temperature_encoding_test.cpp -o build/tests/temperature_encoding_test
./build/tests/temperature_encoding_test
```

On Windows with Zig, use PowerShell:

```powershell
New-Item -ItemType Directory -Force build/tests | Out-Null
zig c++ -std=c++17 -Itests/stubs tests/temperature_encoding_test.cpp -o build/tests/temperature_encoding_test.exe
./build/tests/temperature_encoding_test.exe
```

Assertions must remain enabled; do not compile with `NDEBUG`.

## Calibration reference controls

After copying the source and tests into the container as above:

```sh
docker exec esphome-builder g++ -std=c++17 -DUSE_TEMPERATURE_BRIDGE_CALIBRATION_RESET -I/tmp/temperature-bridge-tests/tests/stubs /tmp/temperature-bridge-tests/tests/calibration_reset_test.cpp -o /tmp/temperature-bridge-tests/calibration-test
docker exec esphome-builder /tmp/temperature-bridge-tests/calibration-test
docker exec esphome-builder python /tmp/temperature-bridge-tests/tests/calibration_config_test.py
docker exec esphome-builder python /tmp/temperature-bridge-tests/tests/calibration_codegen_test.py
```

The control test presses the buttons and checks UART frames and the active indicator.
It checks the timeout at its deadline and when the timer callback is late.
Other checks cover missing readings, changed inputs, excluded slots, Cancel,
repeated Start, timer wraparound, restart state and disabled reporting.
The existing encoding test runs with the calibration reference code disabled.

Use these ESPHome builds to check the YAML key and the generated controls:

```sh
docker exec esphome-builder esphome compile /tmp/temperature-bridge-tests/tests/configs/calibration_enabled.yaml
docker exec esphome-builder esphome compile /tmp/temperature-bridge-tests/tests/configs/calibration_disabled.yaml
docker exec esphome-builder esphome compile /tmp/temperature-bridge-tests/tests/configs/calibration_enabled_led.yaml --only-generate
docker exec esphome-builder esphome compile /tmp/temperature-bridge-tests/tests/configs/calibration_disabled_led.yaml --only-generate
```

When the key is absent, the generated source must contain no reference controls
or `USE_TEMPERATURE_BRIDGE_CALIBRATION_RESET` definition. Configurations with an LED
must keep the LED connected to the bridge. These configurations use the same build
name. Run them one at a time. Do not install these test builds on live equipment.
