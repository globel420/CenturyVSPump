#pragma once
#include <cstdint>
#include <vector>
namespace esphome { namespace century_vs_pump {
// Deterministic transport double for controller unit tests, never a live test.
class CenturyVSPump {
 public:
  bool online{false};
  bool status_known{false};
  bool fault{false};
  bool acknowledge{true};
  uint32_t generation{0};
  uint16_t demand{0};
  std::vector<uint16_t> commands;
  bool communication_is_healthy(uint32_t) const { return online; }
  bool has_recent_no_fault_status(uint32_t) const { return online && status_known && !fault; }
  bool motor_fault_active() const { return fault; }
  bool request_stop(uint32_t *out) { return request_(0, out); }
  bool request_run(uint16_t rpm, uint32_t *out) { return request_(rpm, out); }
  bool motion_request_is_confirmed(uint32_t gen, uint16_t rpm) const {
    return online && acknowledge && gen == generation && rpm == demand;
  }
  bool motion_request_has_failed(uint32_t gen) const {
    return !online || !acknowledge || gen != generation;
  }
 private:
  bool request_(uint16_t rpm, uint32_t *out) {
    *out = ++generation; demand = rpm; commands.push_back(rpm); return true;
  }
};
} }
