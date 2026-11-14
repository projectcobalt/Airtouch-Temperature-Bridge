#pragma once

#ifdef USE_TEMPERATURE_BRIDGE_CALIBRATION_RESET
#include "esphome/components/button/button.h"

namespace esphome::temperature_encoding_bridge {

class TemperatureEncodingBridge;

class CalibrationResetButton : public button::Button {
 public:
  CalibrationResetButton(TemperatureEncodingBridge *parent, bool cancel) : parent_(parent), cancel_(cancel) {}

 protected:
  void press_action() override;
  TemperatureEncodingBridge *parent_;
  bool cancel_;
};

}  // namespace esphome::temperature_encoding_bridge
#endif
