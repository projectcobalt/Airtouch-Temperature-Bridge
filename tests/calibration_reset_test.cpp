#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include "../components/temperature_encoding_bridge/temperature_encoding_bridge.cpp"

using namespace esphome;
using namespace esphome::temperature_encoding_bridge;

class Bridge : public TemperatureEncodingBridge {
 public:
  using TemperatureEncodingBridge::send_next_temperature_;
};

uint16_t independent_crc(const std::vector<uint8_t> &frame) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 2; i < frame.size() - 2; ++i) {
    crc ^= frame[i];
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

void assert_report(const std::vector<uint8_t> &frame, int group, int raw, int sequence) {
  assert(frame.size() == 24 && frame[0] == 0x55 && frame[1] == 0x55);
  assert(frame[2] == 0x80 && frame[3] == (group - 1) * 2);
  assert(frame[4] == 0x33 && frame[5] == 0x65 && frame[6] == 0x72 && frame[7] == 0x32);
  assert(frame[8] == 0x03 && frame[9] == 0x19 && frame[10] == 0x10 && frame[11] == frame[3]);
  assert(frame[12] == sequence && frame[13] == 0x01 && frame[14] == 0 && frame[15] == 6);
  assert(frame[16] == 0 && frame[17] == raw && frame[18] == 0xB0);
  assert(frame[19] == 0x12 && frame[20] == 0x64 && frame[21] == 0x94);
  assert(independent_crc(frame) == ((uint16_t(frame[22]) << 8) | frame[23]));
}

int main() {
  test_clock_ms = 100000;
  Bridge bridge;
  binary_sensor::BinarySensor active;
  sensor::Sensor source;
  source.state = 25.0f;
  bridge.set_calibration_reset_active_sensor(&active);
  bridge.set_fallback_zone_count(7);
  for (int group : {1, 2, 3, 6, 7, 8}) {
    bridge.add_zone(group, Aggregation::AVERAGE);
    bridge.add_temperature_source(group, &source);
  }
  bridge.setup();
  assert(active.has_state && !active.state);
  CalibrationResetButton start(&bridge, false), cancel(&bridge, true);
  cancel.press();
  assert(!active.state);
  for (int group : {1, 2, 3, 6, 7}) {
    assert(bridge.send_next_temperature_());
    assert_report(bridge.written, group, 0x91, 0);
  }
  assert(!bridge.send_next_temperature_());

  // Entry clears the normal cooldown; unavailable sources do not block references.
  source.valid = false;
  source.state = NAN;
  start.press();
  assert(active.state);
  for (int group : {1, 2, 3, 6, 7}) {
    assert(bridge.send_next_temperature_());
    assert_report(bridge.written, group, 0x73, 1);
  }
  assert(!bridge.send_next_temperature_());

  // Live inputs change normally while the reference remains fixed.
  source.valid = true;
  source.state = 28.0f;
  test_clock_ms += 30000;
  assert(bridge.send_next_temperature_());
  assert_report(bridge.written, 1, 0x73, 2);
  cancel.press();
  assert(!active.state);
  assert(bridge.send_next_temperature_());
  assert_report(bridge.written, 2, 0xAF, 2);

  // No restart/extension on repeated Start; real scheduler fires at exactly 120 s.
  start.press();
  assert(active.state);
  test_clock_ms += 119999;
  start.press();
  bridge.run_timeouts();
  assert(active.state);
  ++test_clock_ms;
  bridge.run_timeouts();
  assert(!active.state);
  assert(bridge.send_next_temperature_());
  assert(bridge.written[17] == 0xAF);

  // A due report cannot leak a fixed value if the scheduler callback runs late.
  start.press();
  test_clock_ms += 120000;
  assert(bridge.send_next_temperature_());
  assert(!active.state && bridge.written[17] == 0xAF);

  // Correct across clock wrap; don't fabricate normal values after exit.
  test_clock_ms = UINT32_MAX - 60000;
  start.press();
  source.valid = false;
  test_clock_ms += 120000;
  assert(!bridge.send_next_temperature_());
  assert(!active.state);

  // Live controller count is always respected, even after activation.
  test_clock_ms = 100000;
  bridge.set_fallback_zone_count(1);
  start.press();
  assert(bridge.send_next_temperature_());
  assert(bridge.written[3] == 0);
  assert(!bridge.send_next_temperature_());
  cancel.press();

  // New objects/reboots are idle; disabled reporting and no eligible groups refuse Start.
  Bridge disabled;
  binary_sensor::BinarySensor disabled_active;
  disabled.set_calibration_reset_active_sensor(&disabled_active);
  disabled.add_zone(1, Aggregation::AVERAGE);
  disabled.set_temperature_reporting(false);
  disabled.setup();
  CalibrationResetButton disabled_start(&disabled, false);
  disabled_start.press();
  assert(!disabled_active.state && !disabled.send_next_temperature_());
  Bridge empty;
  binary_sensor::BinarySensor empty_active;
  empty.set_calibration_reset_active_sensor(&empty_active);
  empty.setup();
  CalibrationResetButton empty_start(&empty, false);
  empty_start.press();
  assert(!empty_active.state);
  empty.add_zone(2, Aggregation::AVERAGE);
  empty_start.press();
  assert(!empty_active.state);
  std::puts("PASS: maintenance controls, all eligible slots, timeout/cancel/reboot, invalid sources and framing");
}
