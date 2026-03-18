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

static constexpr float PI = 3.14159265f;

// ─── Gamma 2.2 lookup table ────────────────────────────────────────
// Pre-computed: out = round(pow(in/255, 2.2) * 255)
// Applied in setLed() so all output paths get perceptually correct brightness.
static constexpr uint8_t GAMMA_LUT[256] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,
      1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,
      3,   3,   3,   3,   3,   4,   4,   4,   4,   5,   5,   5,   5,   6,   6,   6,
      6,   7,   7,   7,   8,   8,   8,   9,   9,   9,  10,  10,  11,  11,  11,  12,
     12,  13,  13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,
     20,  20,  21,  22,  22,  23,  23,  24,  25,  25,  26,  26,  27,  28,  28,  29,
     30,  30,  31,  32,  33,  33,  34,  35,  36,  36,  37,  38,  39,  40,  40,  41,
     42,  43,  44,  45,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,
     57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  72,  73,
     74,  75,  76,  77,  79,  80,  81,  82,  83,  85,  86,  87,  88,  90,  91,  92,
     94,  95,  96,  98,  99, 100, 102, 103, 105, 106, 107, 109, 110, 112, 113, 115,
    116, 118, 119, 121, 122, 124, 125, 127, 129, 130, 132, 133, 135, 137, 138, 140,
    142, 143, 145, 147, 148, 150, 152, 154, 155, 157, 159, 161, 163, 164, 166, 168,
    170, 172, 174, 175, 177, 179, 181, 183, 185, 187, 189, 191, 193, 195, 197, 199,
    201, 203, 205, 207, 209, 211, 213, 215, 217, 219, 221, 224, 226, 228, 230, 232,
    234, 237, 239, 241, 243, 245, 248, 250, 252, 255, 255, 255, 255, 255, 255, 255,
};

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
    {255, 160,  40},    // Sine       → Amber
    {  0, 200, 180},    // Triangle   → Teal
    { 30,  80, 255},    // Square     → Electric Blue
    {255, 100,  10},    // RampUp     → Deep Orange
    {  0, 180, 220},    // RampDown   → Cool Cyan
    {255, 220,  30},    // SampleHold → Bright Yellow
    {220,  40, 255},    // ExpRise    → Magenta
    {255, 180, 200},    // ExpFall    → Soft Pink-White
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

// ─── Smoothing time constants (alpha per frame at ~60 Hz) ───────────
//
// These define how "liquid" each parameter feels. Lower = smoother.
// At 60 Hz update rate (rescaled from 20 Hz to preserve time constants):
//   alpha 0.020 ≈ 800ms to 95%  (very smooth, Microcosm-like)
//   alpha 0.033 ≈ 500ms to 95%  (responsive but smooth)
//   alpha 0.050 ≈ 330ms to 95%  (snappy)
//   alpha 0.083 ≈ 200ms to 95%  (quick)

static constexpr float COLOR_ALPHA      = 0.027f;  // color crossfade (~600ms)
static constexpr float BRIGHTNESS_ALPHA = 0.033f;  // brightness envelope (~500ms)
static constexpr float GATE_ALPHA       = 0.020f;  // gate on/off ramp (~800ms)
static constexpr float LFO_SMOOTH_ALPHA = 0.050f;  // LFO brightness (~330ms)

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
    float    smooth_lfo = 0.0f;              // smoothed LFO output
    bool     color_initialized = false;      // first frame flag

    // Frozen LFO value for LED — only updates while gate held or idle,
    // NOT during release tail (~200ms freeze at ~60Hz update rate).
    float    led_lfo = 0.0f;
    bool     prev_gate = false;
    bool     releasing = false;
    int      release_frames = 0;

    int render_err_count = 0;

    void setLed(RGB c)
    {
#ifdef HAS_WS2811
        if (!hw_init) return;

        // Gamma correction + min floor (prevents WS2811 PWM flicker at low brightness)
        auto gamma = [](uint8_t v) -> uint8_t {
            if (v == 0) return 0;
            uint8_t g = GAMMA_LUT[v];
            return g < 3 ? 3 : g;
        };
        RGB gc = { gamma(c.r), gamma(c.g), gamma(c.b) };
        strip.channel[0].leds[0] = pack(gc);

        // Double-render: second render overwrites any frame corrupted
        // by encoder ground noise during DMA.
        ws2811_render(&strip);
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
        pimpl_->ap_idle_phase += 0.005f;  // ~0.3 Hz at 60 Hz update rate
        if (pimpl_->ap_idle_phase >= 1.0f)
            pimpl_->ap_idle_phase -= 1.0f;

        // Sinusoidal breathing with slight ease-in-out (sin² shaping)
        float phase = std::sin(2.0f * PI * pimpl_->ap_idle_phase);
        float breath = 0.06f + 0.20f * (phase * phase) * (phase > 0 ? 1.0f : -0.3f);
        breath = std::clamp(breath, 0.04f, 0.26f);

        // Smooth even the breathing output
        pimpl_->smooth_brightness = smooth(pimpl_->smooth_brightness, breath, 0.050f);
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
        if (++pimpl_->release_frames > 12)  // ~200ms freeze at 60 Hz
            pimpl_->releasing = false;
    }

    if (!pimpl_->releasing)
        pimpl_->led_lfo = lfo_out;

    // ── Target color from waveform ─────────────────────────────────
    int idx = static_cast<int>(wave);
    if (idx < 0 || idx >= NUM_COLORS) idx = 0;
    RGBf target_color = RGBf::from(WAVEFORM_COLORS[idx]);

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
    float lfo_alpha = LFO_SMOOTH_ALPHA + (0.18f - LFO_SMOOTH_ALPHA) * rate_factor;

    float lfo_target = (pimpl_->led_lfo + 1.0f) * 0.5f;  // map [-1,+1] → [0,1]
    pimpl_->smooth_lfo = smooth(pimpl_->smooth_lfo, lfo_target, lfo_alpha);

    // ── Compute brightness ─────────────────────────────────────────
    // Interpolate between idle and active brightness based on smoothed gate.
    //
    // At high LFO rates the eye can't resolve individual pulses, so we
    // boost the base brightness to compensate for perceptual averaging.
    // rate_boost: 0 at slow rates, ramps to 1.0 above ~10 Hz.
    float rate_boost = std::clamp((lfo_rate - 5.0f) / 10.0f, 0.0f, 1.0f);

    // Idle:   base 10-20%, LFO adds up to 20% at full depth (pre-gamma)
    // Active: base 35-55%, LFO adds up to 55% at full depth (pre-gamma)
    // Raised to compensate for gamma 2.2 compression.
    float idle_base = (0.10f + 0.10f * rate_boost) * (1.0f - lfo_depth) + 0.02f * lfo_depth;
    float idle_peak = 0.10f * (1.0f - lfo_depth) + 0.20f * lfo_depth;
    float idle_bright = idle_base + (idle_peak - idle_base) * pimpl_->smooth_lfo;

    float gate_base = (0.35f + 0.20f * rate_boost) * (1.0f - lfo_depth) + 0.04f * lfo_depth;
    float gate_peak = 0.35f * (1.0f - lfo_depth) + 0.55f * lfo_depth;
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
