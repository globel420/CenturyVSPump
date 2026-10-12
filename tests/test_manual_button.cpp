#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "../components/vgreen_evo_controller/manual_button.h"

using vgreen_evo::manual::Action;
using vgreen_evo::manual::Button;

namespace {
int failures = 0;
void expect(bool result, const std::string &name) {
  if (!result) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}

struct Rig {
  Button button;
  uint32_t now;
  bool paused{false};
  bool override_active{false};
  uint8_t profile{1};
  std::vector<Action> actions;
  explicit Rig(uint32_t start = 0, bool held = false) : now(start) {
    button.update(held, now, paused, override_active, profile);
  }
  void drive(bool pressed, uint32_t duration) {
    for (uint32_t i = 0; i < duration; ++i) {
      ++now;
      const Action action = button.update(pressed, now, paused, override_active, profile);
      if (action == Action::NONE) continue;
      actions.push_back(action);
      if (action == Action::PAUSE) { paused = true; override_active = false; }
      if (action == Action::OVERRIDE) override_active = true;
      if (action == Action::RESUME) paused = override_active = false;
      if (action == Action::SAVE_PROFILE) profile = button.selected_profile();
    }
  }
  void click(uint32_t duration = 120, uint32_t gap = 120) {
    drive(true, duration); drive(false, gap);
  }
  void finish() { drive(false, 600); }
  void hold() { drive(true, 3000); drive(false, 40); }
  bool only(Action action) const { return actions.size() == 1 && actions[0] == action; }
};

void test_clicks() {
  for (unsigned clicks = 1; clicks <= 4; ++clicks) {
    Rig rig;
    for (unsigned i = 0; i < clicks; ++i) rig.click();
    expect(rig.actions.empty(), "No single-click action before a multiple-click sequence ends");
    rig.finish();
    const Action expected[] = {Action::NONE, Action::OVERRIDE, Action::PAUSE, Action::CHECK, Action::INVALID};
    expect(rig.only(expected[clicks]), "Exact click classification " + std::to_string(clicks));
  }
  Rig slow;
  slow.click(800, 450); slow.click(700, 450); slow.finish();
  expect(slow.only(Action::PAUSE), "Ordinary slower presses and 450ms gaps work");
}

void test_hold_and_context() {
  Rig normal;
  normal.drive(true, 5000);
  expect(normal.actions.empty(), "Hold acts on release, never while held");
  normal.drive(false, 40);
  expect(normal.only(Action::ENTER_SET), "Normal hold enters Schedule Set");

  for (bool pause : {false, true}) {
    Rig rig;
    rig.paused = pause; rig.override_active = !pause;
    rig.click(); rig.click(); rig.click(); rig.finish();
    expect(rig.actions.empty(), "Short clicks cannot enter another mode from Pause/Override");
    rig.hold(); rig.finish();
    expect(rig.only(Action::RESUME), "Hold exits Pause/Override exactly once");
  }
  Rig short_hold;
  short_hold.click(2999); short_hold.finish();
  expect(short_hold.only(Action::OVERRIDE), "Sub-three-second hold remains a short press");
}

void test_programming() {
  for (uint8_t desired = 1; desired <= 8; ++desired) {
    Rig rig;
    rig.hold();
    expect(rig.button.is_programming(), "Programming mode entered locally");
    for (uint8_t i = 0; i < desired; ++i) rig.click(90, 80);
    rig.hold();
    expect(rig.actions.size() == 2 && rig.actions.back() == Action::SAVE_PROFILE,
           "Programming taps are not normal single/double/triple gestures");
    expect(rig.profile == desired && rig.button.is_reporting(), "Saved profile is reported");
    rig.drive(false, 15000);
    expect(!rig.button.is_reporting() && !rig.button.is_programming(), "Report automatically returns to operation");
  }
  Rig timeout;
  timeout.profile = 4;
  timeout.hold(); timeout.click(); timeout.drive(false, 45000);
  expect(timeout.actions.back() == Action::CANCEL_SET && timeout.profile == 4,
         "45-second inactivity restores original profile");
  Rig invalid;
  invalid.profile = 6;
  invalid.hold();
  for (int i = 0; i < 9; ++i) invalid.click();
  invalid.hold();
  expect(invalid.actions.back() == Action::INVALID && invalid.profile == 6,
         "More than eight programming taps cannot save a wrong profile");
}

void test_bounce_boot_and_wrap() {
  Rig bounce;
  for (int i = 0; i < 10; ++i) { bounce.drive(true, 5); bounce.drive(false, 5); }
  bounce.finish();
  expect(bounce.actions.empty(), "Contact bounce produces no action");
  bounce.drive(true, 80);
  for (int i = 0; i < 5; ++i) { bounce.drive(false, 5); bounce.drive(true, 5); }
  bounce.drive(false, 700);
  expect(bounce.only(Action::OVERRIDE), "Release bounce does not add clicks");

  Rig held(0, true);
  held.drive(true, 6000); held.drive(false, 700);
  expect(held.actions.empty(), "Button held across boot does not generate a command");
  held.click(); held.finish();
  expect(held.only(Action::OVERRIDE), "Normal input works after boot-held button released");

  Rig wrap(std::numeric_limits<uint32_t>::max() - 1500U);
  wrap.hold();
  expect(wrap.only(Action::ENTER_SET), "Hold recognition survives millis wrap");
}

void test_manual_led_timing() {
  Button button;
  expect(button.led_on(0, false, false), "Normal operation indicator stays on");
  expect(button.led_on(124, true, false) && !button.led_on(125, true, false), "Pause 125ms on");
  expect(!button.led_on(2624, true, false) && button.led_on(2625, true, false), "Pause 2500ms off");
  expect(button.led_on(2499, false, true) && !button.led_on(2500, false, true), "Override 2500ms on");
  expect(!button.led_on(2624, false, true) && button.led_on(2625, false, true), "Override 125ms off");

  Rig report;
  report.profile = 3;
  report.click(); report.click(); report.click();
  while (report.actions.empty()) report.drive(false, 1);
  const uint32_t start = report.now;
  expect(report.button.led_on(start + 2999, false, false), "Check starts with three-second light");
  expect(!report.button.led_on(start + 3000, false, false), "Check separator off");
  for (uint32_t i = 0; i < 3; ++i) {
    expect(report.button.led_on(start + 3500 + i * 1000, false, false), "Check flash on");
    expect(!report.button.led_on(start + 4000 + i * 1000, false, false), "Check flash off");
  }
  expect(!report.button.led_on(start + 8999, false, false), "Check final three-second dark interval");
}
}  // namespace

int main() {
  test_clicks(); test_hold_and_context(); test_programming();
  test_bounce_boot_and_wrap(); test_manual_led_timing();
  if (failures) return EXIT_FAILURE;
  std::cout << "PASS: local EVO button modes, programming, debounce, hold release, and LED timing\n";
  return EXIT_SUCCESS;
}
