#include "switchbot_humidifier.h"

#include <algorithm>
#include <cmath>

#include "esp_err.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::switchbot_humidifier {

static const char *const TAG = "switchbot_humidifier";
// Change this when runtime behavior changes; documentation-only edits do not
// require a new firmware revision.
static const char *const PORT_VERSION = "2026.07.23-r3";

// High-confidence map recovered from the W3902310 V07 stock image.
static constexpr gpio_num_t PIN_BLOWER_ENABLE = GPIO_NUM_2;
static constexpr gpio_num_t PIN_PANEL_CLOCK = GPIO_NUM_4;
static constexpr gpio_num_t PIN_WET_AUXILIARY_A = GPIO_NUM_12;
static constexpr gpio_num_t PIN_PANEL_RAIL = GPIO_NUM_13;
static constexpr gpio_num_t PIN_AUTO_REFILL_PWM = GPIO_NUM_14;
static constexpr gpio_num_t PIN_PANEL_STROBE = GPIO_NUM_15;
static constexpr gpio_num_t PIN_BUZZER = GPIO_NUM_16;
static constexpr gpio_num_t PIN_BLOWER_SPEED = GPIO_NUM_17;
static constexpr gpio_num_t PIN_PANEL_RAIL_INVERTED = GPIO_NUM_21;
static constexpr gpio_num_t PIN_WET_AUXILIARY_B = GPIO_NUM_22;
static constexpr gpio_num_t PIN_AUTO_REFILL_SENSE = GPIO_NUM_25;
static constexpr gpio_num_t PIN_PANEL_DATA = GPIO_NUM_26;
static constexpr gpio_num_t PIN_UNUSED_OUTPUT = GPIO_NUM_32;

static constexpr ledc_mode_t HIGH_SPEED = LEDC_HIGH_SPEED_MODE;
static constexpr ledc_mode_t LOW_SPEED = LEDC_LOW_SPEED_MODE;
static constexpr uint32_t DUTY_OFF = 0;
static constexpr uint32_t DUTY_HALF = 512;
static constexpr uint32_t DUTY_STOCK_FULL = 1024;

// Unsigned subtraction keeps millis()-based deadlines correct across the
// approximately 49-day uint32_t wrap.
static bool deadline_reached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

void SwitchBotHumidifier::set_speed_frequencies(uint32_t speed_1, uint32_t speed_2,
                                                 uint32_t speed_3, uint32_t speed_4) {
  this->speed_frequencies_[0] = speed_1;
  this->speed_frequencies_[1] = speed_2;
  this->speed_frequencies_[2] = speed_3;
  this->speed_frequencies_[3] = speed_4;
}

bool SwitchBotHumidifier::configure_output_pin_(gpio_num_t pin, uint32_t initial_level) {
  gpio_config_t config{};
  config.pin_bit_mask = 1ULL << static_cast<uint8_t>(pin);
  config.mode = GPIO_MODE_OUTPUT;
  config.pull_up_en = GPIO_PULLUP_DISABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_DISABLE;
  return gpio_set_level(pin, initial_level) == ESP_OK && gpio_config(&config) == ESP_OK &&
         gpio_set_level(pin, initial_level) == ESP_OK;
}

bool SwitchBotHumidifier::configure_input_pin_(gpio_num_t pin) {
  gpio_config_t config{};
  config.pin_bit_mask = 1ULL << static_cast<uint8_t>(pin);
  config.mode = GPIO_MODE_INPUT;
  config.pull_up_en = GPIO_PULLUP_DISABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_DISABLE;
  return gpio_config(&config) == ESP_OK;
}

bool SwitchBotHumidifier::configure_timer_(ledc_mode_t mode, ledc_timer_t timer,
                                            uint32_t frequency) {
  ledc_timer_config_t config{};
  config.speed_mode = mode;
  config.duty_resolution = LEDC_TIMER_10_BIT;
  config.timer_num = timer;
  config.freq_hz = frequency;
  config.clk_cfg = LEDC_AUTO_CLK;
  const esp_err_t error = ledc_timer_config(&config);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "LEDC timer setup failed: %s", esp_err_to_name(error));
    return false;
  }
  return true;
}

bool SwitchBotHumidifier::configure_channel_(gpio_num_t pin, ledc_mode_t mode,
                                              ledc_channel_t channel, ledc_timer_t timer,
                                              uint32_t duty) {
  ledc_channel_config_t config{};
  config.gpio_num = pin;
  config.speed_mode = mode;
  config.channel = channel;
  config.intr_type = LEDC_INTR_DISABLE;
  config.timer_sel = timer;
  config.duty = duty;
  config.hpoint = 0;
  const esp_err_t error = ledc_channel_config(&config);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "LEDC channel setup failed for GPIO%u: %s", static_cast<unsigned>(pin),
             esp_err_to_name(error));
    return false;
  }
  return true;
}

bool SwitchBotHumidifier::write_duty_(ledc_mode_t mode, ledc_channel_t channel,
                                       uint32_t duty) {
  esp_err_t error = ledc_set_duty(mode, channel, duty);
  if (error == ESP_OK)
    error = ledc_update_duty(mode, channel);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "LEDC duty update failed: %s", esp_err_to_name(error));
    return false;
  }
  return true;
}

bool SwitchBotHumidifier::configure_hardware_() {
  // Put every power/control output into its safe state before attaching LEDC.
  if (!this->configure_output_pin_(PIN_BLOWER_ENABLE, 0) ||
      !this->configure_output_pin_(PIN_WET_AUXILIARY_A, 0) ||
      !this->configure_output_pin_(PIN_WET_AUXILIARY_B, 0) ||
      !this->configure_output_pin_(PIN_UNUSED_OUTPUT, 0) ||
      !this->configure_output_pin_(PIN_BUZZER, 0) ||
      !this->configure_output_pin_(PIN_AUTO_REFILL_PWM, 0) ||
      !this->configure_output_pin_(PIN_BLOWER_SPEED, 0) ||
      !this->configure_output_pin_(PIN_PANEL_RAIL, 0) ||
      !this->configure_output_pin_(PIN_PANEL_RAIL_INVERTED, 1) ||
      !this->configure_input_pin_(PIN_AUTO_REFILL_SENSE)) {
    ESP_LOGE(TAG, "GPIO fail-safe initialization failed");
    return false;
  }

  // AIP/TM1668-compatible panel bus: idle with STB high and CLK low.
  if (!this->configure_output_pin_(PIN_PANEL_STROBE, 1) ||
      !this->configure_output_pin_(PIN_PANEL_CLOCK, 0) ||
      !this->configure_output_pin_(PIN_PANEL_DATA, 0)) {
    ESP_LOGE(TAG, "Panel GPIO initialization failed");
    return false;
  }

  // These timer/channel assignments exactly match stock initialization.
  if (!this->configure_timer_(HIGH_SPEED, LEDC_TIMER_0, 40000) ||
      !this->configure_channel_(PIN_PANEL_RAIL, HIGH_SPEED, LEDC_CHANNEL_0,
                                LEDC_TIMER_0, DUTY_OFF) ||
      !this->configure_channel_(PIN_PANEL_RAIL_INVERTED, HIGH_SPEED, LEDC_CHANNEL_7,
                                LEDC_TIMER_0, DUTY_STOCK_FULL) ||
      !this->configure_timer_(HIGH_SPEED, LEDC_TIMER_1, 4000) ||
      !this->configure_channel_(PIN_BUZZER, HIGH_SPEED, LEDC_CHANNEL_6, LEDC_TIMER_1,
                                DUTY_OFF) ||
      !this->configure_timer_(HIGH_SPEED, LEDC_TIMER_2, 20000) ||
      !this->configure_channel_(PIN_AUTO_REFILL_PWM, HIGH_SPEED, LEDC_CHANNEL_1,
                                LEDC_TIMER_2, DUTY_OFF) ||
      !this->configure_timer_(LOW_SPEED, LEDC_TIMER_2, this->speed_frequencies_[0]) ||
      !this->configure_channel_(PIN_BLOWER_SPEED, LOW_SPEED, LEDC_CHANNEL_2,
                                LEDC_TIMER_2, DUTY_OFF)) {
    return false;
  }

  return true;
}

void SwitchBotHumidifier::setup() {
  if (!this->configure_hardware_()) {
    this->stop_all_hardware_();
    this->mark_failed();
    return;
  }

  this->initialized_ = true;
  this->set_panel_brightness(static_cast<uint8_t>(PanelBrightness::NORMAL));
  this->update_panel_();
  ESP_LOGI(TAG, "Ready: stock blower path GPIO2 + GPIO17; wet outputs are DISARMED");
}

void SwitchBotHumidifier::dump_config() {
  ESP_LOGCONFIG(TAG, "SwitchBot Evaporative Humidifier 2 (W3902310/V07):");
  ESP_LOGCONFIG(TAG, "  Port version: %s", PORT_VERSION);
  ESP_LOGCONFIG(TAG, "  Blower: GPIO2 enable, GPIO17 50%% duty signal");
  ESP_LOGCONFIG(TAG, "  Speeds: %lu / %lu / %lu / %lu Hz",
                static_cast<unsigned long>(this->speed_frequencies_[0]),
                static_cast<unsigned long>(this->speed_frequencies_[1]),
                static_cast<unsigned long>(this->speed_frequencies_[2]),
                static_cast<unsigned long>(this->speed_frequencies_[3]));
  ESP_LOGCONFIG(TAG, "  Tach: GPIO34 (fed by pulse_counter), minimum %.1f pulses/min",
                this->minimum_tach_ppm_);
  ESP_LOGCONFIG(TAG, "  Wet start delay: %.1f s", this->wet_start_delay_ms_ / 1000.0f);
  ESP_LOGCONFIG(TAG, "  Wet auxiliaries: GPIO12 + GPIO22, startup state DISARMED");
  ESP_LOGCONFIG(TAG, "  Filter dry: speed 2 for %.1f min", this->filter_dry_duration_ms_ / 60000.0f);
  ESP_LOGCONFIG(TAG, "  Panel bus: STB GPIO15, CLK GPIO4, DIO GPIO26");
  ESP_LOGCONFIG(TAG, "  Buzzer: GPIO16, bounded one-shot only");
  ESP_LOGCONFIG(TAG, "  Auto-refill/S10 outputs: disabled");
}

void SwitchBotHumidifier::on_shutdown() {
  this->requested_humidifying_ = false;
  this->filter_drying_ = false;
  this->stop_all_hardware_();
}

void SwitchBotHumidifier::write_state(float state) {
  if (!std::isfinite(state))
    return;

  if (state <= 0.001f) {
    const bool should_dry = this->initialized_ && this->automatic_filter_drying_ &&
                            this->wet_started_this_cycle_;
    this->requested_humidifying_ = false;
    this->set_wet_auxiliaries_(false);
    if (should_dry)
      this->start_filter_dry();
    else if (!this->filter_drying_)
      this->stop_blower_();
    return;
  }

  uint8_t speed = static_cast<uint8_t>(std::lround(state * 4.0f));
  speed = std::clamp<uint8_t>(speed, 1, 4);

  if (!this->initialized_)
    return;

  if (this->filter_drying_) {
    this->filter_drying_ = false;
    this->stop_blower_();
  }

  this->requested_humidifying_ = true;
  if (!this->blower_running_) {
    this->wet_started_this_cycle_ = false;
    this->tach_fault_ = false;
    this->start_blower_(speed);
  } else if (speed != this->current_speed_) {
    this->start_blower_(speed, false);
  }
}

bool SwitchBotHumidifier::start_blower_(uint8_t speed, bool restart_tach_window) {
  if (!this->initialized_ || speed < 1 || speed > 4)
    return false;

  const uint32_t requested_frequency = this->speed_frequencies_[speed - 1];
  // ESP-IDF 5.5 returns esp_err_t here. ESP_OK is zero and must not be
  // mistaken for a measured frequency; read the configured timer separately.
  const esp_err_t frequency_error =
      ledc_set_freq(LOW_SPEED, LEDC_TIMER_2, requested_frequency);
  if (frequency_error != ESP_OK) {
    ESP_LOGE(TAG, "Could not set blower speed frequency to %lu Hz: %s",
             static_cast<unsigned long>(requested_frequency),
             esp_err_to_name(frequency_error));
    this->stop_blower_();
    this->tach_fault_ = true;
    this->requested_humidifying_ = false;
    this->filter_drying_ = false;
    return false;
  }
  const uint32_t actual_frequency =
      ledc_get_freq(LOW_SPEED, LEDC_TIMER_2);
  if (actual_frequency == 0) {
    ESP_LOGE(TAG, "Could not read back blower speed frequency");
    this->stop_blower_();
    this->tach_fault_ = true;
    this->requested_humidifying_ = false;
    this->filter_drying_ = false;
    return false;
  }

  if (!this->write_duty_(LOW_SPEED, LEDC_CHANNEL_2, DUTY_HALF) ||
      gpio_set_level(PIN_BLOWER_ENABLE, 1) != ESP_OK) {
    ESP_LOGE(TAG, "Could not start blower");
    this->stop_blower_();
    this->tach_fault_ = true;
    this->requested_humidifying_ = false;
    this->filter_drying_ = false;
    return false;
  }

  this->blower_running_ = true;
  this->current_speed_ = speed;
  if (restart_tach_window) {
    this->blower_started_ms_ = millis();
    this->tach_valid_ = false;
  }
  ESP_LOGI(TAG, "Blower started: speed %u, requested %lu Hz, actual %lu Hz", speed,
           static_cast<unsigned long>(requested_frequency),
           static_cast<unsigned long>(actual_frequency));
  return true;
}

void SwitchBotHumidifier::stop_blower_() {
  if (this->initialized_)
    this->write_duty_(LOW_SPEED, LEDC_CHANNEL_2, DUTY_OFF);
  gpio_set_level(PIN_BLOWER_ENABLE, 0);
  this->blower_running_ = false;
  this->current_speed_ = 0;
  this->tach_valid_ = false;
}

void SwitchBotHumidifier::set_wet_auxiliaries_(bool enabled) {
  if (enabled && !this->initialized_)
    return;
  if (enabled == this->wet_auxiliaries_active_)
    return;

  const bool success_a = gpio_set_level(PIN_WET_AUXILIARY_A, enabled ? 1 : 0) == ESP_OK;
  const bool success_b = gpio_set_level(PIN_WET_AUXILIARY_B, enabled ? 1 : 0) == ESP_OK;
  if (!success_a || !success_b) {
    gpio_set_level(PIN_WET_AUXILIARY_A, 0);
    gpio_set_level(PIN_WET_AUXILIARY_B, 0);
    this->wet_auxiliaries_active_ = false;
    ESP_LOGE(TAG, "Wet auxiliary GPIO update failed; both outputs forced off");
    return;
  }

  this->wet_auxiliaries_active_ = enabled;
  if (enabled) {
    this->wet_started_this_cycle_ = true;
    ESP_LOGI(TAG, "Wet auxiliaries enabled after stock startup delay");
  } else {
    ESP_LOGI(TAG, "Wet auxiliaries disabled");
  }
}

void SwitchBotHumidifier::update_wet_state_() {
  // Treat this as one fail-closed Boolean gate. Losing any prerequisite on a
  // later loop turns both wet outputs off immediately.
  const uint32_t now = millis();
  const bool delay_complete = this->blower_running_ &&
                              (now - this->blower_started_ms_ >= this->wet_start_delay_ms_);
  const bool water_allows_run = this->panel_sampled_ &&
                                this->water_level_ != WaterLevel::UNKNOWN &&
                                this->water_level_ != WaterLevel::EMPTY;
  const bool tach_allows_run = !this->tach_safety_ ||
                               (this->tach_valid_ && this->tach_ppm_ >= this->minimum_tach_ppm_);
  const bool should_enable = this->requested_humidifying_ && !this->filter_drying_ &&
                             this->wet_system_allowed_ && delay_complete && water_allows_run &&
                             tach_allows_run && !this->tach_fault_;
  this->set_wet_auxiliaries_(should_enable);
}

void SwitchBotHumidifier::set_tach_pulses_per_minute(float pulses) {
  if (!std::isfinite(pulses) || pulses < 0.0f)
    return;
  this->tach_ppm_ = pulses;
  this->tach_valid_ = true;
}

void SwitchBotHumidifier::update_tach_safety_() {
  if (!this->tach_safety_ || !this->blower_running_ || this->tach_fault_)
    return;

  const uint32_t now = millis();
  if (now - this->blower_started_ms_ < this->tach_startup_timeout_ms_)
    return;

  if (!this->tach_valid_ || this->tach_ppm_ < this->minimum_tach_ppm_) {
    ESP_LOGE(TAG, "Blower tach fault: %.1f pulses/min (minimum %.1f); all power outputs stopped",
             this->tach_ppm_, this->minimum_tach_ppm_);
    this->tach_fault_ = true;
    this->requested_humidifying_ = false;
    this->filter_drying_ = false;
    this->wet_started_this_cycle_ = false;
    this->set_wet_auxiliaries_(false);
    this->stop_blower_();
  }
}

void SwitchBotHumidifier::set_wet_system_allowed(bool allowed) {
  this->wet_system_allowed_ = allowed;
  if (!allowed) {
    this->set_wet_auxiliaries_(false);
    ESP_LOGW(TAG, "Wet system DISARMED");
  } else {
    ESP_LOGW(TAG, "Wet system ARMED by user; GPIO12 and GPIO22 may now energize");
  }
}

void SwitchBotHumidifier::start_filter_dry() {
  if (!this->initialized_)
    return;

  this->requested_humidifying_ = false;
  // Stock filter dry is blower-only. Home Assistant's fan entity may already
  // be off while this separate internal state keeps the physical blower on.
  this->set_wet_auxiliaries_(false);
  this->wet_started_this_cycle_ = false;
  this->filter_drying_ = true;
  this->tach_fault_ = false;
  this->filter_dry_deadline_ms_ = millis() + this->filter_dry_duration_ms_;
  if (!this->start_blower_(2)) {
    this->filter_drying_ = false;
    return;
  }
  ESP_LOGI(TAG, "Filter drying started for %.1f minutes; wet outputs remain off",
           this->filter_dry_duration_ms_ / 60000.0f);
}

void SwitchBotHumidifier::stop_filter_dry() {
  if (!this->filter_drying_)
    return;
  this->filter_drying_ = false;
  this->stop_blower_();
  ESP_LOGI(TAG, "Filter drying stopped");
}

void SwitchBotHumidifier::stop_all() {
  this->requested_humidifying_ = false;
  this->filter_drying_ = false;
  this->wet_started_this_cycle_ = false;
  this->stop_all_hardware_();
}

void SwitchBotHumidifier::stop_all_hardware_() {
  gpio_set_level(PIN_WET_AUXILIARY_A, 0);
  gpio_set_level(PIN_WET_AUXILIARY_B, 0);
  this->wet_auxiliaries_active_ = false;
  this->set_buzzer_(false);
  if (this->initialized_) {
    this->write_duty_(HIGH_SPEED, LEDC_CHANNEL_1, DUTY_OFF);
    this->write_duty_(LOW_SPEED, LEDC_CHANNEL_2, DUTY_OFF);
  }
  gpio_set_level(PIN_AUTO_REFILL_PWM, 0);
  gpio_set_level(PIN_UNUSED_OUTPUT, 0);
  gpio_set_level(PIN_BLOWER_ENABLE, 0);
  this->configure_input_pin_(PIN_AUTO_REFILL_SENSE);
  this->blower_running_ = false;
  this->current_speed_ = 0;
}

void SwitchBotHumidifier::set_buzzer_(bool enabled) {
  if (this->initialized_)
    this->write_duty_(HIGH_SPEED, LEDC_CHANNEL_6, enabled ? DUTY_HALF : DUTY_OFF);
  else if (!enabled)
    gpio_set_level(PIN_BUZZER, 0);
  this->buzzer_on_ = enabled && this->initialized_;
}

void SwitchBotHumidifier::beep(uint16_t duration_ms) {
  if (!this->initialized_)
    return;
  duration_ms = std::clamp<uint16_t>(duration_ms, 20, 500);
  this->set_buzzer_(true);
  this->buzzer_deadline_ms_ = millis() + duration_ms;
}

void SwitchBotHumidifier::set_panel_brightness(uint8_t mode) {
  mode = std::min<uint8_t>(mode, static_cast<uint8_t>(PanelBrightness::OFF));
  this->panel_brightness_ = static_cast<PanelBrightness>(mode);
  if (!this->initialized_)
    return;

  uint32_t normal_duty = 0;
  uint32_t inverted_duty = DUTY_STOCK_FULL;
  if (this->panel_brightness_ == PanelBrightness::NORMAL) {
    normal_duty = DUTY_STOCK_FULL;
    inverted_duty = DUTY_HALF;  // Effective 50% on the inverted rail.
  } else if (this->panel_brightness_ == PanelBrightness::DIM) {
    normal_duty = 27U * DUTY_STOCK_FULL / 100U;
    inverted_duty = 98U * DUTY_STOCK_FULL / 100U;  // Effective 2%.
  }
  this->write_duty_(HIGH_SPEED, LEDC_CHANNEL_0, normal_duty);
  this->write_duty_(HIGH_SPEED, LEDC_CHANNEL_7, inverted_duty);
}

void SwitchBotHumidifier::write_panel_byte_(uint8_t value) {
  gpio_set_direction(PIN_PANEL_DATA, GPIO_MODE_OUTPUT);
  for (uint8_t bit = 0; bit < 8; bit++) {
    gpio_set_level(PIN_PANEL_CLOCK, 0);
    gpio_set_level(PIN_PANEL_DATA, (value >> bit) & 0x01);
    delayMicroseconds(2);
    gpio_set_level(PIN_PANEL_CLOCK, 1);
    delayMicroseconds(2);
  }
  gpio_set_level(PIN_PANEL_CLOCK, 0);
}

bool SwitchBotHumidifier::read_panel_keys_(uint32_t &result) {
  result = 0;
  gpio_set_level(PIN_PANEL_STROBE, 0);
  delayMicroseconds(2);
  this->write_panel_byte_(0x42);  // AIP/TM1668 key-scan read command used by stock.

  gpio_set_level(PIN_PANEL_DATA, 1);
  gpio_set_direction(PIN_PANEL_DATA, GPIO_MODE_INPUT);
  for (uint8_t byte = 0; byte < 5; byte++) {
    uint8_t value = 0;
    for (uint8_t bit = 0; bit < 8; bit++) {
      gpio_set_level(PIN_PANEL_CLOCK, 0);
      delayMicroseconds(2);
      if (gpio_get_level(PIN_PANEL_DATA))
        value |= 1U << bit;
      gpio_set_level(PIN_PANEL_CLOCK, 1);
      delayMicroseconds(2);
    }
    this->panel_key_bytes_[byte] = value;

    // Stock rejects scans with either reserved high bit set.
    if ((value & 0xC0U) != 0) {
      gpio_set_level(PIN_PANEL_CLOCK, 0);
      gpio_set_level(PIN_PANEL_STROBE, 1);
      gpio_set_direction(PIN_PANEL_DATA, GPIO_MODE_OUTPUT);
      gpio_set_level(PIN_PANEL_DATA, 0);
      return false;
    }

    // FUN_400e276c compresses four key bits from each of five bytes into
    // two 10-bit banks. The water classifier consumes this 20-bit value.
    const uint8_t pair = 2U * byte;
    result |= static_cast<uint32_t>((value >> 0U) & 1U) << pair;
    result |= static_cast<uint32_t>((value >> 3U) & 1U) << (pair + 1U);
    result |= static_cast<uint32_t>((value >> 1U) & 1U) << (pair + 10U);
    result |= static_cast<uint32_t>((value >> 4U) & 1U) << (pair + 11U);
  }

  gpio_set_level(PIN_PANEL_CLOCK, 0);
  gpio_set_level(PIN_PANEL_STROBE, 1);
  gpio_set_direction(PIN_PANEL_DATA, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_PANEL_DATA, 0);
  return true;
}

void SwitchBotHumidifier::decode_water_level_() {
  // This precedence and bit mapping is a direct translation of stock FUN_400defa8.
  if ((this->panel_key_mask_ & (1UL << 7)) != 0)
    this->water_level_ = WaterLevel::EMPTY;
  else if ((this->panel_key_mask_ & (1UL << 18)) != 0)
    this->water_level_ = WaterLevel::LOW;
  else if ((this->panel_key_mask_ & (1UL << 17)) == 0)
    this->water_level_ = WaterLevel::MEDIUM;
  else
    this->water_level_ = WaterLevel::HIGH;
}

void SwitchBotHumidifier::update_panel_() {
  const uint32_t previous_mask = this->panel_key_mask_;
  const WaterLevel previous_level = this->water_level_;
  uint32_t new_mask = 0;
  if (!this->read_panel_keys_(new_mask)) {
    ESP_LOGW(TAG, "Rejected panel key scan: %02X %02X %02X %02X %02X",
             this->panel_key_bytes_[0], this->panel_key_bytes_[1],
             this->panel_key_bytes_[2], this->panel_key_bytes_[3],
             this->panel_key_bytes_[4]);
    return;
  }
  this->panel_key_mask_ = new_mask;
  this->panel_sampled_ = true;
  this->decode_water_level_();
  if (this->panel_key_mask_ != previous_mask || this->water_level_ != previous_level) {
    ESP_LOGI(TAG, "Panel keys %02X %02X %02X %02X %02X -> mask 0x%05lX; water level %s",
             this->panel_key_bytes_[0], this->panel_key_bytes_[1],
             this->panel_key_bytes_[2], this->panel_key_bytes_[3],
             this->panel_key_bytes_[4],
             static_cast<unsigned long>(this->panel_key_mask_),
             this->get_water_level_name());
  }
}

const char *SwitchBotHumidifier::get_water_level_name() const {
  switch (this->water_level_) {
    case WaterLevel::EMPTY:
      return "Empty";
    case WaterLevel::LOW:
      return "Low";
    case WaterLevel::MEDIUM:
      return "Medium";
    case WaterLevel::HIGH:
      return "High";
    case WaterLevel::UNKNOWN:
    default:
      return "Unknown";
  }
}

uint32_t SwitchBotHumidifier::get_speed_frequency() const {
  if (this->current_speed_ < 1 || this->current_speed_ > 4)
    return 0;
  return this->speed_frequencies_[this->current_speed_ - 1];
}

uint32_t SwitchBotHumidifier::get_filter_dry_remaining_seconds() const {
  if (!this->filter_drying_)
    return 0;
  const uint32_t now = millis();
  if (deadline_reached(now, this->filter_dry_deadline_ms_))
    return 0;
  return (this->filter_dry_deadline_ms_ - now + 999U) / 1000U;
}

void SwitchBotHumidifier::loop() {
  if (!this->initialized_)
    return;

  const uint32_t now = millis();
  if (now - this->last_panel_read_ms_ >= 50) {
    this->last_panel_read_ms_ = now;
    this->update_panel_();
  }

  if (this->buzzer_on_ && deadline_reached(now, this->buzzer_deadline_ms_))
    this->set_buzzer_(false);

  if (this->filter_drying_ && deadline_reached(now, this->filter_dry_deadline_ms_)) {
    this->filter_drying_ = false;
    this->stop_blower_();
    ESP_LOGI(TAG, "Filter drying complete");
  }

  this->update_tach_safety_();
  this->update_wet_state_();
}

}  // namespace esphome::switchbot_humidifier
