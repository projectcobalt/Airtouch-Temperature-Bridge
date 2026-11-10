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
