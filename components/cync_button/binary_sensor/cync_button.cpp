#include "cync_button.h"
#include "esphome/core/log.h"

namespace esphome::cync_button {

static const char *const TAG = "cync_button";
static const char *const TAG_RAW = "buttons.raw";

void CyncButton::setup() {
  this->pin_->setup();
  this->isr_pin_ = this->pin_->to_isr();

  // Consecutive identical samples needed, counting the first differing one,
  // so the pin has been stable for at least debounce_ms_ when we commit.
  this->samples_needed_ = (this->debounce_ms_ + this->sample_ms_ - 1) / this->sample_ms_ + 1;
  this->last_sample_ = this->pin_->digital_read();
  this->stable_count_ = this->samples_needed_;
  this->debounced_ = this->last_sample_;
  this->publish_initial_state(this->debounced_);

  if (this->log_spikes_) {
    this->raw_pressed_ = this->last_sample_;
    this->pin_->attach_interrupt(&CyncButton::edge_isr, this, gpio::INTERRUPT_ANY_EDGE);
  }

  const esp_timer_create_args_t args = {
      .callback = &CyncButton::sample_cb,
      .arg = this,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "cync_button",
      .skip_unhandled_events = true,
  };
  if (esp_timer_create(&args, &this->timer_) != ESP_OK ||
      esp_timer_start_periodic(this->timer_, (uint64_t) this->sample_ms_ * 1000) != ESP_OK) {
    ESP_LOGE(TAG, "Could not start the sampling timer");
    this->mark_failed();
  }
}

/// Runs in the esp_timer task, never in the main loop.
void CyncButton::sample_cb(void *arg) {
  auto *self = static_cast<CyncButton *>(arg);
  const bool sample = self->isr_pin_.digital_read();  // true = pressed (pin schema applies `inverted`)
  if (sample == self->last_sample_) {
    if (self->stable_count_ < self->samples_needed_)
      self->stable_count_++;
  } else {
    self->last_sample_ = sample;
    self->stable_count_ = 1;
  }
  if (self->stable_count_ >= self->samples_needed_ && sample != self->debounced_) {
    self->debounced_ = sample;
    self->enable_loop_soon_any_context();  // wake the main loop to publish it
  }
}

/// Edge interrupt: only measures raw lows for the log; takes no decisions.
void IRAM_ATTR CyncButton::edge_isr(CyncButton *self) {
  const bool pressed = self->isr_pin_.digital_read();
  const int64_t now = esp_timer_get_time();
  if (pressed && !self->raw_pressed_) {
    self->raw_pressed_ = true;
    self->raw_since_us_ = now;
  } else if (!pressed && self->raw_pressed_) {
    self->raw_pressed_ = false;
    self->raw_last_us_ = (uint32_t) (now - self->raw_since_us_);
    self->raw_count_ = self->raw_count_ + 1;
    self->enable_loop_soon_any_context();
  }
}

void CyncButton::loop() {
  // Publish from a snapshot compared with what was last published; no flag
  // to hand between the timer task and the loop, so nothing can be skipped.
  const bool debounced = this->debounced_;
  if (!this->has_state() || debounced != this->state)
    this->publish_state(debounced);
  const uint32_t count = this->raw_count_;
  if (count != this->raw_logged_) {
    const uint32_t us = this->raw_last_us_;
    const uint32_t missed = count - this->raw_logged_ - 1;
    if (missed == 0) {
      ESP_LOGD(TAG_RAW, "'%s' line low for %u.%03u ms", this->get_name().c_str(), us / 1000, us % 1000);
    } else {
      ESP_LOGD(TAG_RAW, "'%s' line low for %u.%03u ms (+%u more since last log)", this->get_name().c_str(),
               us / 1000, us % 1000, missed);
    }
    this->raw_logged_ = count;
  }
}

void CyncButton::dump_config() {
  LOG_BINARY_SENSOR("", "Cync Button", this);
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG,
                "  Debounce: %u ms (%u samples of %u ms, in the esp_timer task)\n"
                "  Spike logging: %s",
                this->debounce_ms_, this->samples_needed_, this->sample_ms_, YESNO(this->log_spikes_));
}

}  // namespace esphome::cync_button
