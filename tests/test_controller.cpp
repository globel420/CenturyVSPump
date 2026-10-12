#include <cstdlib>
#include <iostream>
#include <string>

#include "../components/vgreen_evo_controller/VGreenEvoController.h"

namespace esphome { uint32_t test_now = 0; }
using esphome::vgreen_evo_controller::VGreenEvoController;
using esphome::vgreen_evo_controller::OperatingMode;

namespace {
int failures = 0;
void expect(bool result, const std::string &name) {
  if (!result) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}

struct Rig {
  esphome::century_vs_pump::CenturyVSPump pump;
  esphome::GPIOPin button;
  VGreenEvoController controller;
  uint8_t saved_profile{1};
  Rig() {
    esphome::test_now = 0;
    controller.set_pump(&pump);
    controller.set_button_pin(&button);
    controller.set_profile_save_callback([this](uint8_t profile) { saved_profile = profile; });
    controller.setup();
  }
  void tick(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i += 10) {
      esphome::test_now += 10; controller.loop();
    }
  }
  void ready() { pump.online = pump.status_known = true; tick(5000); }
  void press(uint32_t ms, uint32_t gap = 150) {
    button.pressed = true; tick(ms); button.pressed = false; tick(gap);
  }
};

void test_autonomous_start_and_recovery() {
  Rig rig;
  rig.tick(60000);
  expect(rig.controller.get_target_rpm() == 0, "No Run without pump replies");
  expect(rig.controller.get_schedule_hour() == 1, "Schedule begins at hour one without any network");
  rig.pump.online = true; rig.tick(10000);
  expect(rig.controller.get_target_rpm() == 0, "Unknown status cannot authorize Run");
  rig.ready();
  expect(rig.controller.get_target_rpm() == 3450, "Delayed motor startup recovers without reset or HA");
  rig.pump.online = false; rig.tick(1000);
  expect(rig.pump.demand == 0, "Lost communication requests Stop");
  rig.ready();
  expect(rig.controller.get_target_rpm() == 3450, "Link recovery runs without reset");

  rig.pump.fault = true; rig.tick(2000);
  expect(rig.controller.get_target_rpm() == 0, "Reported fault blocks Run");
  rig.pump.fault = false; rig.pump.status_known = false; rig.tick(2000);
  expect(rig.controller.get_target_rpm() == 0, "Fault clear requires known healthy motor status");
  rig.ready();
  expect(rig.controller.get_target_rpm() == 3450, "Motor-cleared fault does not require M5 reset");
}

void test_local_manual_controls_and_persistence_callback() {
  Rig rig;
  rig.ready();
  rig.press(120); rig.press(120); rig.tick(700);
  expect(rig.controller.is_paused() && rig.pump.demand == 0, "M5 double press stops locally");
  const uint32_t before = rig.controller.get_schedule_elapsed_seconds();
  rig.tick(5000);
  expect(rig.controller.get_schedule_elapsed_seconds() >= before + 5, "Schedule advances during Pause");
  rig.press(3000); rig.tick(5000);
  expect(!rig.controller.is_paused() && rig.controller.get_target_rpm() == 3450,
         "M5 hold resumes locally");
  rig.press(3000); rig.tick(1000);
  expect(rig.controller.is_selecting_schedule() && rig.pump.demand == 0, "Schedule Set stops motor");
  for (int i = 0; i < 6; ++i) rig.press(100);
  rig.press(3000); rig.tick(18000);
  expect(rig.saved_profile == 6 && rig.controller.get_schedule_profile() == 6,
         "Physical profile selection invokes local persistence without HA");
  expect(rig.controller.get_target_rpm() == 1100, "Saved A6 runs without priming");
}

void test_phase_sync_and_modes() {
  Rig rig;
  rig.controller.set_schedule_set(1);
  rig.ready();
  expect(rig.controller.set_schedule_phase(18 * 3600), "Valid phase accepted");
  rig.tick(1000);
  expect(rig.controller.get_schedule_hour() == 19 && rig.controller.get_target_rpm() == 0,
         "B1 hour 19 stops even during prime");
  rig.controller.set_schedule_phase(10 * 3600); rig.tick(5000);
  expect(rig.controller.get_target_rpm() == 3450, "Stopped-to-running phase change primes");
  for (int i = 0; i < 40; ++i) {
    rig.controller.set_schedule_phase(10 * 3600 + i * 5); rig.tick(5000);
  }
  expect(rig.controller.get_target_rpm() == 1550, "Repeated time correction does not restart prime timer");
  rig.controller.pause(); rig.tick(1000);
  rig.controller.set_schedule_phase(0); rig.tick(2000);
  expect(rig.controller.is_paused() && rig.controller.get_target_rpm() == 0,
         "HA alignment cannot release Pause");
  rig.controller.resume_schedule(); rig.tick(5000);
  rig.controller.start_override(); rig.tick(1000);
  rig.controller.set_schedule_phase(18 * 3600); rig.tick(2000);
  expect(rig.controller.is_override_active() && rig.controller.get_target_rpm() == 3450,
         "HA alignment cannot cancel Override");
  rig.controller.resume_schedule(); rig.tick(1000);
  expect(rig.controller.get_target_rpm() == 0, "Leaving Override uses corrected schedule phase");
  rig.controller.start_override(); rig.tick(1000);
  esphome::test_now += 24U * 60U * 60U * 1000U;
  rig.tick(1000);
  expect(!rig.controller.is_override_active() && rig.controller.get_target_rpm() == 0,
         "24-hour override expires without Home Assistant");
  const auto phase = rig.controller.get_schedule_elapsed_seconds();
  expect(!rig.controller.set_schedule_phase(-1) && !rig.controller.set_schedule_phase(86400),
         "Invalid phase rejected");
  expect(rig.controller.get_schedule_elapsed_seconds() == phase, "Invalid input leaves schedule untouched");
  rig.controller.set_schedule_phase(86399); rig.tick(1100);
  expect(rig.controller.get_schedule_hour() == 1, "Hour 24 wraps to hour one locally");
  for (unsigned hour = 1; hour <= 24; ++hour) {
    rig.controller.set_schedule_phase(static_cast<int>((hour - 1) * 3600));
    expect(rig.controller.get_schedule_hour() == hour, "One-based selectable hour " + std::to_string(hour));
  }
}
}  // namespace

int main() {
  test_autonomous_start_and_recovery();
  test_local_manual_controls_and_persistence_callback();
  test_phase_sync_and_modes();
  if (failures) return EXIT_FAILURE;
  std::cout << "PASS: actual controller logic, autonomous startup/recovery, button actions, and phase alignment\n";
  return EXIT_SUCCESS;
}
