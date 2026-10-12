#include "VGreenEvoController.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cmath>
#include <limits>

namespace esphome
{
    namespace vgreen_evo_controller
    {
        static const char *const TAG = "vgreen_evo.controller";

        void VGreenEvoController::setup()
        {
            const uint32_t now = millis();
            this->last_clock_ms_ = now;
            this->last_decision_ms_ = now;
            if (this->button_pin_ != nullptr)
            {
                this->button_pin_->setup();
                this->manual_button_.update(this->button_pin_->digital_read(), now, false, false,
                                            this->schedule_profile_);
            }

            if (this->ambient_temperature_ != nullptr)
            {
                this->ambient_temperature_->add_on_state_callback(
                    [this](float value) { this->capture_ambient_temperature_(value); });
                if (this->ambient_temperature_->has_state())
                    this->capture_ambient_temperature_(this->ambient_temperature_->state);
            }

            // A stop is always the first control request after controller boot.  No
            // autonomous start is allowed until the transport proves it can reply.
            this->set_mode_(OperatingMode::STARTUP_SAFE_STOP);
            this->request_stop_(now, true);
        }

        void VGreenEvoController::loop()
        {
            const uint32_t now = millis();
            this->advance_schedule_clock_(now);
            this->handle_button_(now);

            if ((uint32_t) (now - this->last_decision_ms_) < DECISION_INTERVAL_MS)
                return;
            this->last_decision_ms_ = now;

            // Programming is performed entirely on the M5, with the motor stopped
            // as shown on page 14 of the EVO manual. The schedule clock keeps moving.
            if (this->manual_button_.is_programming())
            {
                this->communication_ready_ = false;
                this->cancel_prime_();
                this->set_mode_(OperatingMode::SCHEDULE_SET);
                this->request_stop_(now, this->force_target_update_);
                return;
            }

            // Maintenance pause has absolute priority and suppresses every automatic
            // start, including freeze protection.
            if (this->paused_)
            {
                this->communication_ready_ = false;
                this->cancel_prime_();
                this->freeze_window_active_ = false;
                this->set_mode_(OperatingMode::MAINTENANCE_PAUSE);
                this->request_stop_(now, this->force_target_update_);
                return;
            }

            const bool communication_healthy = this->is_communication_healthy();
            if (!communication_healthy)
            {
                this->communication_ready_ = false;
                this->cancel_prime_();
                this->freeze_window_active_ = false;
                this->set_mode_(this->ever_communicated_
                                    ? OperatingMode::COMMUNICATION_FAULT
                                    : OperatingMode::WAITING_FOR_COMMUNICATION);
                // Best effort only: loss of the RS-485 response path means delivery
                // cannot be confirmed, so keep reasserting Stop at the heartbeat.
                this->request_stop_(now, this->force_target_update_);
                return;
            }

            this->ever_communicated_ = true;
            if (this->pump_ != nullptr && this->pump_->motor_fault_active())
                this->motor_fault_latched_ = true;
            else if (this->motor_fault_latched_ && this->pump_ != nullptr &&
                     this->pump_->has_recent_no_fault_status(COMMUNICATION_STALE_MS))
            {
                // The motor owns fault clearing (manual section 8). A fault that
                // it has cleared must not need an extra M5 reset to run again.
                this->motor_fault_latched_ = false;
                ESP_LOGI(TAG, "Motor reports a healthy status again; resynchronizing");
            }

            if (this->motor_fault_latched_)
            {
                this->communication_ready_ = false;
                this->cancel_prime_();
                this->freeze_window_active_ = false;
                this->set_mode_(OperatingMode::MOTOR_FAULT);
                this->request_stop_(now, this->force_target_update_);
                return;
            }

            // A valid ACK proves only that something answered. Do not authorize
            // motion until the motor has also returned a recent, recognized
            // status that explicitly carries no fault indication.
            if (this->pump_ == nullptr ||
                !this->pump_->has_recent_no_fault_status(COMMUNICATION_STALE_MS))
            {
                this->communication_ready_ = false;
                this->cancel_prime_();
                this->freeze_window_active_ = false;
                this->set_mode_(OperatingMode::WAITING_FOR_STATUS);
                this->request_stop_(now, this->force_target_update_);
                return;
            }

            if (!this->communication_ready_)
            {
                this->cancel_prime_();
                this->set_mode_(OperatingMode::STARTUP_SAFE_STOP);
                if (!this->request_stop_(now, true))
                    return;
                this->communication_ready_ = true;
                this->communication_ready_ms_ = now;
                return;
            }

            if ((uint32_t) (now - this->communication_ready_ms_) < COMMUNICATION_SETTLE_MS)
            {
                this->set_mode_(OperatingMode::STARTUP_SAFE_STOP);
                this->request_stop_(now, this->force_target_update_);
                return;
            }

            // Freeze windows are evaluated only after all stop/fault gates have
            // passed. Resuming maintenance therefore starts a fresh one-hour
            // window if the current local temperature is still low.
            this->update_freeze_window_(now);
            if (this->freeze_window_active_)
            {
                this->cancel_prime_();
                this->set_mode_(OperatingMode::FREEZE_PROTECTION);
                this->apply_target_(FREEZE_RPM, now, this->force_target_update_);
                return;
            }

            if (this->override_active_ && this->deadline_reached_(now, this->override_deadline_ms_))
            {
                this->override_active_ = false;
                this->force_target_update_ = true;
                ESP_LOGI(TAG, "24-hour override completed; returning to schedule");
            }

            if (this->override_active_)
            {
                this->cancel_prime_();
                this->set_mode_(OperatingMode::OVERRIDE);
                this->apply_target_(OVERRIDE_RPM, now, this->force_target_update_);
                return;
            }

            const stock::ScheduleState scheduled = stock::evaluate(
                this->schedule_set_, this->schedule_profile_, this->schedule_elapsed_ms_ / 1000ULL);

            if (!scheduled.valid)
            {
                ESP_LOGE(TAG, "Invalid stock schedule selection; stopping pump");
                this->cancel_prime_();
                this->set_mode_(OperatingMode::SCHEDULE_STOPPED);
                this->request_stop_(now, this->force_target_update_);
                return;
            }

            if (this->prime_active_ && this->prime_timing_started_ &&
                this->deadline_reached_(now, this->prime_deadline_ms_))
            {
                this->cancel_prime_();
                this->force_target_update_ = true;
                ESP_LOGI(TAG, "Priming complete; applying scheduled speed");
            }

            // A phase correction into a scheduled-off period must stop immediately,
            // including when it arrives during a startup prime.
            if (scheduled.rpm == 0)
                this->cancel_prime_();

            // Prime only when a priming-enabled schedule is about to transition the
            // controller from a stopped command to a running command.  Higher-priority
            // running modes (freeze/override) therefore do not cause a redundant prime.
            if (!this->prime_active_ && scheduled.rpm > 0 && this->target_rpm_ == 0 &&
                scheduled.prime_on_start)
            {
                this->prime_active_ = true;
                this->prime_timing_started_ = false;
                this->force_target_update_ = true;
                ESP_LOGI(TAG, "Requesting 3-minute priming cycle");
            }

            if (this->prime_active_)
            {
                this->set_mode_(OperatingMode::PRIMING);
                this->apply_target_(PRIME_RPM, now, this->force_target_update_);
            }
            else if (scheduled.rpm > 0)
            {
                this->set_mode_(OperatingMode::SCHEDULE_RUNNING);
                this->apply_target_(scheduled.rpm, now, this->force_target_update_);
            }
            else
            {
                this->set_mode_(OperatingMode::SCHEDULE_STOPPED);
                this->request_stop_(now, this->force_target_update_);
            }
        }

        void VGreenEvoController::dump_config()
        {
            ESP_LOGCONFIG(TAG, "VGreen EVO autonomous controller:");
            ESP_LOGCONFIG(TAG, "  Schedule: %c%u", this->schedule_set_ == stock::ScheduleSet::B ? 'B' : 'A',
                          (unsigned) this->schedule_profile_);
            ESP_LOGCONFIG(TAG, "  Schedule clock: boot-relative, no wall clock required");
            ESP_LOGCONFIG(TAG, "  Command heartbeat: %lu seconds",
                          (unsigned long) (COMMAND_HEARTBEAT_MS / 1000UL));
            ESP_LOGCONFIG(TAG, "  Priming: 3450 RPM for %lu seconds when enabled by profile",
                          (unsigned long) (PRIME_DURATION_MS / 1000UL));
            ESP_LOGCONFIG(TAG, "  Freeze protection: %s", this->freeze_enabled_ ? "enabled" : "disabled");
            ESP_LOGCONFIG(TAG, "  Freeze sensor: %s", this->ambient_temperature_ != nullptr
                                                          ? "configured (expected degrees F)"
                                                          : "not configured; freeze unavailable");
        }

        void VGreenEvoController::set_schedule_set(stock::ScheduleSet schedule_set)
        {
            if (!stock::is_valid_schedule_set(schedule_set))
            {
                ESP_LOGW(TAG, "Ignoring invalid schedule set");
                return;
            }
            if (this->schedule_set_ == schedule_set)
                return;
            this->schedule_set_ = schedule_set;
            this->restart_schedule_phase_(millis());
        }

        void VGreenEvoController::set_schedule_set(uint8_t schedule_set)
        {
            if (schedule_set > 1U)
            {
                ESP_LOGW(TAG, "Ignoring invalid schedule set %u (use 0 for A or 1 for B)",
                         (unsigned) schedule_set);
                return;
            }
            this->set_schedule_set(schedule_set == 1 ? stock::ScheduleSet::B : stock::ScheduleSet::A);
        }

        void VGreenEvoController::set_schedule_set(const std::string &schedule_set)
        {
            if (schedule_set == "A" || schedule_set == "a")
                this->set_schedule_set(stock::ScheduleSet::A);
            else if (schedule_set == "B" || schedule_set == "b")
                this->set_schedule_set(stock::ScheduleSet::B);
            else
                ESP_LOGW(TAG, "Ignoring invalid schedule set '%s' (use A or B)", schedule_set.c_str());
        }

        void VGreenEvoController::set_schedule_profile(uint8_t profile)
        {
            if (!stock::is_valid_profile_number(profile))
            {
                ESP_LOGW(TAG, "Ignoring invalid schedule profile %u (valid range is 1-8)",
                         (unsigned) profile);
                return;
            }
            if (this->schedule_profile_ == profile)
                return;
            this->schedule_profile_ = profile;
            this->force_target_update_ = true;
        }

        void VGreenEvoController::set_freeze_enabled(bool enabled)
        {
            if (this->freeze_enabled_ == enabled)
                return;
            this->freeze_enabled_ = enabled;
            if (!enabled)
                this->freeze_window_active_ = false;
            this->force_target_update_ = true;
        }

        void VGreenEvoController::start_override()
        {
            const uint32_t now = millis();
            this->advance_schedule_clock_(now);
            this->override_active_ = true;
            this->override_deadline_ms_ = now + OVERRIDE_DURATION_MS;
            this->cancel_prime_();
            this->force_target_update_ = true;
            ESP_LOGI(TAG, "Starting 24-hour 3450 RPM override");
        }

        void VGreenEvoController::set_paused(bool paused)
        {
            const uint32_t now = millis();
            this->advance_schedule_clock_(now);
            if (this->paused_ == paused)
                return;
            this->paused_ = paused;
            this->cancel_prime_();
            if (paused)
                this->freeze_window_active_ = false;
            this->communication_ready_ = false;
            this->force_target_update_ = true;
            if (paused)
                this->override_active_ = false;
            ESP_LOGI(TAG, "Maintenance pause %s", paused ? "enabled" : "released");
        }

        void VGreenEvoController::resume_schedule()
        {
            const uint32_t now = millis();
            this->advance_schedule_clock_(now);
            const bool was_paused = this->paused_;
            const bool was_override = this->override_active_;
            bool cleared_motor_fault = false;

            if (this->pump_ != nullptr && this->pump_->motor_fault_active())
                this->motor_fault_latched_ = true;

            if (this->motor_fault_latched_)
            {
                if (!this->is_communication_healthy() || this->pump_ == nullptr ||
                    !this->pump_->has_recent_no_fault_status(COMMUNICATION_STALE_MS))
                {
                    this->force_target_update_ = true;
                    ESP_LOGW(TAG, "Motor fault is still active or cannot be verified; remaining stopped");
                    return;
                }
                this->motor_fault_latched_ = false;
                cleared_motor_fault = true;
                ESP_LOGI(TAG, "Cleared motor-fault latch after healthy no-fault status");
            }

            // Leaving a healthy override is a direct return to the current
            // schedule position. Keep communication synchronized and retain the
            // previous running target so the schedule does not falsely see a
            // stopped-to-running transition and start a new prime.
            if (was_override && !was_paused && !cleared_motor_fault)
            {
                this->override_active_ = false;
                this->cancel_prime_();
                this->force_target_update_ = true;
                ESP_LOGI(TAG, "Override released; returning to current schedule phase");
                return;
            }

            if (!was_paused && !was_override && !cleared_motor_fault)
            {
                ESP_LOGD(TAG, "Resume requested with no pause, override, or cleared fault");
                return;
            }

            this->paused_ = false;
            this->override_active_ = false;
            this->cancel_prime_();
            this->communication_ready_ = false;
            this->force_target_update_ = true;
            ESP_LOGI(TAG, "Resuming current schedule phase");
        }

        void VGreenEvoController::restart_schedule()
        {
            this->restart_schedule_phase_(millis());
            ESP_LOGI(TAG, "Schedule phase reset to hour one");
        }

        bool VGreenEvoController::set_schedule_phase(int seconds)
        {
            if (seconds < 0 || seconds >= static_cast<int>(stock::kSecondsPerDay))
            {
                ESP_LOGW(TAG, "Ignoring invalid schedule phase %d", seconds);
                return false;
            }
            this->advance_schedule_clock_(millis());
            this->schedule_elapsed_ms_ = static_cast<uint64_t>(seconds) * 1000ULL;
            // Re-evaluate normally: no resume, fault clearing, or priming restart.
            this->force_target_update_ = true;
            ESP_LOGI(TAG, "Schedule aligned to hour %u, minute %u", (unsigned) this->get_schedule_hour(),
                     (unsigned) ((seconds / 60) % 60));
            return true;
        }

        void VGreenEvoController::handle_button_(uint32_t now)
        {
            if (this->button_pin_ == nullptr)
                return;
            using ::vgreen_evo::manual::Action;
            const Action action = this->manual_button_.update(
                this->button_pin_->digital_read(), now, this->paused_, this->is_override_active(),
                this->schedule_profile_);
            switch (action)
            {
                case Action::NONE: return;
                case Action::OVERRIDE:
                    this->last_button_action_ = "single press: override";
                    this->start_override();
                    break;
                case Action::PAUSE:
                    this->last_button_action_ = "double press: pause";
                    this->pause();
                    break;
                case Action::RESUME:
                    this->last_button_action_ = "hold and release: resume";
                    this->resume_schedule();
                    break;
                case Action::CHECK:
                    this->last_button_action_ = "triple press: schedule check";
                    break;
                case Action::ENTER_SET:
                    this->last_button_action_ = "hold and release: schedule set";
                    this->force_target_update_ = true;
                    break;
                case Action::SAVE_PROFILE:
                    this->last_button_action_ = "saved profile " + std::to_string(this->manual_button_.selected_profile());
                    this->set_schedule_profile(this->manual_button_.selected_profile());
                    if (this->profile_save_callback_)
                        this->profile_save_callback_(this->schedule_profile_);
                    this->force_target_update_ = true;
                    break;
                case Action::CANCEL_SET:
                    this->last_button_action_ = "schedule set timed out: unchanged";
                    this->force_target_update_ = true;
                    break;
                case Action::INVALID:
                    this->last_button_action_ = "invalid press count: unchanged";
                    break;
            }
            ESP_LOGI(TAG, "M5 button: %s", this->last_button_action_.c_str());
        }

        uint32_t VGreenEvoController::get_indicator_rgb() const
        {
            if (!this->manual_button_.led_on(millis(), this->paused_, this->is_override_active()))
                return 0;
            // Retain diagnostic colors without hiding the manual's blink timing.
            if (this->is_motor_fault_active()) return 0xFF0000;
            if (!this->is_communication_healthy()) return 0xFF5900;
            return 0x00FF00;
        }

        uint32_t VGreenEvoController::get_schedule_elapsed_seconds() const
        {
            return static_cast<uint32_t>((this->schedule_elapsed_ms_ / 1000ULL) % stock::kSecondsPerDay);
        }

        std::string VGreenEvoController::get_mode_name() const
        {
            switch (this->mode_)
            {
                case OperatingMode::STARTUP_SAFE_STOP:
                    return "startup safe stop";
                case OperatingMode::WAITING_FOR_COMMUNICATION:
                    return "waiting for pump";
                case OperatingMode::WAITING_FOR_STATUS:
                    return "waiting for verified status";
                case OperatingMode::COMMUNICATION_FAULT:
                    return "communication fault";
                case OperatingMode::MOTOR_FAULT:
                    return "motor fault";
                case OperatingMode::MAINTENANCE_PAUSE:
                    return "maintenance pause";
                case OperatingMode::FREEZE_PROTECTION:
                    return "freeze protection";
                case OperatingMode::OVERRIDE:
                    return "24-hour override";
                case OperatingMode::PRIMING:
                    return "priming";
                case OperatingMode::SCHEDULE_RUNNING:
                    return "schedule running";
                case OperatingMode::SCHEDULE_STOPPED:
                    return "schedule stopped";
                case OperatingMode::SCHEDULE_SET:
                    return "schedule set";
            }
            return "unknown";
        }

        uint8_t VGreenEvoController::get_schedule_hour() const
        {
            const stock::ScheduleState scheduled = stock::evaluate(
                this->schedule_set_, this->schedule_profile_, this->schedule_elapsed_ms_ / 1000ULL);
            return scheduled.valid ? (uint8_t) (scheduled.hour + 1U) : 0;
        }

        uint32_t VGreenEvoController::get_schedule_elapsed_minutes() const
        {
            static const uint64_t MINUTES_PER_DAY = 24ULL * 60ULL;
            return (uint32_t) ((this->schedule_elapsed_ms_ / 60000ULL) % MINUTES_PER_DAY);
        }

        bool VGreenEvoController::is_override_active() const
        {
            return this->override_active_ && !this->deadline_reached_(millis(), this->override_deadline_ms_);
        }

        bool VGreenEvoController::is_freeze_available() const
        {
            return this->freeze_enabled_ && this->ambient_temperature_is_fresh_(millis());
        }

        bool VGreenEvoController::is_communication_healthy() const
        {
            return this->pump_ != nullptr &&
                   this->pump_->communication_is_healthy(COMMUNICATION_STALE_MS);
        }

        bool VGreenEvoController::is_motor_fault_active() const
        {
            return this->motor_fault_latched_ ||
                   (this->pump_ != nullptr && this->pump_->motor_fault_active());
        }

        float VGreenEvoController::get_ambient_temperature_f() const
        {
            if (!this->ambient_temperature_is_fresh_(millis()))
                return std::numeric_limits<float>::quiet_NaN();
            return this->ambient_temperature_f_;
        }

        void VGreenEvoController::advance_schedule_clock_(uint32_t now)
        {
            const uint32_t elapsed = now - this->last_clock_ms_;
            this->last_clock_ms_ = now;
            // Stock Pause stops the motor without disrupting the 24-hour
            // schedule, so the boot-relative phase keeps advancing.
            this->schedule_elapsed_ms_ += elapsed;
        }

        void VGreenEvoController::capture_ambient_temperature_(float value)
        {
            if (!std::isfinite(value) || value < MIN_PLAUSIBLE_AMBIENT_F ||
                value > MAX_PLAUSIBLE_AMBIENT_F)
            {
                this->ambient_valid_ = false;
                ESP_LOGW(TAG, "Ambient temperature is invalid or implausible; freeze unavailable");
                return;
            }

            this->ambient_temperature_f_ = value;
            this->ambient_update_ms_ = millis();
            this->ambient_valid_ = true;
        }

        bool VGreenEvoController::ambient_temperature_is_fresh_(uint32_t now) const
        {
            return this->ambient_temperature_ != nullptr && this->ambient_valid_ &&
                   (uint32_t) (now - this->ambient_update_ms_) <= AMBIENT_STALE_MS;
        }

        void VGreenEvoController::update_freeze_window_(uint32_t now)
        {
            if (!this->freeze_enabled_)
            {
                this->freeze_window_active_ = false;
                return;
            }

            if (this->freeze_window_active_)
            {
                if (!this->deadline_reached_(now, this->freeze_window_deadline_ms_))
                    return;

                // The stock behavior runs in one-hour windows and rechecks at the
                // boundary. Once a cold window has been authorized, stale input
                // extends protection conservatively until a fresh reading proves
                // the temperature has risen above the threshold.
                const bool ambient_fresh = this->ambient_temperature_is_fresh_(now);
                if (!ambient_fresh)
                {
                    this->freeze_window_active_ = true;
                    this->freeze_window_deadline_ms_ = now + FREEZE_WINDOW_MS;
                    ESP_LOGW(TAG, "Freeze sensor stale at hourly recheck; extending protection for one hour");
                }
                else if (this->ambient_temperature_f_ <= FREEZE_THRESHOLD_F)
                {
                    this->freeze_window_active_ = true;
                    this->freeze_window_deadline_ms_ = now + FREEZE_WINDOW_MS;
                    ESP_LOGI(TAG, "Freeze temperature remains low; extending for one hour");
                }
                else
                {
                    this->freeze_window_active_ = false;
                    ESP_LOGI(TAG, "Freeze temperature is above threshold; returning to normal control");
                }
                this->force_target_update_ = true;
                return;
            }

            if (this->ambient_temperature_is_fresh_(now) &&
                this->ambient_temperature_f_ <= FREEZE_THRESHOLD_F)
            {
                this->freeze_window_active_ = true;
                this->freeze_window_deadline_ms_ = now + FREEZE_WINDOW_MS;
                this->cancel_prime_();
                this->force_target_update_ = true;
                ESP_LOGI(TAG, "Freeze protection started at %.1f F for one hour",
                         this->ambient_temperature_f_);
            }
        }

        void VGreenEvoController::set_mode_(OperatingMode mode)
        {
            if (this->mode_ == mode)
                return;
            this->mode_ = mode;
            ESP_LOGI(TAG, "Mode: %s", this->get_mode_name().c_str());
        }

        void VGreenEvoController::apply_target_(uint16_t rpm, uint32_t now, bool force)
        {
            if (rpm == 0)
            {
                this->request_stop_(now, force);
                return;
            }

            if (this->pump_ == nullptr)
            {
                ESP_LOGE(TAG, "Cannot run: pump transport is missing");
                this->force_target_update_ = true;
                return;
            }

            if (this->pending_motion_generation_ != 0 && this->pending_target_rpm_ == rpm)
            {
                if (this->pump_->motion_request_is_confirmed(
                        this->pending_motion_generation_, this->pending_target_rpm_))
                {
                    this->pending_motion_generation_ = 0;
                    this->target_rpm_ = rpm;
                    this->last_command_ms_ = now;
                    this->force_target_update_ = false;

                    if (this->prime_active_ && !this->prime_timing_started_ &&
                        this->mode_ == OperatingMode::PRIMING && rpm == PRIME_RPM)
                    {
                        this->prime_timing_started_ = true;
                        this->prime_deadline_ms_ = now + PRIME_DURATION_MS;
                        ESP_LOGI(TAG, "Priming command confirmed; starting 3-minute timer");
                    }

                    ESP_LOGD(TAG, "Confirmed %u RPM command", (unsigned) rpm);
                    return;
                }

                if (!this->pump_->motion_request_has_failed(this->pending_motion_generation_))
                    return;

                ESP_LOGW(TAG, "Run request for %u RPM failed; retrying", (unsigned) rpm);
                this->pending_motion_generation_ = 0;
                this->force_target_update_ = true;
            }

            const bool heartbeat_due = (uint32_t) (now - this->last_command_ms_) >= COMMAND_HEARTBEAT_MS;
            if (!force && rpm == this->target_rpm_ && !heartbeat_due)
                return;

            // The transport keeps this atomic at the protocol level: Go is queued
            // only after Set Demand has been acknowledged successfully. The
            // controller does not publish the new target or start timing priming
            // until the subsequent Go acknowledgment is also received.
            uint32_t request_generation = 0;
            if (!this->pump_->request_run(rpm, &request_generation) || request_generation == 0)
            {
                this->force_target_update_ = true;
                ESP_LOGW(TAG, "Run request for %u RPM was not queued; will retry", (unsigned) rpm);
                return;
            }

            this->pending_motion_generation_ = request_generation;
            this->pending_target_rpm_ = rpm;
            this->force_target_update_ = true;
            ESP_LOGD(TAG, "Queued %u RPM request; awaiting Set Demand and Go acknowledgments",
                     (unsigned) rpm);
        }

        bool VGreenEvoController::request_stop_(uint32_t now, bool force)
        {
            if (this->pump_ == nullptr)
            {
                ESP_LOGE(TAG, "Cannot stop: pump transport is missing");
                this->force_target_update_ = true;
                return false;
            }

            if (this->pending_motion_generation_ != 0 && this->pending_target_rpm_ == 0)
            {
                if (this->pump_->motion_request_is_confirmed(this->pending_motion_generation_, 0))
                {
                    this->pending_motion_generation_ = 0;
                    this->target_rpm_ = 0;
                    this->last_command_ms_ = now;
                    this->force_target_update_ = false;
                    ESP_LOGD(TAG, "Confirmed Stop");
                    return true;
                }

                if (!this->pump_->motion_request_has_failed(this->pending_motion_generation_))
                    return false;

                ESP_LOGW(TAG, "Stop request failed; retrying");
                this->pending_motion_generation_ = 0;
                this->force_target_update_ = true;
            }

            const bool heartbeat_due = (uint32_t) (now - this->last_command_ms_) >= COMMAND_HEARTBEAT_MS;
            if (!force && this->target_rpm_ == 0 && !heartbeat_due)
                return true;

            uint32_t request_generation = 0;
            if (!this->pump_->request_stop(&request_generation) || request_generation == 0)
            {
                this->force_target_update_ = true;
                ESP_LOGW(TAG, "Stop request was not queued; will retry");
                return false;
            }

            this->pending_motion_generation_ = request_generation;
            this->pending_target_rpm_ = 0;
            this->force_target_update_ = true;
            ESP_LOGD(TAG, "Queued Stop request; awaiting acknowledgment");
            return false;
        }

        void VGreenEvoController::cancel_prime_()
        {
            this->prime_active_ = false;
            this->prime_timing_started_ = false;
        }

        void VGreenEvoController::restart_schedule_phase_(uint32_t now)
        {
            this->advance_schedule_clock_(now);
            this->schedule_elapsed_ms_ = 0;
            this->override_active_ = false;
            this->cancel_prime_();
            this->force_target_update_ = true;
        }

        bool VGreenEvoController::deadline_reached_(uint32_t now, uint32_t deadline) const
        {
            return (int32_t) (now - deadline) >= 0;
        }
    }
}
