#include "CenturyVSPump.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"

namespace esphome
{
    namespace century_vs_pump
    {

        //////////////////////////////////////////////////////////////////////////////////////////////
        //
        //  CenturyVSPump implementation
        //
        /////////////////////////////////////////////////////////////////////////////////////////////

        static const char *const TAG = "century_vs_pump";

        static size_t minimum_response_data_length(uint8_t function)
        {
            switch (function)
            {
            case 0x43: // Status
                return 1;
            case 0x44: // Set demand echoes mode and demand
                return 3;
            case 0x45: // Read sensor echoes page/address and a 16-bit value
                return 4;
            default:
                return 0;
            }
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::setup()
        {
#ifdef MODBUS_ENABLE_SWITCH
            enabled_switch_ = new CenturyPumpEnabledSwitch();
            enabled_switch_->set_name(name_ + " MODBUS enabled");
            App.register_switch(enabled_switch_);
#endif
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::loop()
        {
            // Incoming data to process?
            if (!response_queue_.empty())
            {
                auto message = std::move(response_queue_.front());
                response_queue_.pop();
                if (message != nullptr)
                    process_modbus_data_(message.get());
                else
                {
                    ESP_LOGW(TAG, "Discarding null response queue entry");
                    mark_transport_failure_();
                }
            }
            else
            {
                // all messages processed send pending commmands
                send_next_command_();
            }
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::update()
        {
            // Request status & pump RPM
#ifdef MODBUS_ENABLE_SWITCH
            if (enabled_switch_ == nullptr)
                return;
            if (enabled_switch_->state == 0)
                return;
#endif
            ESP_LOGV(TAG, "Updating pump component");
            for (auto item : items_)
            {
                if (item != nullptr)
                    queue_command_(item->create_command());
                else
                    ESP_LOGW(TAG, "Ignoring null pump item");
            }
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        /// called when a modbus response was parsed without errors
        void CenturyVSPump::on_modbus_data(const std::vector<uint8_t> &data)
        {
            ESP_LOGV(TAG, "Pump got data");
            if (this->command_queue_.empty())
            {
                ESP_LOGW(TAG, "Received pump data with no command awaiting a response");
                mark_transport_failure_();
                return;
            }

            auto current_command = std::move(this->command_queue_.front());
            command_queue_.pop_front();
            this->command_in_flight_ = false;
            if (current_command == nullptr)
            {
                ESP_LOGW(TAG, "Received pump data for a null command");
                mark_transport_failure_();
                return;
            }

            if (data.size() > MAX_RESPONSE_PAYLOAD_SIZE)
            {
                ESP_LOGW(TAG, "Function %02X returned an oversized payload (%u > %u)", current_command->function_,
                         static_cast<unsigned>(data.size()), static_cast<unsigned>(MAX_RESPONSE_PAYLOAD_SIZE));
                this->dropped_commands_++;
                mark_transport_failure_();
                notify_command_failure_(current_command.get());
                return;
            }

            if (this->response_queue_.size() >= MAX_RESPONSE_QUEUE_SIZE)
            {
                ESP_LOGW(TAG, "Response queue full; dropping function %02X", current_command->function_);
                this->dropped_commands_++;
                mark_transport_failure_();
                notify_command_failure_(current_command.get());
                return;
            }

            current_command->payload_ = data;
            this->response_queue_.push(std::move(current_command));
            ESP_LOGV(TAG, "Pump response queued");
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        /// called when a modbus error response was received
        void CenturyVSPump::on_modbus_error(uint8_t function_code, uint8_t exception_code)
        {
            ESP_LOGW(TAG, "Received Modbus error for function %02X: exception %02X", function_code, exception_code);
            if (this->command_queue_.empty())
            {
                ESP_LOGW(TAG, "Modbus error arrived with no queued command");
                mark_transport_failure_();
                return;
            }

            auto current_command = std::move(this->command_queue_.front());
            if (current_command != nullptr && current_command->function_ != function_code)
                ESP_LOGW(TAG, "Modbus error function mismatch (%02X != queued %02X)", function_code, current_command->function_);

            command_queue_.pop_front();
            this->command_in_flight_ = false;
            mark_transport_failure_();
            notify_command_failure_(current_command.get());
        }

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 8, 0)
        /////////////////////////////////////////////////////////////////////////////////////////////
        bool CenturyVSPump::on_no_response(std::span<const uint8_t> request_pdu)
        {
            return this->handle_modbus_no_response_();
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::on_not_sent(std::span<const uint8_t> request_pdu)
        {
            this->handle_modbus_not_sent_();
        }
#else
        /////////////////////////////////////////////////////////////////////////////////////////////
        bool CenturyVSPump::on_modbus_no_response()
        {
            return this->handle_modbus_no_response_();
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::on_modbus_not_sent()
        {
            this->handle_modbus_not_sent_();
        }
#endif

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::notify_command_failure_(const CenturyPumpCommand *command)
        {
            if (command != nullptr && command->on_failure_func_)
                command->on_failure_func_(this);
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        bool CenturyVSPump::handle_modbus_no_response_()
        {
            if (this->command_queue_.empty())
            {
                ESP_LOGW(TAG, "Pump command timed out with no correlation entry");
                mark_transport_failure_();
                return false;
            }

            auto &command = this->command_queue_.front();
            if (command == nullptr)
            {
                ESP_LOGW(TAG, "Null pump command timed out");
                this->command_queue_.pop_front();
                this->command_in_flight_ = false;
                mark_transport_failure_();
                return false;
            }

            if (command->on_failure_func_)
            {
                ESP_LOGW(TAG, "Motion command %02X timed out", command->function_);
                auto failed_command = std::move(command);
                this->command_queue_.pop_front();
                this->command_in_flight_ = false;
                mark_transport_failure_();
                notify_command_failure_(failed_command.get());
            }

            // Ordinary telemetry retains the existing bounded component-level
            // retry behavior. Motion commands fail on their first timeout.
            return false;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::handle_modbus_not_sent_()
        {
            if (this->command_queue_.empty())
            {
                ESP_LOGW(TAG, "Pump command was not sent and has no correlation entry");
                mark_transport_failure_();
                return;
            }

            auto failed_command = std::move(this->command_queue_.front());
            this->command_queue_.pop_front();
            this->command_in_flight_ = false;
            ESP_LOGW(TAG, "Pump command was dropped before transmission");
            mark_transport_failure_();
            notify_command_failure_(failed_command.get());
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::dump_config()
        {
            ESP_LOGCONFIG(TAG, "CenturyVSPump:");
            ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
            ESP_LOGCONFIG(TAG, "  Command queue limit: %u", static_cast<unsigned>(MAX_COMMAND_QUEUE_SIZE));
            ESP_LOGCONFIG(TAG, "  Response queue limit: %u", static_cast<unsigned>(MAX_RESPONSE_QUEUE_SIZE));
            ESP_LOGCONFIG(TAG, "  Response payload limit: %u", static_cast<unsigned>(MAX_RESPONSE_PAYLOAD_SIZE));
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::queue_command_(const CenturyPumpCommand &command)
        {
            this->enqueue_command_(command, false);
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::queue_command_front_(const CenturyPumpCommand &command)
        {
            this->enqueue_command_(command, true);
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        bool CenturyVSPump::enqueue_command_(const CenturyPumpCommand &command, bool front)
        {
#ifdef MODBUS_ENABLE_SWITCH
            if (enabled_switch_ == nullptr)
            {
                notify_command_failure_(&command);
                return false;
            }
            if (enabled_switch_->state == 0)
            {
                notify_command_failure_(&command);
                return false;
            }
#endif
            if (command.pump_ == nullptr)
            {
                ESP_LOGW(TAG, "Rejecting function %02X with no pump assigned", command.function_);
                this->dropped_commands_++;
                mark_transport_failure_();
                notify_command_failure_(&command);
                return false;
            }

            if (command.payload_.size() > CenturyPumpCommand::MAX_PAYLOAD_SIZE)
            {
                ESP_LOGW(TAG, "Rejecting function %02X with oversized payload (%u > %u)", command.function_,
                         static_cast<unsigned>(command.payload_.size()),
                         static_cast<unsigned>(CenturyPumpCommand::MAX_PAYLOAD_SIZE));
                this->dropped_commands_++;
                mark_transport_failure_();
                notify_command_failure_(&command);
                return false;
            }

            if (command_queue_.size() >= MAX_COMMAND_QUEUE_SIZE)
            {
                if (front)
                {
                    auto evicted = command_queue_.end();
                    --evicted;
                    if (!(this->command_in_flight_ && evicted == command_queue_.begin()))
                    {
                        auto evicted_command = std::move(*evicted);
                        if (evicted_command != nullptr)
                            ESP_LOGW(TAG, "Command queue full; evicting pending function %02X for priority function %02X",
                                     evicted_command->function_, command.function_);
                        else
                            ESP_LOGW(TAG, "Command queue full; evicting null pending entry for priority function %02X",
                                     command.function_);
                        command_queue_.erase(evicted);
                        this->dropped_commands_++;
                        notify_command_failure_(evicted_command.get());
                    }
                    else
                    {
                        ESP_LOGW(TAG, "Command queue full with no evictable entry; dropping priority function %02X",
                                 command.function_);
                        this->dropped_commands_++;
                        mark_transport_failure_();
                        notify_command_failure_(&command);
                        return false;
                    }
                }
                else
                {
                    ESP_LOGW(TAG, "Command queue full; dropping function %02X", command.function_);
                    this->dropped_commands_++;
                    mark_transport_failure_();
                    notify_command_failure_(&command);
                    return false;
                }
            }

            if (front)
            {
                auto position = command_queue_.begin();
                // The front entry is also the response-correlation record. Never
                // insert ahead of it while it is in flight.
                if (this->command_in_flight_ && position != command_queue_.end())
                    ++position;
                command_queue_.insert(position, make_unique<CenturyPumpCommand>(command));
            }
            else
                command_queue_.push_back(make_unique<CenturyPumpCommand>(command));
            return true;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::purge_pending_motion_commands_()
        {
            auto command = this->command_queue_.begin();
            if (this->command_in_flight_ && command != this->command_queue_.end())
                ++command;

            while (command != this->command_queue_.end())
            {
                if (*command != nullptr && ((*command)->function_ == 0x41 || (*command)->function_ == 0x42 ||
                                            (*command)->function_ == 0x44))
                    command = this->command_queue_.erase(command);
                else
                    ++command;
            }
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        uint32_t CenturyVSPump::begin_motion_request_(uint16_t target_rpm, uint32_t *generation_out)
        {
            uint32_t generation = this->motion_request_generation_ + 1U;
            if (generation == 0)
                generation = 1;

            this->motion_request_generation_ = generation;
            this->motion_request_target_rpm_ = target_rpm;
            this->motion_request_state_ = MotionRequestState::PENDING;
            if (generation_out != nullptr)
                *generation_out = generation;
            return generation;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        bool CenturyVSPump::motion_request_is_pending_(uint32_t generation, uint16_t target_rpm) const
        {
            return generation != 0 && generation == this->motion_request_generation_ &&
                   target_rpm == this->motion_request_target_rpm_ &&
                   this->motion_request_state_ == MotionRequestState::PENDING;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::confirm_motion_request_(uint32_t generation, uint16_t target_rpm)
        {
            if (!this->motion_request_is_pending_(generation, target_rpm))
                return;

            this->motion_request_state_ = MotionRequestState::CONFIRMED;
            this->run_requested_ = target_rpm != 0;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::fail_motion_request_(uint32_t generation)
        {
            if (generation == 0 || generation != this->motion_request_generation_ ||
                this->motion_request_state_ != MotionRequestState::PENDING)
                return;

            this->motion_request_state_ = MotionRequestState::FAILED;
            this->run_requested_ = false;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::invalidate_pending_motion_request_()
        {
            if (this->motion_request_state_ == MotionRequestState::PENDING)
                this->fail_motion_request_(this->motion_request_generation_);
            this->run_requested_ = false;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        bool CenturyVSPump::request_run(uint16_t rpm, uint32_t *generation_out)
        {
            if (generation_out != nullptr)
                *generation_out = 0;

            if (rpm < MIN_RUN_RPM || rpm > MAX_RUN_RPM)
            {
                ESP_LOGW(TAG, "Rejecting run request outside verified range %u..%u RPM: %u",
                         static_cast<unsigned>(MIN_RUN_RPM), static_cast<unsigned>(MAX_RUN_RPM),
                         static_cast<unsigned>(rpm));
                this->dropped_commands_++;
                return false;
            }

            if (this->motor_fault_active_)
            {
                ESP_LOGW(TAG, "Rejecting run request while a motor fault is active");
                return false;
            }

            const uint32_t request_generation = this->begin_motion_request_(rpm, generation_out);
            this->run_requested_ = true;
            this->purge_pending_motion_commands_();
            const bool queued = this->enqueue_command_(CenturyPumpCommand::create_set_demand_command(
                this, rpm, [request_generation, rpm](CenturyVSPump *pump)
                {
                    if (pump == nullptr || !pump->motion_request_is_pending_(request_generation, rpm) ||
                        pump->motor_fault_active_)
                    {
                        ESP_LOGD(TAG, "Suppressing stale or faulted Go request");
                        return;
                    }

                    // The Set Demand command has already been removed from the queue at this point.
                    // Put Go ahead of unrelated polling traffic, but never enqueue it before ACK.
                    const bool go_queued = pump->enqueue_command_(CenturyPumpCommand::create_run_command(
                        pump, [request_generation, rpm](CenturyVSPump *callback_pump)
                        {
                            if (callback_pump != nullptr)
                                callback_pump->confirm_motion_request_(request_generation, rpm);
                        },
                        [request_generation](CenturyVSPump *callback_pump)
                        {
                            if (callback_pump != nullptr)
                                callback_pump->fail_motion_request_(request_generation);
                        }), true);
                    if (!go_queued)
                    {
                        ESP_LOGW(TAG, "Unable to queue Go after confirmed Set Demand");
                        pump->fail_motion_request_(request_generation);
                    }
                },
                [request_generation](CenturyVSPump *pump)
                {
                    if (pump != nullptr)
                        pump->fail_motion_request_(request_generation);
                }), true);
            if (!queued)
            {
                this->fail_motion_request_(request_generation);
                if (generation_out != nullptr)
                    *generation_out = 0;
            }
            return queued;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        bool CenturyVSPump::request_stop(uint32_t *generation_out)
        {
            if (generation_out != nullptr)
                *generation_out = 0;

            const uint32_t request_generation = this->begin_motion_request_(0, generation_out);
            this->run_requested_ = false;
            this->purge_pending_motion_commands_();
            const bool queued = this->enqueue_command_(CenturyPumpCommand::create_stop_command(
                this, [request_generation](CenturyVSPump *pump)
                {
                    if (pump != nullptr)
                        pump->confirm_motion_request_(request_generation, 0);
                },
                [request_generation](CenturyVSPump *pump)
                {
                    if (pump != nullptr)
                        pump->fail_motion_request_(request_generation);
                }), true);
            if (!queued)
            {
                this->fail_motion_request_(request_generation);
                if (generation_out != nullptr)
                    *generation_out = 0;
            }
            return queued;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::process_modbus_data_(const CenturyPumpCommand *response)
        {
            if (response == nullptr)
            {
                ESP_LOGW(TAG, "Cannot process a null pump response");
                mark_transport_failure_();
                return;
            }

            if (response->payload_.empty())
            {
                ESP_LOGW(TAG, "Function %02X returned an empty payload", response->function_);
                mark_transport_failure_();
                notify_command_failure_(response);
                return;
            }

            // ESPHome 2026.7+ consumes the echoed function byte and provides
            // [ACK, data...]. Retain support for older [function, ACK, data...]
            // payloads so existing pinned installations can upgrade safely.
            size_t ack_index = 0;
            if (response->payload_[0] == response->function_)
            {
                ack_index = 1;
                if (response->payload_.size() <= ack_index)
                {
                    ESP_LOGW(TAG, "Function %02X returned no ACK byte", response->function_);
                    mark_transport_failure_();
                    notify_command_failure_(response);
                    return;
                }
            }

            const uint8_t ack = response->payload_[ack_index];
            if (ack != 0x10)
            {
                ESP_LOGW(TAG, "Function %02X returned unexpected ACK %02X", response->function_, ack);
                mark_transport_failure_();
                notify_command_failure_(response);
                return;
            }

            const size_t data_offset = ack_index + 1;
            const size_t data_length = response->payload_.size() - data_offset;
            const size_t minimum_length = minimum_response_data_length(response->function_);
            if (data_length < minimum_length)
            {
                ESP_LOGW(TAG, "Function %02X returned %u data bytes; expected at least %u", response->function_,
                         static_cast<unsigned>(data_length), static_cast<unsigned>(minimum_length));
                mark_transport_failure_();
                notify_command_failure_(response);
                return;
            }

            if (!response->on_data_func_)
            {
                ESP_LOGW(TAG, "Function %02X has no response handler", response->function_);
                mark_transport_failure_();
                notify_command_failure_(response);
                return;
            }

            std::vector<uint8_t> data(response->payload_.begin() + data_offset, response->payload_.end());
            if (response->validate_data_func_ && !response->validate_data_func_(this, data))
            {
                mark_transport_failure_();
                notify_command_failure_(response);
                return;
            }

            mark_transport_success_();
            response->on_data_func_(this, data);
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::mark_transport_success_()
        {
            this->transport_healthy_ = true;
            this->successful_transactions_++;
            this->consecutive_failures_ = 0;
            this->last_response_timestamp_ = millis();
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::mark_transport_failure_()
        {
            this->failed_transactions_++;
            if (this->consecutive_failures_ < 0xFF)
                this->consecutive_failures_++;
            this->transport_healthy_ = this->successful_transactions_ > 0 &&
                                       this->consecutive_failures_ <= MAX_TOLERATED_CONSECUTIVE_FAILURES;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        bool CenturyVSPump::communication_is_healthy(uint32_t max_age_ms) const
        {
            return this->successful_transactions_ > 0 &&
                   this->consecutive_failures_ <= MAX_TOLERATED_CONSECUTIVE_FAILURES &&
                   static_cast<uint32_t>(millis() - this->last_response_timestamp_) <= max_age_ms;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        bool CenturyVSPump::has_recent_no_fault_status(uint32_t max_age_ms) const
        {
            return this->status_seen_ && !this->motor_fault_active_ &&
                   static_cast<uint32_t>(millis() - this->last_status_timestamp_) <= max_age_ms;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        bool CenturyVSPump::motion_request_is_confirmed(uint32_t generation, uint16_t target_rpm) const
        {
            return generation != 0 && generation == this->motion_request_generation_ &&
                   target_rpm == this->motion_request_target_rpm_ &&
                   this->motion_request_state_ == MotionRequestState::CONFIRMED;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        bool CenturyVSPump::motion_request_has_failed(uint32_t generation) const
        {
            return generation == 0 || generation != this->motion_request_generation_ ||
                   this->motion_request_state_ == MotionRequestState::FAILED;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::update_motor_fault_(uint8_t status)
        {
            if (status == 0x20)
            {
                this->status_seen_ = true;
                this->last_status_timestamp_ = millis();
                this->motor_fault_active_ = true;
                this->invalidate_pending_motion_request_();
                this->purge_pending_motion_commands_();
            }
            else if (status == 0x00 || status == 0x09 || status == 0x0B)
            {
                this->status_seen_ = true;
                this->last_status_timestamp_ = millis();
                this->motor_fault_active_ = false;
            }
            else
            {
                ESP_LOGW(TAG, "Unknown pump status %02X; invalidating no-fault authorization", status);
                this->status_seen_ = false;
                this->last_status_timestamp_ = millis();
                this->motor_fault_active_ = true;
                this->invalidate_pending_motion_request_();
                this->purge_pending_motion_commands_();
            }
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        bool CenturyVSPump::send_next_command_()
        {
            uint32_t last_send = millis() - this->last_command_timestamp_;
            if ((last_send > this->command_throttle_) && this->ready_for_immediate_send() && !command_queue_.empty())
            {
                auto &command = command_queue_.front();

                if (command == nullptr)
                {
                    ESP_LOGW(TAG, "Discarding null command queue entry");
                    command_queue_.pop_front();
                    this->command_in_flight_ = false;
                    mark_transport_failure_();
                }
                else if (command->send_countdown < 1)
                {
                    ESP_LOGD(TAG, "Pump command %02X no response received - removed from send queue", command->function_);
                    auto failed_command = std::move(command);
                    command_queue_.pop_front();
                    this->command_in_flight_ = false;
                    mark_transport_failure_();
                    notify_command_failure_(failed_command.get());
                }
                else
                {
                    ESP_LOGV(TAG, "Sending command with function %02X", command->function_);
                    if (!command->send())
                    {
                        ESP_LOGW(TAG, "Unable to send function %02X", command->function_);
                        auto failed_command = std::move(command);
                        command_queue_.pop_front();
                        this->command_in_flight_ = false;
                        mark_transport_failure_();
                        notify_command_failure_(failed_command.get());
                        return false;
                    }
                    this->command_in_flight_ = true;
                    this->last_command_timestamp_ = millis();
                }
            }
            return true;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        bool CenturyPumpCommand::send()
        {
            if (pump_ == nullptr || send_countdown == 0 || payload_.size() > MAX_PAYLOAD_SIZE)
                return false;

            std::vector<uint8_t> pdu;
            pdu.reserve(2 + payload_.size());
            pdu.push_back(function_);
            pdu.push_back(0x20);
            pdu.insert(pdu.end(), payload_.begin(), payload_.end());

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 8, 0)
            const bool accepted = pump_->queue_pdu(pdu);
#else
            // ESPHome 2026.7 queues this PDU through a void API. This request is
            // already size-validated, so the legacy helper has no refusal case.
            pump_->send_pdu(pdu);
            const bool accepted = true;
#endif
            if (!accepted)
                return false;

            this->send_countdown--;
            return true;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        CenturyPumpCommand CenturyPumpCommand::create_status_command(CenturyVSPump *pump, std::function<void(CenturyVSPump *pump, bool running)> on_status_func)
        {
            CenturyPumpCommand cmd = {};
            cmd.pump_ = pump;
            cmd.function_ = 0x43; // Pump status
            cmd.on_data_func_ = [=](CenturyVSPump *pump, const std::vector<uint8_t> &data)
            {
                if (data.empty())
                    return;

                ESP_LOGD(TAG, "Got status command reply %02X", data[0]);

                if (pump != nullptr)
                    pump->update_motor_fault_(data[0]);

                if (!on_status_func)
                    return;

                if (data[0] == 0x00 || data[0] == 0x20)
                    on_status_func(pump, false);
                else if (data[0] == 0x09 || data[0] == 0x0B)
                    on_status_func(pump, true);
                else
                    ESP_LOGW(TAG, "Unknown pump status %02X", data[0]);
            };
            return cmd;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        CenturyPumpCommand CenturyPumpCommand::create_read_sensor_command(CenturyVSPump *pump, uint8_t page, uint8_t address, float scale, bool is_signed, std::function<void(CenturyVSPump *pump, float value)> on_value_func)
        {
            CenturyPumpCommand cmd = {};
            cmd.pump_ = pump;
            cmd.function_ = 0x45; // Read sensor
            cmd.payload_.push_back(page);
            cmd.payload_.push_back(address);
            cmd.validate_data_func_ = [=](CenturyVSPump *pump, const std::vector<uint8_t> &data)
            {
                if (data.size() < 4)
                    return false;
                if (data[0] != page || data[1] != address)
                {
                    ESP_LOGW(TAG, "Sensor response mismatch: requested page %u address %u, got page %u address %u",
                             page, address, data[0], data[1]);
                    return false;
                }
                if (scale <= 0.0f)
                {
                    ESP_LOGW(TAG, "Sensor page %u address %u has invalid scale %.3f", page, address, scale);
                    return false;
                }
                return true;
            };
            cmd.on_data_func_ = [=](CenturyVSPump *pump, const std::vector<uint8_t> &data)
            {
                if (!on_value_func)
                    return;

                const uint16_t raw_value = static_cast<uint16_t>(data[2]) |
                                           (static_cast<uint16_t>(data[3]) << 8);
                const float value = is_signed
                                        ? static_cast<float>(static_cast<int16_t>(raw_value)) / scale
                                        : static_cast<float>(raw_value) / scale;
                ESP_LOGD(TAG, "Read value %.3f from page %u, addr %u", value, page, address);
                on_value_func(pump, value);
            };
            return cmd;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        CenturyPumpCommand CenturyPumpCommand::create_read_sensor_command(CenturyVSPump *pump, uint8_t page, uint8_t address, uint16_t scale, std::function<void(CenturyVSPump *pump, uint16_t value)> on_value_func)
        {
            return create_read_sensor_command(pump, page, address, static_cast<float>(scale), false,
                                              [=](CenturyVSPump *callback_pump, float value)
                                              {
                                                  if (on_value_func)
                                                      on_value_func(callback_pump, static_cast<uint16_t>(value));
                                              });
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        CenturyPumpCommand CenturyPumpCommand::create_run_command(CenturyVSPump *pump, std::function<void(CenturyVSPump *pump)> on_confirmation_func, std::function<void(CenturyVSPump *pump)> on_failure_func)
        {
            CenturyPumpCommand cmd = {};
            cmd.pump_ = pump;
            cmd.function_ = 0x41; // Go
            cmd.on_failure_func_ = std::move(on_failure_func);
            cmd.on_data_func_ = [=](CenturyVSPump *pump, const std::vector<uint8_t> &data)
            {
                ESP_LOGD(TAG, "Confirmed pump running");
                if (on_confirmation_func)
                    on_confirmation_func(pump);
            };
            return cmd;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        CenturyPumpCommand CenturyPumpCommand::create_stop_command(CenturyVSPump *pump, std::function<void(CenturyVSPump *pump)> on_confirmation_func, std::function<void(CenturyVSPump *pump)> on_failure_func)
        {
            CenturyPumpCommand cmd = {};
            cmd.pump_ = pump;
            cmd.function_ = 0x42; // Stop
            cmd.on_failure_func_ = std::move(on_failure_func);
            cmd.on_data_func_ = [=](CenturyVSPump *pump, const std::vector<uint8_t> &data)
            {
                ESP_LOGD(TAG, "Confirmed pump stopped");
                if (on_confirmation_func)
                    on_confirmation_func(pump);
            };
            return cmd;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        CenturyPumpCommand CenturyPumpCommand::create_set_demand_command(CenturyVSPump *pump, uint16_t demand, std::function<void(CenturyVSPump *pump)> on_confirmation_func, std::function<void(CenturyVSPump *pump)> on_failure_func)
        {
            CenturyPumpCommand cmd = {};
            cmd.pump_ = pump;
            cmd.function_ = 0x44;      // Set demand
            cmd.on_failure_func_ = std::move(on_failure_func);
            if (demand > (0xFFFFU / 4U))
            {
                ESP_LOGW(TAG, "Cannot encode out-of-range demand %u", static_cast<unsigned>(demand));
                cmd.pump_ = nullptr;
                return cmd;
            }
            cmd.payload_.push_back(0); // Mode (0=Speed, 1=Torque, 2=Reserved, 3=Reserved)
            const uint16_t scaled_demand = static_cast<uint16_t>(demand * 4U);
            cmd.payload_.push_back(scaled_demand & 0xff);
            cmd.payload_.push_back(scaled_demand >> 8);
            cmd.validate_data_func_ = [=](CenturyVSPump *pump, const std::vector<uint8_t> &data)
            {
                if (data.size() < 3)
                    return false;
                const bool matches = data[0] == 0 && data[1] == (scaled_demand & 0xff) && data[2] == (scaled_demand >> 8);
                if (!matches)
                    ESP_LOGW(TAG, "Set demand response did not echo the requested value");
                return matches;
            };
            cmd.on_data_func_ = [=](CenturyVSPump *pump, const std::vector<uint8_t> &data)
            {
                ESP_LOGD(TAG, "Set demand confirmed");
                if (on_confirmation_func)
                    on_confirmation_func(pump);
            };
            return cmd;
        }
    }
}
