#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/time/real_time_clock.h"
#include <array>

namespace esphome {
namespace sntp {

class SmoothSyncTrigger;

// Server count is calculated at compile time by Python codegen
// SNTP_SERVER_COUNT will always be defined

/// The SNTP component allows you to configure local timekeeping via Simple Network Time Protocol.
///
/// \note
/// The C library (newlib) available on ESPs only supports TZ strings that specify an offset and DST info;
/// you cannot specify zone names or paths to zoneinfo files.
/// \see https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
class SNTPComponent : public time::RealTimeClock {
 public:
  SNTPComponent(const std::array<const char *, SNTP_SERVER_COUNT> &servers, bool smooth_sync)
      : servers_(servers), smooth_sync_(smooth_sync) {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BEFORE_CONNECTION; }

  void update() override;
  void loop() override;

  void time_synced();
  void smooth_time_synced();
  
  void add_on_smooth_time_sync_callback(std::function<void()> &&callback) {
    this->smooth_time_sync_callback_.add(std::move(callback));
  }

 protected:
  // Store const char pointers to string literals
  // ESP8266: strings in rodata (RAM), but avoids std::string overhead (~24 bytes each)
  // Other platforms: strings in flash
  std::array<const char *, SNTP_SERVER_COUNT> servers_;
  bool smooth_sync_;
  bool has_time_{false};
  bool is_syncing_{false};
  CallbackManager<void()> smooth_time_sync_callback_;

#if defined(USE_ESP32)
 private:
  static SNTPComponent *instance;
#endif
  
  friend class SmoothSyncTrigger;
};

class SmoothSyncTrigger : public Trigger<>, public Component {
 public:
  explicit SmoothSyncTrigger(SNTPComponent *parent) {
    parent->add_on_smooth_time_sync_callback([this]() { this->trigger(); });
  }
};

}  // namespace sntp
}  // namespace esphome
