#include "temperature_encoding_bridge.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "esphome/components/api/api_server.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace temperature_encoding_bridge {

static const char *const TAG = "temperature_encoding_bridge";

StreamParser::StreamParser() { this->buffer_.reserve(1024); }

void StreamParser::append(uint8_t byte) { this->buffer_.push_back(byte); }

size_t StreamParser::available_() const {
  return this->buffer_.size() - this->cursor_;
}

void StreamParser::compact_() {
  if (this->cursor_ == 0)
    return;
  if (this->cursor_ >= this->buffer_.size()) {
    this->buffer_.clear();
    this->cursor_ = 0;
    return;
  }
  if (this->cursor_ >= 1024 || this->cursor_ * 2 >= this->buffer_.size()) {
    this->buffer_.erase(this->buffer_.begin(),
                        this->buffer_.begin() + this->cursor_);
    this->cursor_ = 0;
  }
}

bool StreamParser::next_frame(std::vector<uint8_t> &frame) {
  while (this->available_() >= 2) {
    if (this->buffer_[this->cursor_] != 0x55 ||
        this->buffer_[this->cursor_ + 1] != 0x55) {
      this->cursor_++;
      this->resync_count_++;
      continue;
    }
    if (this->available_() < FRAME_OVERHEAD) {
      this->compact_();
      return false;
    }

    const uint8_t destination = this->buffer_[this->cursor_ + 2];
    const uint8_t source = this->buffer_[this->cursor_ + 3];
    if (source != 0x80 ||
        (destination != 0xF0 && destination > 0x1F)) {
      this->cursor_++;
      this->resync_count_++;
      continue;
    }

    const size_t payload_len =
        (static_cast<size_t>(this->buffer_[this->cursor_ + 14]) << 8) |
        this->buffer_[this->cursor_ + 15];
    if (payload_len > 512) {
      this->cursor_++;
      this->resync_count_++;
      continue;
    }

    const size_t frame_len = FRAME_OVERHEAD + payload_len;
    if (this->available_() < frame_len) {
      this->compact_();
      return false;
    }

    frame.assign(this->buffer_.begin() + this->cursor_,
                 this->buffer_.begin() + this->cursor_ + frame_len);
    this->cursor_ += frame_len;
    this->compact_();
    return true;
  }
  this->compact_();
  return false;
}

void TemperatureEncodingBridge::set_module_address(
    const std::vector<uint8_t> &address) {
  if (address.size() == this->module_address_.size())
    std::copy(address.begin(), address.end(), this->module_address_.begin());
}

void TemperatureEncodingBridge::set_fallback_zone_count(uint8_t count) {
  this->controller_zone_count_ = std::max<uint8_t>(
      1, std::min<uint8_t>(MAX_ZONES, count));
}

void TemperatureEncodingBridge::set_temperature_reporting(bool enabled) {
  this->temperature_reporting_ = enabled;
}

void TemperatureEncodingBridge::set_raw_logging(bool enabled) {
  this->raw_logging_ = enabled;
}

void TemperatureEncodingBridge::set_temperature_led(
    output::BinaryOutput *led) {
  this->temperature_led_ = led;
}

void TemperatureEncodingBridge::add_zone(uint8_t group,
                                         Aggregation aggregation) {
  this->zones_.push_back({group, aggregation, {}});
}

void TemperatureEncodingBridge::add_temperature_source(
    uint8_t group, const std::string &entity_id) {
  ZoneConfig *zone = this->find_zone_(group);
  if (zone == nullptr)
    return;
  zone->sources.push_back({entity_id});
}

ZoneConfig *TemperatureEncodingBridge::find_zone_(uint8_t group) {
  for (auto &zone : this->zones_) {
    if (zone.group == group)
      return &zone;
  }
  return nullptr;
}

void TemperatureEncodingBridge::setup() {
  this->rx_frame_.reserve(FRAME_OVERHEAD + 512);
  this->tx_frame_.reserve(FRAME_OVERHEAD + 512);
  this->tx_payload_.reserve(512);
  if (this->temperature_led_ != nullptr)
    this->temperature_led_->turn_off();
  for (auto &zone : this->zones_) {
    for (auto &source : zone.sources) {
      auto *source_ptr = &source;
      api::global_api_server->subscribe_home_assistant_state(
          source.entity_id.c_str(), nullptr,
          [this, source_ptr](StringRef state) {
            const auto value = parse_number<float>(state.c_str());
            source_ptr->has_state = value.has_value();
            source_ptr->state = value.value_or(NAN);
            if (!source_ptr->has_state) {
              ESP_LOGW(TAG, "Cannot convert HA state '%s' for %s",
                       state.c_str(), source_ptr->entity_id.c_str());
              return;
            }
            this->pulse_temperature_led_();
      });
    }
  }
  this->set_timeout("ready_1", 3000, [this]() { this->send_ready_(); });
  this->set_timeout("ready_2", 13000, [this]() { this->send_ready_(); });
  this->set_interval("temperature_scan", 5000,
                     [this]() { this->send_next_temperature_(); });
}

void TemperatureEncodingBridge::loop() {
  size_t bytes_read = 0;
  while (this->available() > 0 && bytes_read < LOOP_BYTE_BUDGET) {
    this->parser_.append(this->read());
    bytes_read++;
  }

  size_t frames_processed = 0;
  while (frames_processed < LOOP_FRAME_BUDGET &&
         this->parser_.next_frame(this->rx_frame_)) {
    this->process_frame_(this->rx_frame_);
    frames_processed++;
  }
}

void TemperatureEncodingBridge::dump_config() {
  ESP_LOGCONFIG(TAG, "Temperature Encoding Bridge:");
  ESP_LOGCONFIG(TAG, "  Configured zones: %u",
                static_cast<unsigned>(this->zones_.size()));
  ESP_LOGCONFIG(TAG, "  Controller zone count: %u",
                this->controller_zone_count_);
  ESP_LOGCONFIG(TAG, "  Temperature reporting: %s",
                this->temperature_reporting_ ? "enabled" : "disabled");
  for (const auto &zone : this->zones_) {
    const char *mode = zone.aggregation == Aggregation::AVERAGE
                           ? "average"
                           : zone.aggregation == Aggregation::MINIMUM ? "min"
                                                                     : "max";
    ESP_LOGCONFIG(TAG, "  Group %u: %u source(s), %s", zone.group,
                  static_cast<unsigned>(zone.sources.size()), mode);
  }
  this->check_uart_settings(115200, 1, uart::UART_CONFIG_PARITY_NONE, 8);
}

uint16_t TemperatureEncodingBridge::crc16_modbus_(const uint8_t *data,
                                                   size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++)
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

void TemperatureEncodingBridge::append_crc_(std::vector<uint8_t> &frame) {
  const uint16_t crc =
      crc16_modbus_(&frame[2], frame.size() - 2);
  frame.push_back((crc >> 8) & 0xFF);
  frame.push_back(crc & 0xFF);
}

uint8_t TemperatureEncodingBridge::encode_temperature_(float temperature) {
  if (temperature < 14.5f) {
    int raw = static_cast<int>(lroundf(temperature)) + 25;
    return static_cast<uint8_t>(std::max(1, std::min(0x27, raw)));
  }
  int raw = static_cast<int>(lroundf(temperature * 10.0f - 105.0f));
  return static_cast<uint8_t>(std::max(0x28, std::min(0xFF, raw)));
}

uint8_t TemperatureEncodingBridge::slot_for_group_(uint8_t group) {
  return static_cast<uint8_t>((group - 1) * 2);
}

std::string TemperatureEncodingBridge::hex_bytes_(const uint8_t *data,
                                                   size_t len) {
  static const char *const HEX_CHARS = "0123456789ABCDEF";
  std::string result;
  result.reserve(len * 3);
  for (size_t i = 0; i < len; i++) {
    if (i != 0)
      result.push_back(' ');
    result.push_back(HEX_CHARS[(data[i] >> 4) & 0x0F]);
    result.push_back(HEX_CHARS[data[i] & 0x0F]);
  }
  return result;
}

void TemperatureEncodingBridge::process_frame_(
    const std::vector<uint8_t> &frame) {
  this->rx_frame_count_++;
  const uint8_t destination = frame[2];
  const uint8_t source = frame[3];
  const uint8_t sequence = frame[12];
  const uint8_t command = frame[13];
  const size_t payload_len =
      (static_cast<size_t>(frame[14]) << 8) | frame[15];
  const uint16_t received_crc =
      (static_cast<uint16_t>(frame[frame.size() - 2]) << 8) |
      frame[frame.size() - 1];
  const uint16_t calculated_crc =
      crc16_modbus_(&frame[2], frame.size() - 4);

  if (received_crc != calculated_crc) {
    this->crc_error_count_++;
    ESP_LOGW(TAG,
             "RX bad CRC dest=%02X src=%02X seq=%02X cmd=%02X len=%u "
             "rx=%04X calc=%04X",
             destination, source, sequence, command,
             static_cast<unsigned>(payload_len), received_crc,
             calculated_crc);
    return;
  }

  if (this->raw_logging_) {
    ESP_LOGI("temperature_bridge.raw",
             "dir=rx dest=%02X src=%02X seq=%02X cmd=%02X payload_len=%u "
             "crc_ok=true hex=\"%s\"",
             destination, source, sequence, command,
             static_cast<unsigned>(payload_len),
             hex_bytes_(frame.data(), frame.size()).c_str());
  }

  if (command == 0x00 && payload_len >= 2) {
    this->pairing_mode_ = (frame[16] & 0x80) != 0;
    this->controller_zone_count_ =
        std::min<uint8_t>(MAX_ZONES, frame[17] + 1);
    this->send_ack_(frame);
    return;
  }

  if (command == 0x03 && payload_len == 8) {
    this->send_ack_(frame);
    return;
  }

  if (command == 0x01 && payload_len > 0 && payload_len % 2 == 0) {
    this->controller_zone_count_ =
        std::min<uint8_t>(MAX_ZONES, payload_len / 2);
    if (this->raw_logging_) {
      for (uint8_t group = 0; group < this->controller_zone_count_;
           group++) {
        const uint8_t flags = frame[16 + group * 2];
        const uint8_t status = frame[17 + group * 2];
        ESP_LOGI("temperature_bridge.map",
                 "group=%u slot=%02X flags=%02X configured=%u open=%u "
                 "status=%02X setpoint=%u low_batt=%u turbo=%u timer=%u",
                 group + 1, group * 2, flags, (flags & 0x80) != 0,
                 (flags & 0x40) != 0, status, (status & 0x1F) + 4,
                 (status & 0x80) != 0, (status & 0x40) != 0,
                 (status & 0x20) != 0);
      }
    }
  }
}

void TemperatureEncodingBridge::send_frame_(
    uint8_t destination, uint8_t source, uint8_t sequence, uint8_t command,
    const std::vector<uint8_t> &payload, const char *direction) {
  this->tx_frame_.clear();
  this->tx_frame_.push_back(0x55);
  this->tx_frame_.push_back(0x55);
  this->tx_frame_.push_back(destination);
  this->tx_frame_.push_back(source);
  this->tx_frame_.insert(this->tx_frame_.end(), this->module_address_.begin(),
                         this->module_address_.end());
  this->tx_frame_.push_back(sequence);
  this->tx_frame_.push_back(command);
  this->tx_frame_.push_back((payload.size() >> 8) & 0xFF);
  this->tx_frame_.push_back(payload.size() & 0xFF);
  this->tx_frame_.insert(this->tx_frame_.end(), payload.begin(), payload.end());
  append_crc_(this->tx_frame_);
  this->write_array(this->tx_frame_.data(), this->tx_frame_.size());
  this->tx_frame_count_++;

  if (this->raw_logging_) {
    ESP_LOGI("temperature_bridge.raw",
             "dir=%s dest=%02X src=%02X seq=%02X cmd=%02X payload_len=%u "
             "hex=\"%s\"",
             direction, destination, source, sequence, command,
             static_cast<unsigned>(payload.size()),
             hex_bytes_(this->tx_frame_.data(), this->tx_frame_.size()).c_str());
  }
}

void TemperatureEncodingBridge::send_ack_(
    const std::vector<uint8_t> &frame) {
  const size_t payload_len =
      (static_cast<size_t>(frame[14]) << 8) | frame[15];
  const uint8_t source = frame[2] == 0xF0 ? 0x80 : frame[2];
  this->tx_payload_.assign(frame.begin() + 16,
                           frame.begin() + 16 + payload_len);
  this->send_frame_(0x80, source, frame[12], frame[13], this->tx_payload_,
                    "tx_ack");
}

void TemperatureEncodingBridge::send_ready_() {
  this->tx_payload_.clear();
  this->tx_payload_.push_back(0x20);
  this->tx_payload_.push_back(this->controller_zone_count_ - 1);
  this->send_frame_(0x80, 0x80, this->ready_sequence_++, 0x00,
                    this->tx_payload_, "tx_ready");
}

bool TemperatureEncodingBridge::aggregate_zone_(
    const ZoneConfig &zone, float &temperature) const {
  bool found = false;
  float total = 0.0f;
  size_t count = 0;
  for (const auto &source : zone.sources) {
    if (!source.has_state || !std::isfinite(source.state))
      continue;
    if (!found) {
      temperature = source.state;
      found = true;
    } else if (zone.aggregation == Aggregation::MINIMUM) {
      temperature = std::min(temperature, source.state);
    } else if (zone.aggregation == Aggregation::MAXIMUM) {
      temperature = std::max(temperature, source.state);
    }
    total += source.state;
    count++;
  }
  if (!found)
    return false;
  if (zone.aggregation == Aggregation::AVERAGE)
    temperature = total / count;
  return true;
}

bool TemperatureEncodingBridge::send_next_temperature_() {
  if (!this->temperature_reporting_ || this->zones_.empty())
    return false;

  const uint32_t now = millis();
  for (size_t attempt = 0; attempt < this->zones_.size(); attempt++) {
    ZoneConfig &zone = this->zones_[this->next_zone_index_];
    this->next_zone_index_ =
        (this->next_zone_index_ + 1) % this->zones_.size();

    if (zone.group > this->controller_zone_count_)
      continue;
    if (zone.last_send_ms != 0 &&
        now - zone.last_send_ms < REPORT_MIN_INTERVAL_MS)
      continue;

    float temperature;
    if (!this->aggregate_zone_(zone, temperature))
      continue;

    const uint8_t slot = slot_for_group_(zone.group);
    this->tx_frame_ = {0x55, 0x55, 0x80, slot,
                       0x33, 0x65, 0x72, 0x32,
                       0x03, 0x19, 0x10, slot,
                       zone.sequence++, 0x01, 0x00, 0x06,
                       0x00, encode_temperature_(temperature),
                       0xB0, 0x12, 0x64, 0x94};
    append_crc_(this->tx_frame_);
    this->write_array(this->tx_frame_.data(), this->tx_frame_.size());
    this->tx_frame_count_++;
    zone.last_send_ms = now;

    if (this->raw_logging_) {
      ESP_LOGI("temperature_bridge.raw",
               "dir=tx_sensor group=%u slot=%02X seq=%02X cmd=01 "
               "temp=%.1f temp_raw=%02X sources=%u hex=\"%s\"",
               zone.group, slot, static_cast<uint8_t>(zone.sequence - 1),
               temperature, this->tx_frame_[17],
               static_cast<unsigned>(zone.sources.size()),
               hex_bytes_(this->tx_frame_.data(),
                          this->tx_frame_.size()).c_str());
    }
    return true;
  }
  return false;
}

void TemperatureEncodingBridge::pulse_temperature_led_() {
  if (this->temperature_led_ == nullptr)
    return;
  this->temperature_led_->turn_on();
  this->set_timeout("temperature_led", 150,
                    [this]() { this->temperature_led_->turn_off(); });
}

}  // namespace temperature_encoding_bridge
}  // namespace esphome
