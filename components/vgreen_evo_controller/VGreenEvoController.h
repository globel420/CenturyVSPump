#pragma once

#include "esphome/components/centuryvspump/CenturyVSPump.h"
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include "manual_button.h"
#include "stock_schedule.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace esphome
{
    namespace vgreen_evo_controller
    {
        namespace stock = ::vgreen_evo::stock;

        enum class OperatingMode : uint8_t
        {
            STARTUP_SAFE_STOP,
            WAITING_FOR_COMMUNICATION,
            WAITING_FOR_STATUS,
            COMMUNICATION_FAULT,
            MOTOR_FAULT,
            MAINTENANCE_PAUSE,
            FREEZE_PROTECTION,
            OVERRIDE,
            PRIMING,
            SCHEDULE_RUNNING,
            SCHEDULE_STOPPED,
            SCHEDULE_SET,
        };

        /**
         * Autonomous, boot-relative controller for VGreen EVO pumps.
         *
         * This component deliberately has no dependency on Wi-Fi, the native API,
         * Home Assistant, wall-clock time, or flash-backed state.  The only inputs
         * needed to keep the pump operating are millis(), pump communication, and
         * the optional motor ambient-temperature sensor.
         *
         * CenturyVSPump transport contract:
         *   bool communication_is_healthy(uint32_t max_age_ms) const;
         *   bool has_recent_no_fault_status(uint32_t max_age_ms) const;
         *   bool motor_fault_active() const;
         *   bool request_run(uint16_t rpm, uint32_t *generation);
         *   bool request_stop(uint32_t *generation);
         *   bool motion_request_is_confirmed(uint32_t generation, uint16_t rpm) const;
         *   bool motion_request_has_failed(uint32_t generation) const;
         */
        class VGreenEvoController : public Component
        {
        public:
            void setup() override;
            void loop() override;
            void dump_config() override;
            float get_setup_priority() const override { return setup_priority::LATE; }

            void set_pump(century_vs_pump::CenturyVSPump *pump) { this->pump_ = pump; }
            void set_run_switch(switch_::Switch *run_switch) { this->run_switch_ = run_switch; }
            void set_demand_number(number::Number *demand_number) { this->demand_number_ = demand_number; }
            void set_button_pin(GPIOPin *pin) { this->button_pin_ = pin; }
            void set_profile_save_callback(std::function<void(uint8_t)> callback)
            { this->profile_save_callback_ = std::move(callback); }
            void set_ambient_temperature_sensor(sensor::Sensor *ambient_temperature)
            {
                this->ambient_temperature_ = ambient_temperature;
            }

            // Runtime configuration.  These setters intentionally do not write flash.
            void set_schedule_set(stock::ScheduleSet schedule_set);
            void set_schedule_set(uint8_t schedule_set);
            void set_schedule_set(const std::string &schedule_set);
            void set_schedule_profile(uint8_t profile);
            void set_freeze_enabled(bool enabled);

            // Stock-like local actions for the ATOM button and optional UI entities.
            void start_override();
            void set_paused(bool paused);
            void pause() { this->set_paused(true); }
            void resume_schedule();
            void restart_schedule();
            bool set_schedule_phase(int seconds);

            // Read-only state for template sensors, text sensors, and diagnostics.
            std::string get_mode_name() const;
            OperatingMode get_mode() const { return this->mode_; }
            stock::ScheduleSet get_schedule_set() const { return this->schedule_set_; }
            uint8_t get_schedule_profile() const { return this->schedule_profile_; }
            uint8_t get_schedule_hour() const;
            uint32_t get_schedule_elapsed_minutes() const;
            uint16_t get_target_rpm() const { return this->target_rpm_; }
            bool is_paused() const { return this->paused_; }
            bool is_override_active() const;
            bool is_freeze_active() const { return this->freeze_window_active_; }
            bool is_freeze_available() const;
            bool is_communication_healthy() const;
            bool is_motor_fault_active() const;
            float get_ambient_temperature_f() const;
            bool is_button_pressed() const { return this->manual_button_.is_pressed(); }
            bool is_selecting_schedule() const { return this->manual_button_.is_programming(); }
            const std::string &get_last_button_action() const { return this->last_button_action_; }
            uint32_t get_indicator_rgb() const;
            uint32_t get_schedule_elapsed_seconds() const;

        protected:
            static constexpr uint32_t DECISION_INTERVAL_MS = 250;
            static constexpr uint32_t COMMAND_HEARTBEAT_MS = 20000;
            static constexpr uint32_t COMMUNICATION_STALE_MS = 30000;
            static constexpr uint32_t COMMUNICATION_SETTLE_MS = 1000;
            static constexpr uint32_t AMBIENT_STALE_MS = 120000;
            static constexpr uint32_t PRIME_DURATION_MS = 180000;
            static constexpr uint32_t OVERRIDE_DURATION_MS = 24UL * 60UL * 60UL * 1000UL;
            static constexpr uint32_t FREEZE_WINDOW_MS = 60UL * 60UL * 1000UL;
            static constexpr float FREEZE_THRESHOLD_F = 39.0f;
            static constexpr float MIN_PLAUSIBLE_AMBIENT_F = -40.0f;
            static constexpr float MAX_PLAUSIBLE_AMBIENT_F = 160.0f;
            static constexpr uint16_t OVERRIDE_RPM = 3450;
            static constexpr uint16_t PRIME_RPM = 3450;
            static constexpr uint16_t FREEZE_RPM = 1725;

            void advance_schedule_clock_(uint32_t now);
            void capture_ambient_temperature_(float value);
            bool ambient_temperature_is_fresh_(uint32_t now) const;
            void update_freeze_window_(uint32_t now);
            void set_mode_(OperatingMode mode);
            void apply_target_(uint16_t rpm, uint32_t now, bool force = false);
            bool request_stop_(uint32_t now, bool force = false);
            void cancel_prime_();
            void restart_schedule_phase_(uint32_t now);
            bool deadline_reached_(uint32_t now, uint32_t deadline) const;
            void handle_button_(uint32_t now);

            century_vs_pump::CenturyVSPump *pump_{nullptr};
            switch_::Switch *run_switch_{nullptr};
            number::Number *demand_number_{nullptr};
            sensor::Sensor *ambient_temperature_{nullptr};
            GPIOPin *button_pin_{nullptr};
            ::vgreen_evo::manual::Button manual_button_;
            std::function<void(uint8_t)> profile_save_callback_;
            std::string last_button_action_{"none"};

            stock::ScheduleSet schedule_set_{stock::ScheduleSet::A};
            uint8_t schedule_profile_{1};
            OperatingMode mode_{OperatingMode::STARTUP_SAFE_STOP};

            uint64_t schedule_elapsed_ms_{0};
            uint32_t last_clock_ms_{0};
            uint32_t last_decision_ms_{0};
            uint32_t last_command_ms_{0};
            uint32_t communication_ready_ms_{0};
            uint32_t override_deadline_ms_{0};
            uint32_t prime_deadline_ms_{0};
            uint32_t freeze_window_deadline_ms_{0};
            uint32_t ambient_update_ms_{0};
            uint32_t pending_motion_generation_{0};

            uint16_t target_rpm_{0};
            uint16_t pending_target_rpm_{0};
            float ambient_temperature_f_{0.0f};

            bool paused_{false};
            bool freeze_enabled_{false};
            bool override_active_{false};
            bool prime_active_{false};
            bool prime_timing_started_{false};
            bool freeze_window_active_{false};
            bool ambient_valid_{false};
            bool communication_ready_{false};
            bool ever_communicated_{false};
            bool motor_fault_latched_{false};
            bool force_target_update_{true};
        };
    }
}
