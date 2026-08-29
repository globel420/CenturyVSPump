#pragma once

#include "esphome/components/centuryvspump/CenturyVSPump.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome
{
    using namespace sensor;

    namespace century_vs_pump
    {
        class CenturyVSPumpSensor : public CenturyPumpItemBase, public Component, public Sensor
        {
        public:
            CenturyVSPumpSensor(uint8_t page, uint8_t address, float scale, bool is_signed = false) : CenturyPumpItemBase(), page_(page), address_(address), scale_(scale), is_signed_(is_signed)
            {
            }

            CenturyPumpCommand create_command() override;

        private:
            uint8_t page_;
            uint8_t address_;
            float scale_;
            bool is_signed_;
        };

    }
}
