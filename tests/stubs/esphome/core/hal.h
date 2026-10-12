#pragma once
#include <cstdint>
namespace esphome {
extern uint32_t test_now;
inline uint32_t millis() { return test_now; }
class GPIOPin {
 public:
  bool pressed{false};
  void setup() {}
  bool digital_read() const { return pressed; }
};
}
