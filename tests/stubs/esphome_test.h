#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <functional>
#include <string>
#include <utility>
namespace esphome {
inline uint32_t test_clock_ms = 100000;
inline uint32_t millis() { return test_clock_ms; }
class Component {
 public:
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
  template<typename F> void set_timeout(const char *name, uint32_t delay, F callback) {
    this->cancel_timeout(name);
    timers_.push_back({name, millis(), delay, callback});
  }
  bool cancel_timeout(const char *name) {
    for (auto it = timers_.begin(); it != timers_.end(); ++it) {
      if (it->name == name) {
        timers_.erase(it);
        return true;
      }
    }
    return false;
  }
  void run_timeouts() {
    for (size_t i = 0; i < timers_.size();) {
      if (millis() - timers_[i].start < timers_[i].delay) {
        ++i;
        continue;
      }
      auto callback = std::move(timers_[i].callback);
      timers_.erase(timers_.begin() + i);
      callback();
      i = 0;
    }
  }
  template<typename F> void set_interval(const char *, uint32_t, F) {}
 protected:
  struct Timer {
    std::string name;
    uint32_t start;
    uint32_t delay;
    std::function<void()> callback;
  };
  std::vector<Timer> timers_;
};
namespace button {
class Button {
 public:
  virtual ~Button() = default;
  void press() { this->press_action(); }
 protected:
  virtual void press_action() = 0;
};
}
namespace binary_sensor {
class BinarySensor {
 public:
  bool state{false};
  bool has_state{false};
  void publish_state(bool value) { state = value; has_state = true; }
};
}
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
  std::vector<std::vector<uint8_t>> written_frames;
  int available() { return 0; }
  uint8_t read() { return 0; }
  void write_array(const uint8_t *data, size_t size) {
    written.assign(data, data + size);
    written_frames.push_back(written);
  }
  void check_uart_settings(int, int, int, int) {}
};
}
}
#define ESP_LOGCONFIG(...) ((void) 0)
#define ESP_LOGW(...) ((void) 0)
#define ESP_LOGI(...) ((void) 0)
