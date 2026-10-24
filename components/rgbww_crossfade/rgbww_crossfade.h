#pragma once

#include <algorithm>
#include <memory>

#include "esphome/core/component.h"
#include "esphome/core/optional.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/transformers.h"

namespace esphome::rgbww_crossfade {

static constexpr int NUM_CHANNELS = 5;  // red, green, blue, cold white, warm white

/// Order matches the options of the "Fade Style" select.
enum FadeMode : uint8_t {
  FADE_LINEAR = 0,   // Linear Crossfade
  FADE_ESPHOME = 1,  // ESPHome Default
};

class CrossfadeRGBWWLightOutput;

/// Transition used for every change of the light (fades, presets, Home
/// Assistant calls, dimming steps).
///
/// Linear crossfade (default): every channel's PWM duty moves in a straight
/// line from where it is to where it is going, on the same clock. All
/// channels start together, move at a constant rate and finish together, as
/// on a lighting console. The overall brightness still fades on the gamma
/// curve, so dimming looks even.
///
/// ESPHome default: ESPHome's own transition, which eases in and out and
/// interpolates each channel before gamma, so outgoing channels drop early
/// and incoming ones arrive late. Kept for comparison, with one fix: a dark
/// LED group takes the colour of the other end so it only fades in or out.
class CrossfadeTransformer : public light::LightTransitionTransformer {
 public:
  explicit CrossfadeTransformer(CrossfadeRGBWWLightOutput *output) : output_(output) {}
  ~CrossfadeTransformer() override;

  void start() override;
  optional<light::LightColorValues> apply() override;
  void stop() override;

 protected:
  static constexpr float DARK = 1e-4f;

  static float level_of(const light::LightColorValues &v) { return v.get_state() * v.get_brightness(); }
  static float rgb_level_of(const light::LightColorValues &v) { return level_of(v) * v.get_color_brightness(); }
  static void copy_rgb(const light::LightColorValues &from, light::LightColorValues &to) {
    to.set_red(from.get_red());
    to.set_green(from.get_green());
    to.set_blue(from.get_blue());
  }

  CrossfadeRGBWWLightOutput *output_;
  uint8_t mode_{FADE_LINEAR};
  float level_start_{0.0f};
  float level_end_{0.0f};
  float mix_start_[NUM_CHANNELS]{};
  float mix_end_[NUM_CHANNELS]{};
};

/// ESPHome's rgbww light output with the transition above.
class CrossfadeRGBWWLightOutput : public light::LightOutput {
 public:
  void set_red(output::FloatOutput *red) { red_ = red; }
  void set_green(output::FloatOutput *green) { green_ = green; }
  void set_blue(output::FloatOutput *blue) { blue_ = blue; }
  void set_cold_white(output::FloatOutput *cold_white) { cold_white_ = cold_white; }
  void set_warm_white(output::FloatOutput *warm_white) { warm_white_ = warm_white; }
  void set_cold_white_temperature(float cold_white_temperature) { cold_white_temperature_ = cold_white_temperature; }
  void set_warm_white_temperature(float warm_white_temperature) { warm_white_temperature_ = warm_white_temperature; }
  void set_constant_brightness(bool constant_brightness) { constant_brightness_ = constant_brightness; }
  void set_color_interlock(bool color_interlock) { color_interlock_ = color_interlock; }

  /// One of FadeMode; takes effect from the next change.
  void set_fade_mode(uint8_t mode) { fade_mode_ = mode == FADE_ESPHOME ? FADE_ESPHOME : FADE_LINEAR; }
  uint8_t get_fade_mode() const { return fade_mode_; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    if (this->color_interlock_) {
      traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::COLD_WARM_WHITE});
    } else {
      traits.set_supported_color_modes({light::ColorMode::RGB_COLD_WARM_WHITE});
    }
    traits.set_min_mireds(this->cold_white_temperature_);
    traits.set_max_mireds(this->warm_white_temperature_);
    return traits;
  }

  void setup_state(light::LightState *state) override { this->state_ = state; }

  std::unique_ptr<light::LightTransformer> create_default_transition() override {
    return std::make_unique<CrossfadeTransformer>(this);
  }

  void write_state(light::LightState *state) override {
    float duty[NUM_CHANNELS];
    if (this->override_active_) {
      std::copy(this->override_duty_, this->override_duty_ + NUM_CHANNELS, duty);
    } else {
      state->current_values_as_rgbww(&duty[0], &duty[1], &duty[2], &duty[3], &duty[4], this->constant_brightness_);
    }
    this->red_->set_level(duty[0]);
    this->green_->set_level(duty[1]);
    this->blue_->set_level(duty[2]);
    this->cold_white_->set_level(duty[3]);
    this->warm_white_->set_level(duty[4]);
    std::copy(duty, duty + NUM_CHANNELS, this->last_duty_);
    this->last_write_was_override_ = this->override_active_;
  }

  // ---- used by CrossfadeTransformer ----

  /// PWM duty of each channel that `values` would produce.
  void compute_duty(const light::LightColorValues &values, float *duty) {
    const light::LightColorValues saved = this->state_->current_values;
    this->state_->current_values = values;
    this->state_->current_values_as_rgbww(&duty[0], &duty[1], &duty[2], &duty[3], &duty[4],
                                          this->constant_brightness_);
    this->state_->current_values = saved;
  }
  float gamma(float value) const { return this->state_->gamma_correct_lut(value); }
  void set_override(const float *duty) {
    std::copy(duty, duty + NUM_CHANNELS, this->override_duty_);
    this->override_active_ = true;
  }
  void clear_override() { this->override_active_ = false; }
  bool last_write_was_override() const { return this->last_write_was_override_; }
  const float *last_duty() const { return this->last_duty_; }

 protected:
  output::FloatOutput *red_;
  output::FloatOutput *green_;
  output::FloatOutput *blue_;
  output::FloatOutput *cold_white_;
  output::FloatOutput *warm_white_;
  float cold_white_temperature_{0};
  float warm_white_temperature_{0};
  bool constant_brightness_{false};
  bool color_interlock_{false};
  uint8_t fade_mode_{FADE_LINEAR};

  light::LightState *state_{nullptr};
  float override_duty_[NUM_CHANNELS]{};
  bool override_active_{false};
  float last_duty_[NUM_CHANNELS]{};
  bool last_write_was_override_{false};
};

// ---- CrossfadeTransformer ----

inline CrossfadeTransformer::~CrossfadeTransformer() { this->output_->clear_override(); }

inline void CrossfadeTransformer::start() {
  // Sets start_values_ / end_values_, including the off -> on and on -> off cases.
  light::LightTransitionTransformer::start();
  this->mode_ = this->output_->get_fade_mode();

  if (this->mode_ == FADE_ESPHOME) {
    const float start_rgb = rgb_level_of(this->start_values_);
    const float end_rgb = rgb_level_of(this->end_values_);
    if (start_rgb < DARK && end_rgb >= DARK) {
      copy_rgb(this->end_values_, this->start_values_);
    } else if (end_rgb < DARK && start_rgb >= DARK) {
      copy_rgb(this->start_values_, this->end_values_);
    }
    return;
  }

  // Split each end into an overall level and a channel mix at full level.
  this->level_start_ = level_of(this->start_values_);
  this->level_end_ = level_of(this->end_values_);
  light::LightColorValues full = this->start_values_;
  full.set_state(1.0f);
  full.set_brightness(1.0f);
  this->output_->compute_duty(full, this->mix_start_);
  full = this->end_values_;
  full.set_state(1.0f);
  full.set_brightness(1.0f);
  this->output_->compute_duty(full, this->mix_end_);

  // A new change arrived mid-fade: carry on from what the LEDs show now.
  if (this->output_->last_write_was_override()) {
    const float g = this->output_->gamma(this->level_start_);
    if (g > 0.0f) {
      const float *last = this->output_->last_duty();
      for (int i = 0; i < NUM_CHANNELS; i++)
        this->mix_start_[i] = std::min(1.0f, last[i] / g);
    }
  }
}

inline optional<light::LightColorValues> CrossfadeTransformer::apply() {
  if (this->mode_ == FADE_ESPHOME)
    return light::LightTransitionTransformer::apply();

  const float p = this->get_progress_();  // linear in time, no easing
  const float level = this->output_->gamma(this->level_start_ + (this->level_end_ - this->level_start_) * p);
  float duty[NUM_CHANNELS];
  for (int i = 0; i < NUM_CHANNELS; i++)
    duty[i] = level * (this->mix_start_[i] + (this->mix_end_[i] - this->mix_start_[i]) * p);
  this->output_->set_override(duty);

  // Report values on the same clock, so the light's state and level stay in step.
  return light::LightColorValues::lerp(this->start_values_, this->end_values_, p);
}

inline void CrossfadeTransformer::stop() { this->output_->clear_override(); }

}  // namespace esphome::rgbww_crossfade
