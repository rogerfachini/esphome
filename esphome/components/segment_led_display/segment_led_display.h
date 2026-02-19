#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/time.h"
#include "esphome/components/output/binary_output.h"

#include <vector>

namespace esphome {
namespace segment_led_display {

enum DisplayType : uint8_t {
  SEGMENT_7 = 7,
  SEGMENT_9 = 9,
  SEGMENT_14 = 14,
  SEGMENT_16 = 16,
};

class SegmentLEDDisplay;

using segment_writer_t = std::function<void(SegmentLEDDisplay &)>;

class SegmentLEDDisplay : public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

  /// Push the display buffer to the hardware
  void display();

  // Configuration methods
  void set_display_type(DisplayType type) { this->display_type_ = type; }
  void set_segment_pins(const std::vector<GPIOPin *> &pins) { this->segment_pins_ = pins; }
  void set_digit_select_pins(const std::vector<output::BinaryOutput *> &pins) { this->digit_select_pins_ = pins; }
  void set_decimal_point_pins(const std::vector<GPIOPin *> &pins) { this->decimal_point_pins_ = pins; }
  void set_intensity(uint8_t intensity) { this->intensity_ = intensity; }
  void set_writer(segment_writer_t &&writer) { this->writer_ = writer; }

  // Display control methods
  /// Print string to display starting at position 0
  uint8_t print(const char *str);
  /// Print string to display starting at specified position
  uint8_t print(uint8_t pos, const char *str);

  /// Print formatted string to display starting at position 0
  uint8_t printf(const char *format, ...) __attribute__((format(printf, 2, 3)));
  /// Print formatted string to display starting at specified position
  uint8_t printf(uint8_t pos, const char *format, ...) __attribute__((format(printf, 3, 4)));

  /// Print formatted time to display starting at position 0
  uint8_t strftime(const char *format, ESPTime time) __attribute__((format(strftime, 2, 0)));
  /// Print formatted time to display starting at specified position
  uint8_t strftime(uint8_t pos, const char *format, ESPTime time) __attribute__((format(strftime, 3, 0)));

  /// Clear the display buffer
  void clear();

  /// Set decimal point state for a specific digit (dp_index: 0 or 1 for dual DP)
  void set_decimal_point(uint8_t digit_pos, uint8_t dp_index, bool state);

  /// Set colon state for a specific digit (requires 2 decimal point pins)
  void set_colon(uint8_t digit_pos, bool state);

  /// Get the number of digits
  uint8_t get_num_digits() const { return this->digit_select_pins_.size(); }

 protected:
  void display_digit_(uint8_t digit_index);
  uint32_t encode_character_(char c);
  void write_segments_(uint32_t pattern);
  void write_decimal_points_(uint8_t digit_index);

  DisplayType display_type_{SEGMENT_7};
  std::vector<GPIOPin *> segment_pins_;
  std::vector<output::BinaryOutput *> digit_select_pins_;
  std::vector<GPIOPin *> decimal_point_pins_;
  uint8_t intensity_{255};
  segment_writer_t writer_{};

  // Display buffer: stores segment pattern for each digit
  std::vector<uint32_t> display_buffer_;
  // Decimal point buffer: stores DP state for each digit (2 bits per digit)
  std::vector<uint8_t> dp_buffer_;

};

}  // namespace segment_led_display
}  // namespace esphome
