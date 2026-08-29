#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/version.h"

#include "esphome/components/modbus/modbus.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <queue>
#include <span>
#include <utility>
#include <vector>

// #define MODBUS_ENABLE_SWITCH

/*
    I know there are multiple classes in this file, but I wanted to keep them in one place
    so it's easier for other folk to integrate into their ESPHome environment

    This component piggybacks on the modbus driver, replacing logically a modbus_controller
    since the controller has a hardwired view of what the protocol looks like and this protocol
    uses user defined functions.
*/

namespace esphome
{
    using namespace modbus;

    namespace century_vs_pump
    {

        class CenturyVSPump;
        class CenturyVSPumpSensor;

        /////////////////////////////////////////////////////////////////////////////////////////////////
        class CenturyPumpCommand
        {
        public:
            static const uint8_t MAX_SEND_REPEATS = 5;
            static constexpr size_t MAX_PAYLOAD_SIZE = 11;
            CenturyVSPump *pump_{};
            uint8_t function_{};
            uint8_t ack_{0x20};
            std::vector<uint8_t> payload_ = {};
            std::function<bool(CenturyVSPump *pump, const std::vector<uint8_t> &data)> validate_data_func_;
            std::function<void(CenturyVSPump *pump, const std::vector<uint8_t> &data)> on_data_func_;
            std::function<void(CenturyVSPump *pump)> on_failure_func_;
            // limit the number of repeats
            uint8_t send_countdown{MAX_SEND_REPEATS};

            bool send();

            static CenturyPumpCommand create_status_command(CenturyVSPump *pump, std::function<void(CenturyVSPump *pump, bool running)> on_status_func);
            static CenturyPumpCommand create_read_sensor_command(CenturyVSPump *pump, uint8_t page, uint8_t address, float scale, bool is_signed, std::function<void(CenturyVSPump *pump, float value)> on_value_func);
            // Compatibility overload for existing unsigned integer consumers.
            static CenturyPumpCommand create_read_sensor_command(CenturyVSPump *pump, uint8_t page, uint8_t address, uint16_t scale, std::function<void(CenturyVSPump *pump, uint16_t value)> on_value_func);
            static CenturyPumpCommand create_run_command(CenturyVSPump *pump, std::function<void(CenturyVSPump *pump)> on_confirmation_func, std::function<void(CenturyVSPump *pump)> on_failure_func = {});
            static CenturyPumpCommand create_stop_command(CenturyVSPump *pump, std::function<void(CenturyVSPump *pump)> on_confirmation_func, std::function<void(CenturyVSPump *pump)> on_failure_func = {});
            static CenturyPumpCommand create_set_demand_command(CenturyVSPump *pump, uint16_t demand, std::function<void(CenturyVSPump *pump)> on_confirmation_func, std::function<void(CenturyVSPump *pump)> on_failure_func = {});
        };

        /////////////////////////////////////////////////////////////////////////////////////////////////
        class CenturyPumpItemBase
        {
        public:
            CenturyPumpItemBase() : pump_(nullptr) {}
            CenturyPumpItemBase(CenturyVSPump *pump) : pump_(pump) {}
            virtual CenturyPumpCommand create_command() = 0;

            void set_pump(CenturyVSPump *pump) { pump_ = pump; }

        protected:
            CenturyVSPump *pump_;
        };

/////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef MODBUS_ENABLE_SWITCH
        class CenturyPumpEnabledSwitch : public esphome::switch_::Switch
        {
        public:
            void write_state(bool state) override
            {
                enabled_ = state;
                this->publish_state(enabled_);
            }

        private:
            bool enabled_{false};
        };
#endif

        /////////////////////////////////////////////////////////////////////////////////////////////////
        //
        //  To work successfully, this component needs modification to the ESPHome modbus.cpp file which
        //  can be found in the `modbus` component in https://github.com/gazoodle/esphome branch
        //  "Feature-Request-#1725"
        //
        class CenturyVSPump : public PollingComponent,
                              public ModbusDevice
        {
        public:
            CenturyVSPump() {}

            uint8_t get_address() const { return this->address_; }

            bool transport_healthy() const { return this->transport_healthy_; }
            bool communication_is_healthy(uint32_t max_age_ms) const;
            bool has_recent_no_fault_status(uint32_t max_age_ms) const;
            bool motor_fault_active() const { return this->motor_fault_active_; }
            bool motion_request_is_confirmed(uint32_t generation, uint16_t target_rpm) const;
            bool motion_request_has_failed(uint32_t generation) const;
            uint32_t successful_transactions() const { return this->successful_transactions_; }
            uint32_t failed_transactions() const { return this->failed_transactions_; }
            uint32_t dropped_commands() const { return this->dropped_commands_; }
            uint8_t consecutive_failures() const { return this->consecutive_failures_; }
            uint32_t last_response_timestamp() const { return this->last_response_timestamp_; }
            size_t command_queue_size() const { return this->command_queue_.size(); }
            size_t response_queue_size() const { return this->response_queue_.size(); }

            void loop() override;
            void setup() override;
            void update() override;
            void dump_config() override;

            /// called when a modbus response was parsed without errors
            void on_modbus_data(const std::vector<uint8_t> &data) override;
            /// called when a modbus error response was received
            void on_modbus_error(uint8_t function_code, uint8_t exception_code) override;
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 8, 0)
            bool on_no_response(std::span<const uint8_t> request_pdu) override;
            void on_not_sent(std::span<const uint8_t> request_pdu) override;
#else
            bool on_modbus_no_response() override;
            void on_modbus_not_sent() override;
#endif
            /// Registers an item with the controller. Called by esphomes code generator
            void add_item(CenturyPumpItemBase *item) { items_.push_back(item); }
            void queue_command_(const CenturyPumpCommand &cmd);
            bool request_run(uint16_t rpm, uint32_t *generation_out = nullptr);
            bool request_stop(uint32_t *generation_out = nullptr);

        protected:
            void process_modbus_data_(const CenturyPumpCommand *response);
            bool send_next_command_();
            void mark_transport_success_();
            void mark_transport_failure_();
            void queue_command_front_(const CenturyPumpCommand &command);

        private:
            static constexpr size_t MAX_COMMAND_QUEUE_SIZE = 32;
            static constexpr size_t MAX_RESPONSE_QUEUE_SIZE = 8;
            static constexpr size_t MAX_RESPONSE_PAYLOAD_SIZE = CenturyPumpCommand::MAX_PAYLOAD_SIZE + 2;
            static constexpr uint8_t MAX_TOLERATED_CONSECUTIVE_FAILURES = 2;
            static constexpr uint16_t MIN_RUN_RPM = 600;
            static constexpr uint16_t MAX_RUN_RPM = 3450;

            bool enqueue_command_(const CenturyPumpCommand &command, bool front);
            void purge_pending_motion_commands_();
            void notify_command_failure_(const CenturyPumpCommand *command);
            bool handle_modbus_no_response_();
            void handle_modbus_not_sent_();
            uint32_t begin_motion_request_(uint16_t target_rpm, uint32_t *generation_out);
            bool motion_request_is_pending_(uint32_t generation, uint16_t target_rpm) const;
            void confirm_motion_request_(uint32_t generation, uint16_t target_rpm);
            void fail_motion_request_(uint32_t generation);
            void invalidate_pending_motion_request_();
            void update_motor_fault_(uint8_t status);

            friend class CenturyPumpCommand;

            std::list<std::unique_ptr<CenturyPumpCommand>> command_queue_;
            std::queue<std::unique_ptr<CenturyPumpCommand>> response_queue_;
            uint32_t last_command_timestamp_{0};
            uint32_t last_response_timestamp_{0};
            uint32_t last_status_timestamp_{0};
            uint16_t command_throttle_{10};
            bool command_in_flight_{false};
            bool transport_healthy_{false};
            uint32_t successful_transactions_{0};
            uint32_t failed_transactions_{0};
            uint32_t dropped_commands_{0};
            uint8_t consecutive_failures_{0};
            uint32_t motion_request_generation_{0};
            uint16_t motion_request_target_rpm_{0};
            enum class MotionRequestState : uint8_t
            {
                NONE,
                PENDING,
                CONFIRMED,
                FAILED,
            };
            MotionRequestState motion_request_state_{MotionRequestState::NONE};
            bool run_requested_{false};
            bool status_seen_{false};
            // Unknown-at-boot is represented by status_seen_ == false. Keep the
            // fault bit clear until the motor explicitly reports a fault or an
            // unrecognized status; the controller's status gate still blocks Run.
            bool motor_fault_active_{false};

        public:
            std::string name_;
            std::vector<CenturyPumpItemBase *> items_;
#ifdef MODBUS_ENABLE_SWITCH
            CenturyPumpEnabledSwitch *enabled_switch_{nullptr};
#endif
        };

    }
}
