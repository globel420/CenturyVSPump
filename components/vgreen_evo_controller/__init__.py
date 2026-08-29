import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, sensor, switch
from esphome.const import CONF_ID

from esphome.components.centuryvspump import CenturyVSPump


CODEOWNERS = ["@globel420"]
DEPENDENCIES = ["centuryvspump"]

CONF_CENTURYVSPUMP_ID = "centuryvspump_id"
CONF_RUN_SWITCH_ID = "run_switch_id"
CONF_DEMAND_NUMBER_ID = "demand_number_id"
CONF_AMBIENT_TEMPERATURE_ID = "ambient_temperature_id"

vgreen_evo_ns = cg.esphome_ns.namespace("vgreen_evo_controller")
VGreenEvoController = vgreen_evo_ns.class_("VGreenEvoController", cg.Component)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VGreenEvoController),
            cv.Required(CONF_CENTURYVSPUMP_ID): cv.use_id(CenturyVSPump),
            cv.Required(CONF_RUN_SWITCH_ID): cv.use_id(switch.Switch),
            cv.Required(CONF_DEMAND_NUMBER_ID): cv.use_id(number.Number),
            cv.Optional(CONF_AMBIENT_TEMPERATURE_ID): cv.use_id(sensor.Sensor),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pump = await cg.get_variable(config[CONF_CENTURYVSPUMP_ID])
    run_switch = await cg.get_variable(config[CONF_RUN_SWITCH_ID])
    demand_number = await cg.get_variable(config[CONF_DEMAND_NUMBER_ID])

    cg.add(var.set_pump(pump))
    cg.add(var.set_run_switch(run_switch))
    cg.add(var.set_demand_number(demand_number))

    if CONF_AMBIENT_TEMPERATURE_ID in config:
        ambient_temperature = await cg.get_variable(
            config[CONF_AMBIENT_TEMPERATURE_ID]
        )
        cg.add(var.set_ambient_temperature_sensor(ambient_temperature))
