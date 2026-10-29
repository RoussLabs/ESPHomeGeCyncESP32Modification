#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include <esp_timer.h>

namespace esphome::cync_button {

/// Debounced button input whose decisions do not depend on the main loop.
///
/// ESPHome's gpio binary sensor with delayed_on_off decides in the main
/// loop: the line goes low, a timer starts, and the release is only noticed
/// when the loop next runs. If that is after the timer has expired, a
/// sub-millisecond spike becomes a press of exactly the debounce length
/// (seen as 52 ms presses with a 50 ms debounce, 82 ms with 80 ms).
///
/// Here a periodic esp_timer, running in the high-priority esp_timer task,
/// samples the pin every `sample_period` and commits a new state once the
/// pin has read the same for `debounce`. A spike that lands on one sample
/// is undone by the next. The main loop only publishes the result.
///
/// Edges are also counted by an interrupt so the spikes the sampler never
/// sees can be logged (DEBUG, tag buttons.raw), which shows whether the
/// hardware filtering on a fixture is doing its job.
class CyncButton final : public binary_sensor::BinarySensor, public Component {
 public:
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_debounce(uint32_t ms) { this->debounce_ms_ = ms; }
  void set_sample_period(uint32_t ms) { this->sample_ms_ = ms; }
  void set_log_spikes(bool log_spikes) { this->log_spikes_ = log_spikes; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  static void IRAM_ATTR edge_isr(CyncButton *self);
  static void sample_cb(void *arg);

  InternalGPIOPin *pin_{nullptr};
  ISRInternalGPIOPin isr_pin_;
  uint32_t debounce_ms_{50};
  uint32_t sample_ms_{5};
  bool log_spikes_{true};
  esp_timer_handle_t timer_{nullptr};

  // Sampler state (esp_timer task only)
  uint32_t samples_needed_{10};
  uint32_t stable_count_{0};
  bool last_sample_{false};
  // Sampler -> main loop
  volatile bool debounced_{false};
  volatile bool changed_{false};

  // Edge interrupt -> main loop
  volatile bool raw_pressed_{false};
  volatile int64_t raw_since_us_{0};
  volatile uint32_t raw_count_{0};
  volatile uint32_t raw_last_us_{0};
  uint32_t raw_logged_{0};
};

}  // namespace esphome::cync_button
