"""cync_button: a debounced button input that does not depend on the main loop.

A high-priority esp_timer samples the pin every `sample_period`; a press or
release is reported once the pin has read the same for `debounce`. Edges are
also counted by an interrupt so sub-millisecond spikes can be logged.
"""

from esphome import pins
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_PIN

CODEOWNERS = ["@RoussLabs"]

cync_button_ns = cg.esphome_ns.namespace("cync_button")
CyncButton = cync_button_ns.class_(
    "CyncButton", binary_sensor.BinarySensor, cg.Component
)

CONF_DEBOUNCE = "debounce"
CONF_SAMPLE_PERIOD = "sample_period"
CONF_LOG_SPIKES = "log_spikes"


def _validate(config):
    debounce = config[CONF_DEBOUNCE].total_milliseconds
    period = config[CONF_SAMPLE_PERIOD].total_milliseconds
    if period <= 0 or period > debounce:
        raise cv.Invalid(
            f"{CONF_SAMPLE_PERIOD} must be between 1ms and {CONF_DEBOUNCE} ({debounce}ms)"
        )
    return config


CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(CyncButton)
    .extend(
        {
            cv.Required(CONF_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_DEBOUNCE, default="50ms"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_SAMPLE_PERIOD, default="5ms"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_LOG_SPIKES, default=True): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    _validate,
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_debounce(config[CONF_DEBOUNCE].total_milliseconds))
    cg.add(var.set_sample_period(config[CONF_SAMPLE_PERIOD].total_milliseconds))
    cg.add(var.set_log_spikes(config[CONF_LOG_SPIKES]))
