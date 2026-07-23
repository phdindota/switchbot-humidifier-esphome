"""ESPHome code-generation schema for the W3902310/V07 hardware driver.

The C++ object is registered both as an ESPHome Component and a FloatOutput.
The speed-fan platform therefore supplies a normalized 0.0–1.0 request while
the driver owns the recovered GPIO/LEDC sequence and safety interlocks.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID

CODEOWNERS = ["@phdindota"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["output"]

switchbot_humidifier_ns = cg.esphome_ns.namespace("switchbot_humidifier")

SwitchBotHumidifier = switchbot_humidifier_ns.class_(
    "SwitchBotHumidifier", output.FloatOutput, cg.Component
)

CONF_SPEED_1_FREQUENCY = "speed_1_frequency"
CONF_SPEED_2_FREQUENCY = "speed_2_frequency"
CONF_SPEED_3_FREQUENCY = "speed_3_frequency"
CONF_SPEED_4_FREQUENCY = "speed_4_frequency"
CONF_WET_START_DELAY = "wet_start_delay"
CONF_FILTER_DRY_DURATION = "filter_dry_duration"
CONF_AUTOMATIC_FILTER_DRYING = "automatic_filter_drying"
CONF_TACH_SAFETY = "tach_safety"
CONF_TACH_STARTUP_TIMEOUT = "tach_startup_timeout"
CONF_MINIMUM_TACH_PULSES_PER_MINUTE = "minimum_tach_pulses_per_minute"


def _validate_frequencies(config):
    # write_state() maps the four fan steps in ascending order. Reject duplicate
    # or reversed values rather than silently assigning the wrong stock mode.
    frequencies = [
        config[CONF_SPEED_1_FREQUENCY],
        config[CONF_SPEED_2_FREQUENCY],
        config[CONF_SPEED_3_FREQUENCY],
        config[CONF_SPEED_4_FREQUENCY],
    ]
    if frequencies != sorted(set(frequencies)):
        raise cv.Invalid("speed frequencies must be unique and strictly increasing")
    return config


CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SwitchBotHumidifier),
            cv.Optional(CONF_SPEED_1_FREQUENCY, default=125): cv.int_range(
                min=50, max=1000
            ),
            cv.Optional(CONF_SPEED_2_FREQUENCY, default=250): cv.int_range(
                min=50, max=1000
            ),
            cv.Optional(CONF_SPEED_3_FREQUENCY, default=400): cv.int_range(
                min=50, max=1000
            ),
            cv.Optional(CONF_SPEED_4_FREQUENCY, default=550): cv.int_range(
                min=50, max=1000
            ),
            cv.Optional(CONF_WET_START_DELAY, default="8s"):
                cv.positive_time_period_milliseconds,
            cv.Optional(CONF_FILTER_DRY_DURATION, default="55min"):
                cv.positive_time_period_milliseconds,
            cv.Optional(CONF_AUTOMATIC_FILTER_DRYING, default=True): cv.boolean,
            cv.Optional(CONF_TACH_SAFETY, default=True): cv.boolean,
            cv.Optional(CONF_TACH_STARTUP_TIMEOUT, default="15s"):
                cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MINIMUM_TACH_PULSES_PER_MINUTE, default=30):
                cv.float_range(min=1, max=100000),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_frequencies,
)


async def to_code(config):
    # A single C++ instance owns every power/control output. Keeping the
    # low-level sequence in one component avoids unsynchronized YAML GPIO
    # actions during startup, faults, filter drying, or shutdown.
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    cg.add(
        var.set_speed_frequencies(
            config[CONF_SPEED_1_FREQUENCY],
            config[CONF_SPEED_2_FREQUENCY],
            config[CONF_SPEED_3_FREQUENCY],
            config[CONF_SPEED_4_FREQUENCY],
        )
    )
    cg.add(
        var.set_wet_start_delay(
            config[CONF_WET_START_DELAY].total_milliseconds
        )
    )
    cg.add(
        var.set_filter_dry_duration(
            config[CONF_FILTER_DRY_DURATION].total_milliseconds
        )
    )
    cg.add(
        var.set_automatic_filter_drying(
            config[CONF_AUTOMATIC_FILTER_DRYING]
        )
    )
    cg.add(var.set_tach_safety(config[CONF_TACH_SAFETY]))
    cg.add(
        var.set_tach_startup_timeout(
            config[CONF_TACH_STARTUP_TIMEOUT].total_milliseconds
        )
    )
    cg.add(
        var.set_minimum_tach_pulses_per_minute(
            config[CONF_MINIMUM_TACH_PULSES_PER_MINUTE]
        )
    )
