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

The control test presses the component's forwarding buttons and checks emitted
frames, status and deterministic scheduler expiry. It covers unavailable sources,
live input changes, slot filtering, cancellation, repeated Start, exact and delayed
timeout callbacks, clock wrap, reboot state and disabled reporting. The existing
encoding test also runs without the maintenance compile define.

Real ESPHome builds exercise the presence-only key and generated entity classes:

```sh
docker exec esphome-builder esphome compile /tmp/temperature-bridge-tests/tests/configs/calibration_enabled.yaml
docker exec esphome-builder esphome compile /tmp/temperature-bridge-tests/tests/configs/calibration_disabled.yaml
docker exec esphome-builder esphome compile /tmp/temperature-bridge-tests/tests/configs/calibration_enabled_led.yaml --only-generate
docker exec esphome-builder esphome compile /tmp/temperature-bridge-tests/tests/configs/calibration_disabled_led.yaml --only-generate
```

The omitted-key generated source must have no calibration-reference entities or
`USE_TEMPERATURE_BRIDGE_CALIBRATION_RESET` define. The LED variants must retain
their LED setter wiring. Fixtures share a build name and are run sequentially;
these sandbox builds are never intended for flashing live equipment.
