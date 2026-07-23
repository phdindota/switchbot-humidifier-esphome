#pragma once

#include <cstdint>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"

namespace esphome::switchbot_humidifier {

enum class WaterLevel : uint8_t {
  EMPTY = 0,
  LOW = 1,
  MEDIUM = 2,
  HIGH = 3,
  UNKNOWN = 0xFF,
};

enum class PanelBrightness : uint8_t {
  NORMAL = 0,
  DIM = 1,
  OFF = 2,
};

/**
 * Low-level W3902310/V07 hardware coordinator.
 *
 * The class deliberately owns blower, wet, panel-rail, refill-safe-state, and
 * buzzer pins together. Its important invariants are:
 *   - all power/control outputs begin in their recovered safe states;
 *   - wet outputs can only run after explicit arming, valid non-empty water,
 *     blower feedback, and the configured startup delay;
 *   - the wet pair is always forced off for filter drying and faults; and
 *   - unverified auto-refill hardware never receives a drive signal.
 *
 * Higher-level Target/Auto/Sleep policy lives in the example YAML. This class
 * enforces the hardware sequence regardless of which policy requested a speed.
 */
class SwitchBotHumidifier final : public output::FloatOutput, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_speed_frequencies(uint32_t speed_1, uint32_t speed_2, uint32_t speed_3,
                             uint32_t speed_4);
  void set_wet_start_delay(uint32_t delay_ms) { this->wet_start_delay_ms_ = delay_ms; }
  void set_filter_dry_duration(uint32_t duration_ms) { this->filter_dry_duration_ms_ = duration_ms; }
  void set_automatic_filter_drying(bool enabled) { this->automatic_filter_drying_ = enabled; }
  void set_tach_safety(bool enabled) { this->tach_safety_ = enabled; }
  void set_tach_startup_timeout(uint32_t timeout_ms) { this->tach_startup_timeout_ms_ = timeout_ms; }
  void set_minimum_tach_pulses_per_minute(float pulses) { this->minimum_tach_ppm_ = pulses; }

  void set_tach_pulses_per_minute(float pulses);
  void set_wet_system_allowed(bool allowed);
  void set_panel_brightness(uint8_t mode);
  void start_filter_dry();
  void stop_filter_dry();
  void stop_all();
  void beep(uint16_t duration_ms = 80);

  bool is_blower_running() const { return this->blower_running_; }
  bool is_humidifying() const { return this->requested_humidifying_; }
  bool is_wet_active() const { return this->wet_auxiliaries_active_; }
  bool is_wet_system_allowed() const { return this->wet_system_allowed_; }
  bool is_filter_drying() const { return this->filter_drying_; }
  bool has_tach_fault() const { return this->tach_fault_; }
  bool has_panel_sample() const { return this->panel_sampled_; }
  bool is_water_empty() const { return this->water_level_ == WaterLevel::EMPTY; }
  uint8_t get_speed() const { return this->current_speed_; }
  uint32_t get_speed_frequency() const;
  uint32_t get_panel_key_mask() const { return this->panel_key_mask_; }
  float get_tach_pulses_per_minute() const { return this->tach_ppm_; }
  WaterLevel get_water_level() const { return this->water_level_; }
  const char *get_water_level_name() const;
  uint32_t get_filter_dry_remaining_seconds() const;

 protected:
  void write_state(float state) override;

  bool configure_hardware_();
  bool configure_timer_(ledc_mode_t mode, ledc_timer_t timer, uint32_t frequency);
  bool configure_channel_(gpio_num_t pin, ledc_mode_t mode, ledc_channel_t channel,
                          ledc_timer_t timer, uint32_t duty);
  bool configure_output_pin_(gpio_num_t pin, uint32_t initial_level);
  bool configure_input_pin_(gpio_num_t pin);
  bool write_duty_(ledc_mode_t mode, ledc_channel_t channel, uint32_t duty);

  bool start_blower_(uint8_t speed, bool restart_tach_window = true);
  void stop_blower_();
  void set_wet_auxiliaries_(bool enabled);
  void update_wet_state_();
  void update_tach_safety_();
  void stop_all_hardware_();

  void set_buzzer_(bool enabled);
  void write_panel_byte_(uint8_t value);
  bool read_panel_keys_(uint32_t &result);
  void update_panel_();
  void decode_water_level_();

  bool initialized_{false};
  bool requested_humidifying_{false};
  bool blower_running_{false};
  bool wet_system_allowed_{false};
  bool wet_auxiliaries_active_{false};
  bool wet_started_this_cycle_{false};
  bool filter_drying_{false};
  bool automatic_filter_drying_{true};
  bool tach_safety_{true};
  bool tach_valid_{false};
  bool tach_fault_{false};
  bool buzzer_on_{false};
  bool panel_sampled_{false};

  uint8_t current_speed_{0};
  uint32_t speed_frequencies_[4]{125, 250, 400, 550};
  float tach_ppm_{0.0f};
  float minimum_tach_ppm_{30.0f};
  uint32_t wet_start_delay_ms_{8000};
  uint32_t filter_dry_duration_ms_{55U * 60U * 1000U};
  uint32_t tach_startup_timeout_ms_{15000};
  uint32_t blower_started_ms_{0};
  uint32_t filter_dry_deadline_ms_{0};
  uint32_t buzzer_deadline_ms_{0};
  uint32_t last_panel_read_ms_{0};
  uint32_t panel_key_mask_{0};
  uint8_t panel_key_bytes_[5]{};
  WaterLevel water_level_{WaterLevel::UNKNOWN};
  PanelBrightness panel_brightness_{PanelBrightness::NORMAL};
};

}  // namespace esphome::switchbot_humidifier
