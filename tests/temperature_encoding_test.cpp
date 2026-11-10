#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include "../components/temperature_encoding_bridge/temperature_encoding_bridge.cpp"

using namespace esphome::temperature_encoding_bridge;
class Bridge : public TemperatureEncodingBridge {
 public:
  using TemperatureEncodingBridge::encode_temperature_;
  using TemperatureEncodingBridge::send_next_temperature_;
};

// Independent receiver contract, expressed in integer tenths Celsius.
int decode(uint8_t raw) {
  assert(raw <= 0xFC);
  if (raw <= 0x27) return (int(raw) - 25) * 10;
  if (raw <= 0xE7) return int(raw) + 105;
  return (int(raw) - 197) * 10;
}

uint16_t receiver_crc(const std::vector<uint8_t> &frame) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 2; i < frame.size() - 2; ++i) {
    crc ^= frame[i];
    for (int bit = 0; bit < 8; ++bit) {
      bool low = crc & 1;
      crc >>= 1;
      if (low) crc ^= 0xA001;
    }
  }
  return crc;
}

int main() {
  for (int raw = 0; raw <= 0xFC; ++raw) {
    auto actual = Bridge::encode_temperature_(decode(raw) / 10.0f);
    if (actual != raw) {
      std::printf("roundtrip failed: raw=%02X actual=%02X\n", raw, actual);
      return 1;
    }
  }
  assert(Bridge::encode_temperature_(20.0f) == 0x5F);
  assert(Bridge::encode_temperature_(30.0f) == 0xC3);
  assert(Bridge::encode_temperature_(34.0f) == 0xE7);
  // Test both float neighbors of every midpoint, including both range gaps.
  for (int raw = 0; raw < 0xFC; ++raw) {
    float midpoint = (decode(raw) + decode(raw + 1)) / 20.0f;
    assert(Bridge::encode_temperature_(std::nextafter(midpoint, -INFINITY)) == raw);
    assert(Bridge::encode_temperature_(std::nextafter(midpoint, INFINITY)) == raw + 1);
    // Exact binary ties go to the warmer reading.
    if (double(midpoint) * 20.0 == decode(raw) + decode(raw + 1))
      assert(Bridge::encode_temperature_(midpoint) == raw + 1);
  }
  assert(Bridge::encode_temperature_(33.65f) == 0xE7);
  assert(Bridge::encode_temperature_(-1e30f) == 0x00);
  assert(Bridge::encode_temperature_(1e30f) == 0xFC);
  for (float t : {NAN, INFINITY, -INFINITY})
    assert(Bridge::encode_temperature_(t) == 0xFF);
  for (int tenths = -1000; tenths <= 1000; ++tenths)
    assert(Bridge::encode_temperature_(tenths / 10.0f) <= 0xFC);

  // Exercise actual report generation for every valid temperature.
  for (int raw = 0; raw <= 0xFC; ++raw) {
    Bridge bridge;
    esphome::sensor::Sensor source;
    source.state = decode(raw) / 10.0f;
    bridge.set_fallback_zone_count(16);
    bridge.add_zone(16, Aggregation::AVERAGE);
    bridge.add_temperature_source(16, &source);
    assert(bridge.send_next_temperature_());
    const auto first = bridge.written;
    assert(first.size() == 24);
    const std::vector<uint8_t> header = {0x55, 0x55, 0x80, 0x1E, 0x33, 0x65,
        0x72, 0x32, 0x03, 0x19, 0x10, 0x1E, 0x00, 0x01, 0x00, 0x06};
    assert(std::equal(header.begin(), header.end(), first.begin()));
    assert(first[16] == 0x00 && first[17] == raw);
    assert(first[18] == 0xB0 && first[19] == 0x12 && first[20] == 0x64 && first[21] == 0x94);
    assert(receiver_crc(first) == (uint16_t(first[22]) << 8 | first[23]));
    bridge.set_report_interval(0);
    assert(bridge.send_next_temperature_());
    assert(bridge.written[12] == 1);
  }
  Bridge invalid;
  esphome::sensor::Sensor a, b;
  a.state = b.state = std::numeric_limits<float>::max();
  invalid.add_zone(1, Aggregation::AVERAGE);
  invalid.add_temperature_source(1, &a);
  invalid.add_temperature_source(1, &b);
  assert(!invalid.send_next_temperature_()); // Overflowed aggregate is not sent.
  a.state = NAN;
  b.state = INFINITY;
  assert(!invalid.send_next_temperature_());
  std::puts("PASS: 253 roundtrips, float boundaries, invalid inputs, report frames and CRC");
}
