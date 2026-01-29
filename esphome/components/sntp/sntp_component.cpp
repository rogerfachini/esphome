#include "sntp_component.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
#include "esp_sntp.h"
#elif USE_ESP8266
#include "sntp.h"
#else
#include "lwip/apps/sntp.h"
#endif

namespace esphome {
namespace sntp {

static const char *const TAG = "sntp";

#if defined(USE_ESP32)
SNTPComponent *SNTPComponent::instance = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif

void SNTPComponent::setup() {
#if defined(USE_ESP32)
  SNTPComponent::instance = this;
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_set_sync_mode(this->smooth_sync_ ? SNTP_SYNC_MODE_SMOOTH : SNTP_SYNC_MODE_IMMED);
  
  size_t i = 0;
  for (auto &server : this->servers_) {
    esp_sntp_setservername(i++, server);
  }
  esp_sntp_set_sync_interval(this->get_update_interval());
  esp_sntp_set_time_sync_notification_cb([](struct timeval *tv) {
    if (SNTPComponent::instance != nullptr) {
      SNTPComponent::instance->defer([]() { SNTPComponent::instance->time_synced(); });
    }
  });
  esp_sntp_init();
#else
  sntp_stop();
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_set_sync_mode(this->smooth_sync_ ? SNTP_SYNC_MODE_SMOOTH : SNTP_SYNC_MODE_IMMED);

  size_t i = 0;
  for (auto &server : this->servers_) {
    sntp_setservername(i++, server);
  }

#if defined(USE_ESP8266)
  settimeofday_cb([this](bool from_sntp) {
    if (from_sntp)
      this->time_synced();
  });
#endif

  sntp_init();
#endif
}
void SNTPComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SNTP Time:");
  size_t i = 0;
  for (auto &server : this->servers_) {
    ESP_LOGCONFIG(TAG, "  Server %zu: '%s'", i++, server);
  }
  ESP_LOGCONFIG(TAG, "  Smooth Sync: %s", YESNO(this->smooth_sync_));
  RealTimeClock::dump_config();
}
void SNTPComponent::update() {
#if !defined(USE_ESP32)
  // Some platforms currently cannot set the sync interval at runtime so we need
  // to do the re-sync by hand for now.
  if (sntp_enabled()) {
    sntp_stop();
    this->has_time_ = false;
    sntp_init();
  }
#endif
}
void SNTPComponent::loop() {
// The loop is used to infer whether we have valid time on platforms where we
// cannot tell whether SNTP has succeeded, and to poll for status when smooth
// time sync is enabled.
// One limitation of this approach is that we cannot tell if it was the SNTP
// component that set the time.
// ESP-IDF and ESP8266 use callbacks from the SNTP task to trigger the
// `on_time_sync` trigger on successful sync events.
#if defined(USE_ESP32) || defined(USE_ESP8266)
  // Keep loop enabled when smooth sync is active on ESP32/8266 platforms,
  // otherwise disable the loop
  if (!this->smooth_sync_ || !this->is_syncing_) {
    this->disable_loop();
  }
#endif

  if (this->has_time_ && !this->is_syncing_)
    return;

  // Poll for smooth sync status
  if (this->smooth_sync_ && this->is_syncing_) {
#if defined(USE_ESP32)
    switch (esp_sntp_get_sync_status()) {
#else
    switch (sntp_get_sync_status()) {
#endif    
      case SNTP_SYNC_STATUS_COMPLETED:
        this->smooth_time_synced();
        break;
      case SNTP_SYNC_STATUS_RESET:
        this->is_syncing_ = false;
        break;
    }
    return;
  }

  this->time_synced();
}

void SNTPComponent::time_synced() {
  // Handle time sync completion for immediate mode, and start of smooth time sync in smooth mode 
  auto time = this->now();
  this->has_time_ = time.is_valid();
  if (!this->has_time_)
    return;

  if (!this->smooth_sync_) {
    ESP_LOGD(TAG, "Synchronized time: %04d-%02d-%02d %02d:%02d:%02d", time.year, time.month, time.day_of_month, 
             time.hour, time.minute, time.second);
  }else {
    ESP_LOGD(TAG, "Smooth time synchronization started");
  }

  this->time_sync_callback_.call();

  // Enable polling loop for smooth sync
  if (this->smooth_sync_) {
    this->is_syncing_ = true;
    this->enable_loop();
  }
}

void SNTPComponent::smooth_time_synced() {
  // Handle smooth time sync completion
  auto time = this->now();
  this->has_time_ = time.is_valid();
  
  if (!this->has_time_) {
    this->is_syncing_ = false;
    return;
  }
  
  ESP_LOGD(TAG, "Smooth time synchronization completed: %04d-%02d-%02d %02d:%02d:%02d", time.year, time.month,
           time.day_of_month, time.hour, time.minute, time.second);
  
  this->is_syncing_ = false;
  this->smooth_time_sync_callback_.call();
}

}  // namespace sntp
}  // namespace esphome
