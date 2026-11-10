#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
namespace esphome {
inline uint32_t millis() { return 100000; }
class Component {
 public:
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
  template<typename F> void set_timeout(const char *, uint32_t, F) {}
  template<typename F> void set_interval(const char *, uint32_t, F) {}
};
namespace sensor {
class Sensor {
 public:
  float state{0};
  bool valid{true};
  bool has_state() const { return valid; }
  template<typename F> void add_on_state_callback(F) {}
};
}
namespace output {
class BinaryOutput {
 public:
  void turn_on() {}
  void turn_off() {}
};
}
namespace uart {
constexpr int UART_CONFIG_PARITY_NONE = 0;
class UARTDevice {
 public:
  std::vector<uint8_t> written;
  int available() { return 0; }
  uint8_t read() { return 0; }
  void write_array(const uint8_t *data, size_t size) { written.assign(data, data + size); }
  void check_uart_settings(int, int, int, int) {}
};
}
}
#define ESP_LOGCONFIG(...) ((void) 0)
#define ESP_LOGW(...) ((void) 0)
#define ESP_LOGI(...) ((void) 0)
