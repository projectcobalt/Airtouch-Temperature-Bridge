#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "esphome/components/output/binary_output.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace temperature_encoding_bridge {

constexpr uint8_t MAX_ZONES = 16;
constexpr size_t FRAME_OVERHEAD = 18;
constexpr uint32_t REPORT_MIN_INTERVAL_MS = 30000;
constexpr size_t LOOP_BYTE_BUDGET = 256;
constexpr size_t LOOP_FRAME_BUDGET = 8;

enum class Aggregation : uint8_t {
  AVERAGE = 0,
  MINIMUM = 1,
  MAXIMUM = 2,
};

struct TemperatureSource {
  std::string entity_id;
  float state{NAN};
  bool has_state{false};
};

struct ZoneConfig {
  uint8_t group;
  Aggregation aggregation;
  std::vector<TemperatureSource> sources;
  uint8_t sequence{0};
  uint32_t last_send_ms{0};
};

class StreamParser {
 public:
  StreamParser();
  void append(uint8_t byte);
  bool next_frame(std::vector<uint8_t> &frame);
  uint32_t get_resync_count() const { return this->resync_count_; }

 protected:
  void compact_();
  size_t available_() const;

  std::vector<uint8_t> buffer_;
  size_t cursor_{0};
  uint32_t resync_count_{0};
};

class TemperatureEncodingBridge : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_module_address(const std::vector<uint8_t> &address);
  void set_fallback_zone_count(uint8_t count);
  void set_temperature_reporting(bool enabled);
  void set_raw_logging(bool enabled);
  void set_temperature_led(output::BinaryOutput *led);
  void add_zone(uint8_t group, Aggregation aggregation);
  void add_temperature_source(uint8_t group, const std::string &entity_id);
  uint32_t get_rx_frame_count() const { return this->rx_frame_count_; }
  uint32_t get_tx_frame_count() const { return this->tx_frame_count_; }
  uint32_t get_crc_error_count() const { return this->crc_error_count_; }
  uint32_t get_parser_resync_count() const {
    return this->parser_.get_resync_count();
  }

 protected:
  static uint16_t crc16_modbus_(const uint8_t *data, size_t len);
  static void append_crc_(std::vector<uint8_t> &frame);
  static uint8_t encode_temperature_(float temperature);
  static uint8_t slot_for_group_(uint8_t group);
  static std::string hex_bytes_(const uint8_t *data, size_t len);

  ZoneConfig *find_zone_(uint8_t group);
  bool aggregate_zone_(const ZoneConfig &zone, float &temperature) const;
  void process_frame_(const std::vector<uint8_t> &frame);
  void send_frame_(uint8_t destination, uint8_t source, uint8_t sequence,
                   uint8_t command, const std::vector<uint8_t> &payload,
                   const char *direction);
  void send_ack_(const std::vector<uint8_t> &frame);
  void send_ready_();
  bool send_next_temperature_();
  void pulse_temperature_led_();

  std::array<uint8_t, 8> module_address_{
      0x34, 0x65, 0x72, 0x30, 0x03, 0x19, 0x00, 0x79};
  std::vector<ZoneConfig> zones_;
  StreamParser parser_;
  std::vector<uint8_t> rx_frame_;
  std::vector<uint8_t> tx_frame_;
  std::vector<uint8_t> tx_payload_;
  output::BinaryOutput *temperature_led_{nullptr};

  uint8_t controller_zone_count_{1};
  uint8_t ready_sequence_{0};
  size_t next_zone_index_{0};
  uint32_t rx_frame_count_{0};
  uint32_t tx_frame_count_{0};
  uint32_t crc_error_count_{0};
  bool pairing_mode_{false};
  bool temperature_reporting_{true};
  bool raw_logging_{false};
};

}  // namespace temperature_encoding_bridge
}  // namespace esphome
