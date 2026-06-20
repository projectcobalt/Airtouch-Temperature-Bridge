# AirTouch Temperature Bridge

AirTouch Temperature Bridge is an ESPHome replacement for the original wireless
temperature receiver used by compatible AirTouch 4 installations. It lets Home
Assistant temperature entities act as zone sensors while leaving the main
controller and normal control interface in place.

This project is intended for technically confident owners and integrators who:

- already run Home Assistant and ESPHome;
- have a compatible controller with the removable temperature receiver module;
- want to use wired, Zigbee, Bluetooth, Wi-Fi, or calculated Home Assistant
  temperatures instead of maintaining the original wireless sensors;
- are comfortable identifying low-voltage UART pins and validating voltage
  levels before connecting an ESP32.

It is not a general HVAC controller and does not replace the main controller,
touchscreen, zoning logic, or safety controls.

## Why This Exists

Temperature sensing should not be tied to one discontinued radio ecosystem.
Home Assistant already has access to reliable room sensors, averages, and
derived temperatures. This bridge translates those existing readings into the
compact sensor reports expected by the controller.

Each configured zone can use one to three Home Assistant entities. Multiple
readings can be combined using `average`, `min`, or `max`.

## Hardware

The reference build uses a Wemos/Lolin S2 Mini:

| Function | Default pin |
|---|---:|
| Controller data into ESP | GPIO7 |
| ESP data into controller | GPIO5 |
| ESPHome status LED | GPIO18 |
| Temperature update LED | GPIO16 |

Confirm the electrical interface before wiring. The UART must share ground with
the controller, and its voltage must be safe for the ESP32. This repository does
not provide power-supply or level-shifter guarantees for every installation.

## Install

1. Create a new ESPHome device using the board appropriate for your hardware.
2. Replace the generated device YAML with
   [examples/basic.yaml](examples/basic.yaml).
3. Set the four pin substitutions for your board and wiring.
4. Add the Home Assistant temperature entities for each required zone.
5. Validate and install the device from ESPHome.

The custom component is loaded with:

```yaml
external_components:
  - source: github://projectcobalt/Airtouch-Temperature-Bridge
    components:
      - temperature_encoding_bridge
```

Pin mapping remains device-specific:

```yaml
substitutions:
  bridge_rx_pin: GPIO7
  bridge_tx_pin: GPIO5
  status_led_pin: GPIO18
  temperature_led_pin: GPIO16
```

Then configure the required zones:

```yaml
temperature_encoding_bridge:
  id: temperature_bridge
  uart_id: bridge_uart
  fallback_zone_count: 5
  temperature_led: temperature_update_led
  zones:
    - group: 1
      temperature_entities:
        - sensor.living_room_temperature

    - group: 2
      aggregation: average
      temperature_entities:
        - sensor.bedroom_temperature
        - sensor.bedroom_remote_temperature
```

Only listed groups are enabled. Groups must be unique and between 1 and 16.
Each group accepts one to three numeric Home Assistant entities. Invalid or
unavailable readings are ignored; the group waits until at least one source has
a valid value.

## First Flash

[factory.yaml](factory.yaml) provides a serial Improv first-flash configuration.
After adoption, configure the board pins and zones using
[examples/basic.yaml](examples/basic.yaml).

## Diagnostics

The example exposes slow-updating hardware health entities:

- uptime, Wi-Fi signal, ESP temperature, and ESPHome version;
- free, minimum, and largest heap values;
- heap fragmentation and loop time;
- reset reason, IP address, SSID, and MAC address;
- received/transmitted frame counts, CRC errors, and parser resynchronisations;
- a restart button.

## Protocol Overview

The receiver link is a framed binary UART protocol. Frames contain addressing,
a sequence value, a command, a payload length, payload data, and a CRC. The
bridge acknowledges controller management messages, learns the active zone
count, and rotates encoded temperature reports for configured zones.

The implementation is intentionally limited to the interoperability needed for
temperature reporting. This repository does not publish controller firmware,
application code, confidential documentation, cryptographic material, or a
general-purpose protocol dump.

## Repository Layout

```text
components/temperature_encoding_bridge/  ESPHome external component
examples/basic.yaml                       Annotated installed-device example
factory.yaml                              Serial Improv first-flash configuration
```

## Status

Current release: `v1.0.0`

The component supports up to 16 zones, one to three source sensors per zone,
dynamic frame lengths, CRC validation, controller restart recovery, and bounded
UART processing. It has been tested on an ESP32-S2 with ESPHome 2026.5.3.

## Disclaimer

This is an independent, unofficial interoperability project. AirTouch and
related product names are trademarks of their respective owners. Use is at your
own risk and may affect warranty or support eligibility.
