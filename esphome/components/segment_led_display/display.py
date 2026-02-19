"""Python configuration schema for segment_led_display component."""
from esphome import pins
from esphome.components import output
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INTENSITY,
    CONF_LAMBDA,
)

from . import segment_led_display_ns

CONF_SEGMENT_PINS = "segment_pins"
CONF_DIGIT_SELECT_PINS = "digit_select_pins"
CONF_DECIMAL_POINT_PINS = "decimal_point_pins"
CONF_DISPLAY_TYPE = "display_type"

SegmentLEDDisplay = segment_led_display_ns.class_(
    "SegmentLEDDisplay", cg.PollingComponent
)
SegmentLEDDisplayRef = SegmentLEDDisplay.operator("ref")

DisplayType = segment_led_display_ns.enum("DisplayType")
DISPLAY_TYPES = {
    "7_SEGMENT": DisplayType.SEGMENT_7,
    "9_SEGMENT": DisplayType.SEGMENT_9,
    "14_SEGMENT": DisplayType.SEGMENT_14,
    "16_SEGMENT": DisplayType.SEGMENT_16,
}

DISPLAY_TYPE_PIN_COUNTS = {
    "7_SEGMENT": 7,
    "9_SEGMENT": 9,
    "14_SEGMENT": 14,
    "16_SEGMENT": 16,
}


def validate_segment_pin_count(config):
    """Validate segment pin count matches display type."""
    display_type = config[CONF_DISPLAY_TYPE]
    expected_pins = DISPLAY_TYPE_PIN_COUNTS[display_type]
    actual_pins = len(config[CONF_SEGMENT_PINS])

    if actual_pins != expected_pins:
        raise cv.Invalid(
            f"Display type '{display_type}' requires exactly {expected_pins} "
            f"segment pins, but {actual_pins} were provided"
        )
    return config


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SegmentLEDDisplay),
            cv.Required(CONF_DISPLAY_TYPE): cv.enum(DISPLAY_TYPES, upper=True),
            cv.Required(CONF_SEGMENT_PINS): cv.ensure_list(
                pins.gpio_output_pin_schema
            ),
            cv.Required(CONF_DIGIT_SELECT_PINS): cv.All(
                cv.ensure_list(cv.use_id(output.BinaryOutput)), cv.Length(min=1, max=16)
            ),
            cv.Optional(CONF_DECIMAL_POINT_PINS): cv.All(
                cv.ensure_list(pins.gpio_output_pin_schema), cv.Length(min=0, max=2)
            ),
            cv.Optional(CONF_INTENSITY, default=255): cv.int_range(min=0, max=255),
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .add_extra(validate_segment_pin_count)
)


async def to_code(config):
    """Generate C++ code for the component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set display type and add corresponding define for conditional compilation
    display_type = config[CONF_DISPLAY_TYPE]
    cg.add(var.set_display_type(display_type))

    # Add compiler define to include only the needed pattern table
    if display_type == "7_SEGMENT":
        cg.add_define("USE_SEGMENT_7_PATTERNS")
    elif display_type == "9_SEGMENT":
        cg.add_define("USE_SEGMENT_9_PATTERNS")
    elif display_type == "14_SEGMENT":
        cg.add_define("USE_SEGMENT_14_PATTERNS")
    elif display_type == "16_SEGMENT":
        cg.add_define("USE_SEGMENT_16_PATTERNS")

    # Process segment pins
    segment_pins = []
    for pin_conf in config[CONF_SEGMENT_PINS]:
        pin = await cg.gpio_pin_expression(pin_conf)
        segment_pins.append(pin)
    cg.add(var.set_segment_pins(segment_pins))

    # Process digit select pins
    digit_pins = []
    for pin_conf in config[CONF_DIGIT_SELECT_PINS]:
        pin = await cg.get_variable(pin_conf)
        digit_pins.append(pin)
    cg.add(var.set_digit_select_pins(digit_pins))

    # Process optional decimal point pins
    if CONF_DECIMAL_POINT_PINS in config:
        dp_pins = []
        for pin_conf in config[CONF_DECIMAL_POINT_PINS]:
            pin = await cg.gpio_pin_expression(pin_conf)
            dp_pins.append(pin)
        cg.add(var.set_decimal_point_pins(dp_pins))

    # Set intensity
    cg.add(var.set_intensity(config[CONF_INTENSITY]))

    # Add lambda if provided
    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(SegmentLEDDisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
