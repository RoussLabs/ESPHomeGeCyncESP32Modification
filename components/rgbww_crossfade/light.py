"""Light platform: ESPHome's rgbww light with a crossfading transition.

Configuration is identical to `platform: rgbww`.
"""

import esphome.codegen as cg
from esphome.components import light, output
import esphome.config_validation as cv
from esphome.const import (
    CONF_BLUE,
    CONF_COLD_WHITE,
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_COLOR_INTERLOCK,
    CONF_CONSTANT_BRIGHTNESS,
    CONF_GREEN,
    CONF_OUTPUT_ID,
    CONF_RED,
    CONF_WARM_WHITE,
    CONF_WARM_WHITE_COLOR_TEMPERATURE,
)

rgbww_crossfade_ns = cg.esphome_ns.namespace("rgbww_crossfade")
CrossfadeRGBWWLightOutput = rgbww_crossfade_ns.class_(
    "CrossfadeRGBWWLightOutput", light.LightOutput
)

CONFIG_SCHEMA = cv.All(
    light.RGB_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(CrossfadeRGBWWLightOutput),
            cv.Required(CONF_RED): cv.use_id(output.FloatOutput),
            cv.Required(CONF_GREEN): cv.use_id(output.FloatOutput),
            cv.Required(CONF_BLUE): cv.use_id(output.FloatOutput),
            cv.Required(CONF_COLD_WHITE): cv.use_id(output.FloatOutput),
            cv.Required(CONF_WARM_WHITE): cv.use_id(output.FloatOutput),
            cv.Optional(CONF_COLD_WHITE_COLOR_TEMPERATURE): cv.color_temperature,
            cv.Optional(CONF_WARM_WHITE_COLOR_TEMPERATURE): cv.color_temperature,
            cv.Optional(CONF_CONSTANT_BRIGHTNESS, default=False): cv.boolean,
            cv.Optional(CONF_COLOR_INTERLOCK, default=False): cv.boolean,
        }
    ),
    cv.has_none_or_all_keys(
        [CONF_COLD_WHITE_COLOR_TEMPERATURE, CONF_WARM_WHITE_COLOR_TEMPERATURE]
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)

    cg.add(var.set_red(await cg.get_variable(config[CONF_RED])))
    cg.add(var.set_green(await cg.get_variable(config[CONF_GREEN])))
    cg.add(var.set_blue(await cg.get_variable(config[CONF_BLUE])))
    cg.add(var.set_cold_white(await cg.get_variable(config[CONF_COLD_WHITE])))
    cg.add(var.set_warm_white(await cg.get_variable(config[CONF_WARM_WHITE])))
    if CONF_COLD_WHITE_COLOR_TEMPERATURE in config:
        cg.add(var.set_cold_white_temperature(config[CONF_COLD_WHITE_COLOR_TEMPERATURE]))
        cg.add(var.set_warm_white_temperature(config[CONF_WARM_WHITE_COLOR_TEMPERATURE]))
    cg.add(var.set_constant_brightness(config[CONF_CONSTANT_BRIGHTNESS]))
    cg.add(var.set_color_interlock(config[CONF_COLOR_INTERLOCK]))
