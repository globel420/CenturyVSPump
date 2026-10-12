#pragma once

#include <cstdint>

namespace vgreen_evo {
namespace manual {

enum class Action : uint8_t {
  NONE, OVERRIDE, PAUSE, RESUME, CHECK, ENTER_SET, SAVE_PROFILE, CANCEL_SET, INVALID
};

// Pure local input/UI logic. It has no ESPHome, network, or motor dependency.
class Button {
 public:
  static constexpr uint32_t DEBOUNCE_MS = 30;
  static constexpr uint32_t CLICK_GAP_MS = 500;
  static constexpr uint32_t HOLD_MS = 3000;
  static constexpr uint32_t SET_TIMEOUT_MS = 45000;

  Action update(bool pressed, uint32_t now, bool paused, bool override_active,
                uint8_t profile) {
    const Edge edge = read_input_(pressed, now);
    if ((paused || override_active) && overlay_ != Overlay::NORMAL)
      cancel();

    if (overlay_ == Overlay::CHECK) {
      if (elapsed_(now, overlay_started_) >= report_duration_()) {
        overlay_ = Overlay::NORMAL;
        ignore_until_release_ = stable_pressed_ || raw_pressed_;
      }
      return Action::NONE;
    }

    if (ignore_until_release_) {
      if (!stable_pressed_ && !raw_pressed_) ignore_until_release_ = false;
      return Action::NONE;
    }

    if (overlay_ == Overlay::SET) {
      if (edge == Edge::PRESS || edge == Edge::SHORT || edge == Edge::HOLD)
        last_activity_ = now;
      if (elapsed_(now, last_activity_) >= SET_TIMEOUT_MS) {
        cancel();
        ignore_until_release_ = stable_pressed_ || raw_pressed_;
        return Action::CANCEL_SET;
      }
      if (edge == Edge::SHORT) {
        if (program_presses_ < 9) ++program_presses_;
      } else if (edge == Edge::HOLD) {
        if (program_presses_ >= 1 && program_presses_ <= 8) {
          selected_profile_ = program_presses_;
          begin_report_(now, selected_profile_);
          return Action::SAVE_PROFILE;
        }
        program_presses_ = 0;
        return Action::INVALID;
      }
      return Action::NONE;
    }

    if (edge == Edge::HOLD) {
      pending_clicks_ = 0;
      if (paused || override_active) return Action::RESUME;
      overlay_ = Overlay::SET;
      overlay_started_ = last_activity_ = now;
      program_presses_ = 0;
      return Action::ENTER_SET;
    }

    // Manual sections 4.0-4.4: mode entry is from Operating Mode only.
    if (paused || override_active) {
      pending_clicks_ = 0;
      return Action::NONE;
    }
    if (edge == Edge::SHORT) {
      if (pending_clicks_ < 4) ++pending_clicks_;
      last_release_ = now;
    }
    if (pending_clicks_ != 0 && !stable_pressed_ && !raw_pressed_ &&
        elapsed_(now, last_release_) >= CLICK_GAP_MS) {
      const uint8_t count = pending_clicks_;
      pending_clicks_ = 0;
      if (count == 1) return Action::OVERRIDE;
      if (count == 2) return Action::PAUSE;
      if (count == 3) {
        begin_report_(now, profile);
        return Action::CHECK;
      }
      return Action::INVALID;
    }
    return Action::NONE;
  }

  bool is_programming() const { return overlay_ == Overlay::SET; }
  bool is_reporting() const { return overlay_ == Overlay::CHECK; }
  bool is_pressed() const { return stable_pressed_; }
  uint8_t selected_profile() const { return selected_profile_; }
  uint8_t programming_count() const { return program_presses_; }

  void cancel() {
    overlay_ = Overlay::NORMAL;
    pending_clicks_ = program_presses_ = 0;
  }

  bool led_on(uint32_t now, bool paused, bool override_active) const {
    if (overlay_ == Overlay::SET)
      return elapsed_(now, overlay_started_) % 500U < 250U;
    if (overlay_ == Overlay::CHECK) {
      const uint32_t elapsed = elapsed_(now, overlay_started_);
      if (elapsed < 3000U) return true;
      if (elapsed < 3500U) return false;
      const uint32_t flashes = elapsed - 3500U;
      return flashes < static_cast<uint32_t>(report_profile_) * 1000U &&
             flashes % 1000U < 500U;
    }
    if (paused) return now % 2625U < 125U;
    if (override_active) return now % 2625U < 2500U;
    return true;
  }

 private:
  enum class Edge : uint8_t { NONE, PRESS, SHORT, HOLD };
  enum class Overlay : uint8_t { NORMAL, SET, CHECK };
  static uint32_t elapsed_(uint32_t now, uint32_t before) { return now - before; }

  Edge read_input_(bool pressed, uint32_t now) {
    if (!initialized_) {
      initialized_ = true;
      raw_pressed_ = stable_pressed_ = pressed;
      raw_changed_ = now;
      armed_ = !pressed;  // A button held during boot must first be released.
      return Edge::NONE;
    }
    if (pressed != raw_pressed_) {
      raw_pressed_ = pressed;
      raw_changed_ = now;
    }
    if (raw_pressed_ == stable_pressed_ || elapsed_(now, raw_changed_) < DEBOUNCE_MS)
      return Edge::NONE;
    stable_pressed_ = raw_pressed_;
    if (!armed_) {
      if (!stable_pressed_) armed_ = true;
      return Edge::NONE;
    }
    if (stable_pressed_) {
      pressed_at_ = raw_changed_;
      return Edge::PRESS;
    }
    return elapsed_(raw_changed_, pressed_at_) >= HOLD_MS ? Edge::HOLD : Edge::SHORT;
  }

  void begin_report_(uint32_t now, uint8_t profile) {
    overlay_ = Overlay::CHECK;
    overlay_started_ = now;
    report_profile_ = profile;
    pending_clicks_ = 0;
  }
  uint32_t report_duration_() const { return 6000U + report_profile_ * 1000U; }

  Overlay overlay_{Overlay::NORMAL};
  bool initialized_{false};
  bool armed_{false};
  bool raw_pressed_{false};
  bool stable_pressed_{false};
  bool ignore_until_release_{false};
  uint32_t raw_changed_{0};
  uint32_t pressed_at_{0};
  uint32_t last_release_{0};
  uint32_t last_activity_{0};
  uint32_t overlay_started_{0};
  uint8_t pending_clicks_{0};
  uint8_t program_presses_{0};
  uint8_t selected_profile_{1};
  uint8_t report_profile_{1};
};

}  // namespace manual
}  // namespace vgreen_evo
