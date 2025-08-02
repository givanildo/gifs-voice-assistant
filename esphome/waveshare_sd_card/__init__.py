import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_MODE,
    CONF_OUTPUT,
    DEVICE_CLASS_DATA_SIZE,
    STATE_CLASS_MEASUREMENT,
    UNIT_BYTES,
)

DEPENDENCIES = ["esp32", "sensor"]

CONF_WAVESHARE_SD_CARD_ID = "waveshare_sd_card_id"
CONF_CLK_PIN = "clk_pin"
CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"
CONF_CS_PIN = "cs_pin"
CONF_TOTAL_SPACE = "total_space"
CONF_USED_SPACE = "used_space"
CONF_FREE_SPACE = "free_space"

waveshare_sd_card_ns = cg.esphome_ns.namespace("waveshare_sd_card")
WaveshareSdCard = waveshare_sd_card_ns.class_("WaveshareSdCard", cg.Component)
FileInfo = waveshare_sd_card_ns.struct("FileInfo")

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WaveshareSdCard),
    cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
    cv.Required(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
    cv.Required(CONF_DATA0_PIN): pins.internal_gpio_input_pin_number,
    cv.Optional(CONF_CS_PIN): pins.gpio_pin_schema({CONF_OUTPUT: True}),
    cv.Optional(CONF_TOTAL_SPACE): sensor.sensor_schema(
        unit_of_measurement=UNIT_BYTES,
        device_class=DEVICE_CLASS_DATA_SIZE,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
    ),
    cv.Optional(CONF_USED_SPACE): sensor.sensor_schema(
        unit_of_measurement=UNIT_BYTES,
        device_class=DEVICE_CLASS_DATA_SIZE,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
    ),
    cv.Optional(CONF_FREE_SPACE): sensor.sensor_schema(
        unit_of_measurement=UNIT_BYTES,
        device_class=DEVICE_CLASS_DATA_SIZE,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
    ),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_clk_pin(config[CONF_CLK_PIN]))
    cg.add(var.set_cmd_pin(config[CONF_CMD_PIN]))
    cg.add(var.set_data0_pin(config[CONF_DATA0_PIN]))
    if CONF_CS_PIN in config:
        cs_pin = await cg.gpio_pin_expression(config[CONF_CS_PIN])
        cg.add(var.set_cs_pin(cs_pin))
    if CONF_TOTAL_SPACE in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_SPACE])
        cg.add(var.set_total_space_sensor(sens))
    if CONF_USED_SPACE in config:
        sens = await sensor.new_sensor(config[CONF_USED_SPACE])
        cg.add(var.set_used_space_sensor(sens))
    if CONF_FREE_SPACE in config:
        sens = await sensor.new_sensor(config[CONF_FREE_SPACE])
        cg.add(var.set_free_space_sensor(sens))
