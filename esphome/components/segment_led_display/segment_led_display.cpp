#include "segment_led_display.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"


// Conditional includes based on display type
#if defined(USE_SEGMENT_7_PATTERNS)
#include "segment_patterns_7.h"
#endif
#if defined(USE_SEGMENT_9_PATTERNS)
#include "segment_patterns_9.h"
#endif
#if defined(USE_SEGMENT_14_PATTERNS)
#include "segment_patterns_14.h"
#endif
#if defined(USE_SEGMENT_16_PATTERNS)
#include "segment_patterns_16.h"
#endif

#include <cstdarg>
#include <cstdio>

namespace esphome {
namespace segment_led_display {

static const char *const TAG = "segment_led_display";

void SegmentLEDDisplay::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Segment LED Display...");

  // Initialize segment pins
  for (auto *pin : this->segment_pins_) {
    pin->setup();
    pin->pin_mode(gpio::FLAG_OUTPUT);
    pin->digital_write(false);  // Off state
  }

  // Initialize digit select pins (turn off all digits)
  for (auto *output : this->digit_select_pins_) {
    output->turn_off();  // Deselected
  }

  // Initialize decimal point pins
  for (auto *pin : this->decimal_point_pins_) {
    pin->setup();
    pin->pin_mode(gpio::FLAG_OUTPUT);
    pin->digital_write(false);  // Off state
  }

  // Initialize display buffer
  this->display_buffer_.resize(this->digit_select_pins_.size(), 0);
  this->dp_buffer_.resize(this->digit_select_pins_.size(), 0);

  ESP_LOGCONFIG(TAG, "  Display Type: %d-segment", static_cast<int>(this->display_type_));
  ESP_LOGCONFIG(TAG, "  Number of Digits: %zu", this->digit_select_pins_.size());
  ESP_LOGCONFIG(TAG, "  Number of Segment Pins: %zu", this->segment_pins_.size());
  ESP_LOGCONFIG(TAG, "  Number of Decimal Point Pins: %zu", this->decimal_point_pins_.size());
}

void SegmentLEDDisplay::loop() {}

void SegmentLEDDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "Segment LED Display:");
  ESP_LOGCONFIG(TAG, "  Display Type: %d-segment", static_cast<int>(this->display_type_));
  ESP_LOGCONFIG(TAG, "  Digits: %zu", this->digit_select_pins_.size());
}

void SegmentLEDDisplay::update() {
  // Clear buffer
  for (auto &val : this->display_buffer_) {
    val = 0;
  }
  for (auto &val : this->dp_buffer_) {
    val = 0;
  }

  // Call user-defined writer function if set
  if (this->writer_) {
    this->writer_(*this);
  }

  this->display();
}

void SegmentLEDDisplay::display() {
  for (uint8_t i = 0; i < this->digit_select_pins_.size(); i++) {
    // Turn off all digits
    for (size_t j = 0; j < this->digit_select_pins_.size(); j++) {
      this->digit_select_pins_[j]->turn_off();
    }

    // Write segment pattern for this digit
    if (i < this->display_buffer_.size()) {
      this->write_segments_(this->display_buffer_[i]);
      this->write_decimal_points_(i);
    }

    // Turn on only this digit
    this->digit_select_pins_[i]->turn_on();
    
    // Delay for multiplexing - allows digit to be visible
    delayMicroseconds(2000);  // 2ms per digit
  }
}

float SegmentLEDDisplay::get_setup_priority() const { return setup_priority::PROCESSOR; }


uint32_t SegmentLEDDisplay::encode_character_(char c) {
  // Ensure character is in valid ASCII range
  uint8_t char_code = static_cast<uint8_t>(c);
  if (char_code >= 128) {
    return 0;
  }

  // Select pattern table based on display type (conditional compilation)
#if defined(USE_SEGMENT_7_PATTERNS)
  if (this->display_type_ == SEGMENT_7) {
    return SEGMENT_7_PATTERNS[char_code];
  }
#endif
#if defined(USE_SEGMENT_9_PATTERNS)
  if (this->display_type_ == SEGMENT_9) {
    return SEGMENT_9_PATTERNS[char_code];
  }
#endif
#if defined(USE_SEGMENT_14_PATTERNS)
  if (this->display_type_ == SEGMENT_14) {
    return SEGMENT_14_PATTERNS[char_code];
  }
#endif
#if defined(USE_SEGMENT_16_PATTERNS)
  if (this->display_type_ == SEGMENT_16) {
    return SEGMENT_16_PATTERNS[char_code];
  }
#endif
  
  // No pattern table available for this display type
  ESP_LOGW(TAG, "No pattern table available for display type %d", static_cast<int>(this->display_type_));
  return 0;
}

void SegmentLEDDisplay::write_segments_(uint32_t pattern) {
  // Write each segment pin according to the pattern
  for (size_t i = 0; i < this->segment_pins_.size(); i++) {
    bool segment_on = (pattern >> i) & 0x01;
    this->segment_pins_[i]->digital_write(segment_on);
  }
}

void SegmentLEDDisplay::write_decimal_points_(uint8_t digit_index) {
  if (digit_index >= this->dp_buffer_.size()) {
    return;
  }

  uint8_t dp_state = this->dp_buffer_[digit_index];

  // Write decimal point pins (up to 2)
  for (size_t i = 0; i < this->decimal_point_pins_.size(); i++) {
    bool dp_on = (dp_state >> i) & 0x01;
    this->decimal_point_pins_[i]->digital_write(dp_on);
  }
}

uint8_t SegmentLEDDisplay::print(const char *str) { return this->print(0, str); }

uint8_t SegmentLEDDisplay::print(uint8_t pos, const char *str) {
  uint8_t digit = pos;
  bool last_was_dp = false;

  for (const char *p = str; *p != '\0' && digit < this->display_buffer_.size(); p++) {
    // Handle decimal point - merge with previous digit
    if (*p == '.' && digit > pos && !last_was_dp && this->decimal_point_pins_.size() > 0) {
      this->dp_buffer_[digit - 1] |= 0x01;  // Set first DP bit
      last_was_dp = true;
      continue;
    }

    // Handle colon - use dual DP if available
    if (*p == ':' && this->decimal_point_pins_.size() >= 2) {
      if (digit > pos) {
        this->dp_buffer_[digit - 1] |= 0x03;  // Set both DP bits
      }
      last_was_dp = true;
      continue;
    }

    // Regular character
    this->display_buffer_[digit] = this->encode_character_(*p);
    digit++;
    last_was_dp = false;
  }

  return digit - pos;
}

uint8_t SegmentLEDDisplay::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[64];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    return this->print(buffer);
  return 0;
}

uint8_t SegmentLEDDisplay::printf(uint8_t pos, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[64];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    return this->print(pos, buffer);
  return 0;
}

uint8_t SegmentLEDDisplay::strftime(const char *format, ESPTime time) { return this->strftime(0, format, time); }

uint8_t SegmentLEDDisplay::strftime(uint8_t pos, const char *format, ESPTime time) {
  char buffer[64];
  size_t ret = time.strftime(buffer, sizeof(buffer), format);
  if (ret > 0)
    return this->print(pos, buffer);
  return 0;
}

void SegmentLEDDisplay::clear() {
  for (auto &val : this->display_buffer_) {
    val = 0;
  }
  for (auto &val : this->dp_buffer_) {
    val = 0;
  }
}

void SegmentLEDDisplay::set_decimal_point(uint8_t digit_pos, uint8_t dp_index, bool state) {
  if (digit_pos >= this->dp_buffer_.size() || dp_index >= this->decimal_point_pins_.size()) {
    return;
  }

  if (state) {
    this->dp_buffer_[digit_pos] |= (1 << dp_index);
  } else {
    this->dp_buffer_[digit_pos] &= ~(1 << dp_index);
  }
}

void SegmentLEDDisplay::set_colon(uint8_t digit_pos, bool state) {
  if (this->decimal_point_pins_.size() < 2) {
    ESP_LOGW(TAG, "Colon requires 2 decimal point pins, but only %zu configured", this->decimal_point_pins_.size());
    return;
  }

  if (digit_pos >= this->dp_buffer_.size()) {
    return;
  }

  if (state) {
    this->dp_buffer_[digit_pos] = 0x03;  // Set both DP bits
  } else {
    this->dp_buffer_[digit_pos] = 0x00;  // Clear both DP bits
  }
}

}  // namespace segment_led_display
}  // namespace esphome
