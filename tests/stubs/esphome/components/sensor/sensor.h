#pragma once
#include <functional>
namespace esphome { namespace sensor {
class Sensor {
 public:
  float state{0};
  bool valid{false};
  bool has_state() const { return valid; }
  void add_on_state_callback(std::function<void(float)> callback) { callback_ = callback; }
 private:
  std::function<void(float)> callback_;
};
} }
