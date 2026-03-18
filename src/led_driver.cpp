// led_driver.cpp — APA106 / WS2811 LED driver
//
// Microcosm-inspired smooth LED behavior: all transitions (color, brightness,
// gate state) are exponentially smoothed so the LED feels liquid and organic.
// No hard jumps, no steppy changes — everything crossfades.
//
// AP mode: Andúril-inspired Doppler fly-by in white.
// Desktop: compiles as no-op stubs when HAS_WS2811 is not defined.

#include "led_driver.h"
#include "siren_log.h"

#include <cmath>
#include <algorithm>
#include <chrono>
#include <thread>

#ifdef HAS_WS2811
#include <ws2811.h>
#endif

// ─── RGB color type (float for smooth interpolation) ────────────────

struct RGB {
    uint8_t r, g, b;
};

struct RGBf {
    float r, g, b;

    static RGBf from(RGB c) {
        return {static_cast<float>(c.r), static_cast<float>(c.g), static_cast<float>(c.b)};
    }

    RGB to_rgb() const {
        return {
            static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f)),
        };
    }

    RGBf operator*(float s) const { return {r * s, g * s, b * s}; }
    RGBf operator+(RGBf o) const { return {r + o.r, g + o.g, b + o.b}; }
};

// Exponential smoothing toward target: moves `current` toward `target`
// by fraction `alpha` per frame. Lower alpha = slower/smoother.
static float smooth(float current, float target, float alpha)
{
    return current + alpha * (target - current);
}

static RGBf smooth_rgb(RGBf current, RGBf target, float alpha)
{
    return {
        smooth(current.r, target.r, alpha),
        smooth(current.g, target.g, alpha),
        smooth(current.b, target.b, alpha),
    };
}

// ─── Pure waveform palette ──────────────────────────────────────────

static constexpr RGB WAVEFORM_COLORS[] = {
    {255,   0,   0},    // Sine       → Red
    {  0, 255,   0},    // Triangle   → Green
    {  0,   0, 255},    // Square     → Blue
    {255, 160,   0},    // RampUp     → Orange
    {  0, 255, 255},    // RampDown   → Cyan
    {255, 255,   0},    // SampleHold → Yellow
    {255,   0, 255},    // ExpRise    → Magenta
    {255, 255, 255},    // ExpFall    → White
};
static constexpr int NUM_COLORS = static_cast<int>(std::size(WAVEFORM_COLORS));

// ─── Helpers ────────────────────────────────────────────────────────

static RGB scale(RGB c, float brightness)
{
    brightness = std::clamp(brightness, 0.0f, 1.0f);
    return {
        static_cast<uint8_t>(c.r * brightness),
        static_cast<uint8_t>(c.g * brightness),
        static_cast<uint8_t>(c.b * brightness),
    };
}

static uint32_t pack(RGB c)
{
    return (static_cast<uint32_t>(c.r) << 16)
         | (static_cast<uint32_t>(c.g) << 8)
         |  static_cast<uint32_t>(c.b);
}

// Frequency constants for pitch→saturation mapping
static constexpr float FREQ_LO = 30.0f;
static constexpr float FREQ_HI = 8000.0f;
static const float LOG_FREQ_RANGE = std::log2(FREQ_HI / FREQ_LO);

// ─── Smoothing time constants (alpha per frame at ~20 Hz) ───────────
//
// These define how "liquid" each parameter feels. Lower = smoother.
// At 20 Hz update rate:
//   alpha 0.06 ≈ 800ms to 95%  (very smooth, Microcosm-like)
//   alpha 0.10 ≈ 500ms to 95%  (responsive but smooth)
//   alpha 0.15 ≈ 330ms to 95%  (snappy)
//   alpha 0.25 ≈ 200ms to 95%  (quick)

static constexpr float COLOR_ALPHA      = 0.08f;  // color crossfade (~600ms)
static constexpr float BRIGHTNESS_ALPHA = 0.10f;  // brightness envelope (~500ms)
static constexpr float GATE_ALPHA       = 0.06f;  // gate on/off ramp (~800ms)
static constexpr float DESAT_ALPHA      = 0.08f;  // pitch→saturation (~600ms)
static constexpr float LFO_SMOOTH_ALPHA = 0.15f;  // LFO brightness (~330ms)

// ─── Implementation ─────────────────────────────────────────────────

struct LedDriver::Impl {
#ifdef HAS_WS2811
    ws2811_t strip{};
    bool     hw_init = false;
#endif
    bool     ap_idle = false;
    float    ap_idle_phase = 0.0f;

    // ── Smoothed state (all transitions crossfade) ──────────────
    RGBf     smooth_color = {0, 0, 0};      // current displayed color (pre-brightness)
    float    smooth_brightness = 0.08f;      // current brightness
    float    smooth_gate = 0.0f;             // 0=idle, 1=gate held (smoothed)
    float    smooth_desat = 0.0f;            // pitch→desaturation
    float    smooth_lfo = 0.0f;              // smoothed LFO output
    bool     color_initialized = false;      // first frame flag

    // Frozen LFO value for LED — only updates while gate held or idle,
    // NOT during release tail.
    float    led_lfo = 0.0f;
    bool     prev_gate = false;
    bool     releasing = false;
    int      release_frames = 0;

    int render_err_count = 0;

    void setLed(RGB c)
    {
#ifdef HAS_WS2811
        if (!hw_init) return;
        strip.channel[0].leds[0] = pack(c);
        ws2811_return_t ret = ws2811_render(&strip);
        if (ret != WS2811_SUCCESS) {
            if (render_err_count++ < 10)
                slog("LED: render failed: %s", ws2811_get_return_t_str(ret));
        }
#else
        (void)c;
#endif
    }

    void setLedOff()
    {
        setLed({0, 0, 0});
    }

    void sleepMs(int ms)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
};

// ─── Public interface ───────────────────────────────────────────────

LedDriver::LedDriver() : pimpl_(std::make_unique<Impl>()) {}
LedDriver::~LedDriver() { shutdown(); }

bool LedDriver::init(int gpio_pin)
{
#ifdef HAS_WS2811
    auto& s = pimpl_->strip;
    s.freq   = WS2811_TARGET_FREQ;
    s.dmanum = 10;

    s.channel[0].gpionum    = gpio_pin;
    s.channel[0].count      = 1;
    s.channel[0].invert     = 0;
    s.channel[0].brightness = 255;
    s.channel[0].strip_type = WS2811_STRIP_GRB;

    s.channel[1].gpionum = 0;
    s.channel[1].count   = 0;

    ws2811_return_t ret = ws2811_init(&s);
    if (ret != WS2811_SUCCESS) {
        slog("LED: ws2811_init failed: %s", ws2811_get_return_t_str(ret));
        return false;
    }
    pimpl_->hw_init = true;
    slog("LED: Initialised on GPIO %d (DMA %d, freq %d)",
         gpio_pin, s.dmanum, s.freq);

    // Startup self-test: smooth fade through R → G → B
    RGB test_colors[] = {{255,0,0}, {0,255,0}, {0,0,255}};
    const char* test_names[] = {"RED", "GREEN", "BLUE"};
    for (int i = 0; i < 3; i++) {
        // Fade in
        for (int step = 0; step <= 10; step++) {
            float t = static_cast<float>(step) / 10.0f;
            pimpl_->setLed(scale(test_colors[i], t * 0.25f));
            pimpl_->sleepMs(20);
        }
        slog("LED: self-test %s (0x%06X → render)",
             test_names[i], pimpl_->strip.channel[0].leds[0]);
        pimpl_->sleepMs(150);
        // Fade out
        for (int step = 10; step >= 0; step--) {
            float t = static_cast<float>(step) / 10.0f;
            pimpl_->setLed(scale(test_colors[i], t * 0.25f));
            pimpl_->sleepMs(20);
        }
    }
    pimpl_->setLedOff();
    slog("LED: self-test complete");

    return true;
#else
    (void)gpio_pin;
    slog("LED: No ws2811 — stub mode");
    return true;
#endif
}

void LedDriver::update(LfoWave wave, float lfo_out, float lfo_depth,
                        bool gate, float freq, float lfo_rate)
{
    // AP idle mode: gentle white breathing
    if (pimpl_->ap_idle) {
        pimpl_->ap_idle_phase += 0.015f;  // ~0.3 Hz — slower, more meditative
        if (pimpl_->ap_idle_phase >= 1.0f)
            pimpl_->ap_idle_phase -= 1.0f;

        // Sinusoidal breathing with slight ease-in-out (sin² shaping)
        float phase = std::sin(2.0f * 3.14159265f * pimpl_->ap_idle_phase);
        float breath = 0.06f + 0.20f * (phase * phase) * (phase > 0 ? 1.0f : -0.3f);
        breath = std::clamp(breath, 0.04f, 0.26f);

        // Smooth even the breathing output
        pimpl_->smooth_brightness = smooth(pimpl_->smooth_brightness, breath, 0.15f);
        pimpl_->setLed(scale({255, 255, 255}, pimpl_->smooth_brightness));
        return;
    }

    // ── Gate edge detection for LFO freeze ─────────────────────────
    if (gate && !pimpl_->prev_gate) {
        pimpl_->releasing = false;
        pimpl_->release_frames = 0;
    }
    if (!gate && pimpl_->prev_gate) {
        pimpl_->releasing = true;
        pimpl_->release_frames = 0;
    }
    pimpl_->prev_gate = gate;

    if (pimpl_->releasing) {
        if (++pimpl_->release_frames > 10)
            pimpl_->releasing = false;
    }

    if (!pimpl_->releasing)
        pimpl_->led_lfo = lfo_out;

    // ── Target color from waveform ─────────────────────────────────
    int idx = static_cast<int>(wave);
    if (idx < 0 || idx >= NUM_COLORS) idx = 0;
    RGBf target_color = RGBf::from(WAVEFORM_COLORS[idx]);

    // ── Pitch → desaturation (smoothed) ────────────────────────────
    float freq_clamped = std::clamp(freq, FREQ_LO, FREQ_HI);
    float desat_target = std::log2(freq_clamped / FREQ_LO) / LOG_FREQ_RANGE;
    desat_target *= 0.30f;
    pimpl_->smooth_desat = smooth(pimpl_->smooth_desat, desat_target, DESAT_ALPHA);

    // Apply desaturation toward white
    float d = std::clamp(pimpl_->smooth_desat, 0.0f, 1.0f);
    target_color.r = target_color.r + (255.0f - target_color.r) * d;
    target_color.g = target_color.g + (255.0f - target_color.g) * d;
    target_color.b = target_color.b + (255.0f - target_color.b) * d;

    // ── Smooth color crossfade ─────────────────────────────────────
    // On first frame, snap to target so we don't fade up from black
    if (!pimpl_->color_initialized) {
        pimpl_->smooth_color = target_color;
        pimpl_->color_initialized = true;
    } else {
        pimpl_->smooth_color = smooth_rgb(pimpl_->smooth_color, target_color, COLOR_ALPHA);
    }

    // ── Smooth gate transition ─────────────────────────────────────
    // 0 = idle, 1 = gate held. Smooth so brightness ramps organically.
    float gate_target = gate ? 1.0f : 0.0f;
    pimpl_->smooth_gate = smooth(pimpl_->smooth_gate, gate_target, GATE_ALPHA);

    // ── Rate-adaptive LFO smoothing ───────────────────────────────
    // At slow rates (<3 Hz): heavy smoothing for liquid Microcosm feel.
    // At fast rates (>10 Hz): lighter smoothing so the LED can track.
    // The smoothing alpha scales with rate so the filter's cutoff
    // frequency rises proportionally with the LFO speed.
    float rate_factor = std::clamp(lfo_rate / 8.0f, 0.0f, 1.0f);  // 0→8 Hz maps to 0→1
    float lfo_alpha = LFO_SMOOTH_ALPHA + (0.55f - LFO_SMOOTH_ALPHA) * rate_factor;

    float lfo_target = (pimpl_->led_lfo + 1.0f) * 0.5f;  // map [-1,+1] → [0,1]
    pimpl_->smooth_lfo = smooth(pimpl_->smooth_lfo, lfo_target, lfo_alpha);

    // ── Compute brightness ─────────────────────────────────────────
    // Interpolate between idle and active brightness based on smoothed gate.
    //
    // At high LFO rates the eye can't resolve individual pulses, so we
    // boost the base brightness to compensate for perceptual averaging.
    // rate_boost: 0 at slow rates, ramps to 1.0 above ~10 Hz.
    float rate_boost = std::clamp((lfo_rate - 5.0f) / 10.0f, 0.0f, 1.0f);

    // Idle:   base 8%, LFO adds up to 15% at full depth
    // Active: base 30%, LFO adds up to 45% at full depth
    // At high rates: idle base → 12%, active base → 35%
    float idle_base = (0.08f + 0.04f * rate_boost) * (1.0f - lfo_depth) + 0.01f * lfo_depth;
    float idle_peak = 0.08f * (1.0f - lfo_depth) + 0.15f * lfo_depth;
    float idle_bright = idle_base + (idle_peak - idle_base) * pimpl_->smooth_lfo;

    float gate_base = (0.30f + 0.05f * rate_boost) * (1.0f - lfo_depth) + 0.02f * lfo_depth;
    float gate_peak = 0.30f * (1.0f - lfo_depth) + 0.45f * lfo_depth;
    float gate_bright = gate_base + (gate_peak - gate_base) * pimpl_->smooth_lfo;

    // Crossfade between idle and active brightness
    float target_brightness = idle_bright + (gate_bright - idle_bright) * pimpl_->smooth_gate;

    // Final brightness smoothing for extra liquidity
    pimpl_->smooth_brightness = smooth(pimpl_->smooth_brightness, target_brightness, BRIGHTNESS_ALPHA);

    // ── Final output ───────────────────────────────────────────────
    RGBf final_color = pimpl_->smooth_color * pimpl_->smooth_brightness;
    pimpl_->setLed(final_color.to_rgb());
}

void LedDriver::blinkSave()
{
    // Smooth pulse rather than hard blink: fade up, hold, fade down × 3
    for (int i = 0; i < 3; i++) {
        // Fade up
        for (int step = 0; step <= 8; step++) {
            float t = static_cast<float>(step) / 8.0f;
            // Ease-in curve for organic feel
            t = t * t;
            pimpl_->setLed(scale({255, 255, 255}, t * 0.30f));
            pimpl_->sleepMs(12);
        }
        // Brief hold at peak
        pimpl_->sleepMs(30);
        // Fade down
        for (int step = 8; step >= 0; step--) {
            float t = static_cast<float>(step) / 8.0f;
            pimpl_->setLed(scale({255, 255, 255}, t * 0.30f));
            pimpl_->sleepMs(12);
        }
        if (i < 2) pimpl_->sleepMs(40);
    }
}

void LedDriver::playApEnter()
{
    // Andúril-inspired Doppler fly-by: white light simulates a
    // transmission approaching, peaking, and departing.
    //
    // Asymmetric: slow cubic approach → brief peak → fast quadratic departure
    // Duration: ~1.5s (75 steps at 20ms each)

    constexpr int   STEPS    = 75;
    constexpr int   STEP_MS  = 20;
    constexpr float PEAK_POS = 0.6f;

    for (int i = 0; i <= STEPS; i++) {
        float t = static_cast<float>(i) / STEPS;
        float brightness;

        if (t <= PEAK_POS) {
            float tn = t / PEAK_POS;
            brightness = tn * tn * tn;
        } else if (t <= PEAK_POS + 0.1f) {
            brightness = 1.0f;
        } else {
            float tn = (t - PEAK_POS - 0.1f) / (1.0f - PEAK_POS - 0.1f);
            float inv = 1.0f - tn;
            brightness = inv * inv;
        }

        pimpl_->setLed(scale({255, 255, 255}, brightness));
        pimpl_->sleepMs(STEP_MS);
    }

    pimpl_->setLedOff();
}

void LedDriver::playApExit()
{
    // Reverse Doppler: fast approach, slow departure

    constexpr int   STEPS    = 75;
    constexpr int   STEP_MS  = 20;
    constexpr float PEAK_POS = 0.3f;

    for (int i = 0; i <= STEPS; i++) {
        float t = static_cast<float>(i) / STEPS;
        float brightness;

        if (t <= PEAK_POS) {
            float tn = t / PEAK_POS;
            brightness = tn * tn;
        } else if (t <= PEAK_POS + 0.1f) {
            brightness = 1.0f;
        } else {
            float tn = (t - PEAK_POS - 0.1f) / (1.0f - PEAK_POS - 0.1f);
            float inv = 1.0f - tn;
            brightness = inv * inv * inv;
        }

        pimpl_->setLed(scale({255, 255, 255}, brightness));
        pimpl_->sleepMs(STEP_MS);
    }

    pimpl_->setLedOff();
}

void LedDriver::setApIdle(bool active)
{
    pimpl_->ap_idle = active;
    if (active)
        pimpl_->ap_idle_phase = 0.0f;
}

void LedDriver::shutdown()
{
#ifdef HAS_WS2811
    if (pimpl_ && pimpl_->hw_init) {
        pimpl_->setLedOff();
        ws2811_fini(&pimpl_->strip);
        pimpl_->hw_init = false;
        slog("LED: Shutdown");
    }
#endif
}
