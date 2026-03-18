// led_driver.cpp — APA106 / WS2811 LED driver
//
// Pure single-channel waveform colors with LFO-driven brightness pulse.
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

// ─── RGB color type ─────────────────────────────────────────────────

struct RGB {
    uint8_t r, g, b;
};

// ─── Pure waveform palette ──────────────────────────────────────────
//
// Clean single/dual-channel colors that diffuse well through the APA106
// dome. Avoids RGB mixing that looks muddy through diffusion.
// Ordered to match LfoWave enum.

static constexpr RGB WAVEFORM_COLORS[] = {
    {255,   0,   0},    // Sine       → Red
    {  0, 255,   0},    // Triangle   → Green
    {  0,   0, 255},    // Square     → Blue
    {255, 160,   0},    // RampUp     → Orange  (R+slight G, no blue)
    {  0, 255, 255},    // RampDown   → Cyan
    {255, 255,   0},    // SampleHold → Yellow
    {255,   0, 255},    // ExpRise    → Magenta
    {255, 255, 255},    // ExpFall    → White
};
static constexpr int NUM_COLORS = static_cast<int>(std::size(WAVEFORM_COLORS));

// ─── Helpers ────────────────────────────────────────────────────────

// Desaturate an RGB color toward white by factor t (0=original, 1=white).
// For pure colors this effectively raises the off-channels, so minimize
// usage to avoid muddy diffusion.
static RGB desaturate(RGB c, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    auto mix = [t](uint8_t v) -> uint8_t {
        return static_cast<uint8_t>(v + (255.0f - v) * t);
    };
    return {mix(c.r), mix(c.g), mix(c.b)};
}

// Scale RGB brightness by factor [0, 1]
static RGB scale(RGB c, float brightness)
{
    brightness = std::clamp(brightness, 0.0f, 1.0f);
    return {
        static_cast<uint8_t>(c.r * brightness),
        static_cast<uint8_t>(c.g * brightness),
        static_cast<uint8_t>(c.b * brightness),
    };
}

// Pack RGB into 0x00RRGGBB for ws2811
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

// ─── Implementation ─────────────────────────────────────────────────

struct LedDriver::Impl {
#ifdef HAS_WS2811
    ws2811_t strip{};
    bool     hw_init = false;
#endif
    bool     ap_idle = false;
    float    ap_idle_phase = 0.0f;   // breathing animation phase

    // Smoothed pitch→saturation to avoid steppy LED jumps
    float    smooth_desat = 0.0f;

    // Frozen LFO value for LED — only updates while gate held or idle,
    // NOT during release tail (so pitch-envelope acceleration doesn't
    // show on the LED).
    float    led_lfo = 0.0f;
    bool     prev_gate = false;
    bool     releasing = false;
    int      release_frames = 0;    // frames since release began

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
    s.dmanum = 10;                        // DMA channel (10 avoids conflicts)

    s.channel[0].gpionum    = gpio_pin;   // GPIO 12 (PWM0)
    s.channel[0].count      = 1;          // single LED
    s.channel[0].invert     = 0;
    s.channel[0].brightness = 255;        // we control brightness in software
    s.channel[0].strip_type = WS2811_STRIP_GRB;

    // Channel 1 unused
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

    // Startup self-test: flash R → G → B so user can verify signal chain
    RGB test_colors[] = {{255,0,0}, {0,255,0}, {0,0,255}};
    const char* test_names[] = {"RED", "GREEN", "BLUE"};
    for (int i = 0; i < 3; i++) {
        pimpl_->setLed(scale(test_colors[i], 0.25f));
        slog("LED: self-test %s (0x%06X → render)",
             test_names[i], pimpl_->strip.channel[0].leds[0]);
        pimpl_->sleepMs(400);
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
                        bool gate, float freq, float /*lfo_rate*/)
{
    // AP idle mode: gentle white breathing
    if (pimpl_->ap_idle) {
        pimpl_->ap_idle_phase += 0.025f;  // ~0.5 Hz at 20 Hz update rate
        if (pimpl_->ap_idle_phase >= 1.0f)
            pimpl_->ap_idle_phase -= 1.0f;

        float breath = 0.25f + 0.15f * std::sin(2.0f * 3.14159265f * pimpl_->ap_idle_phase);
        pimpl_->setLed(scale({255, 255, 255}, breath));
        return;
    }

    // ── Gate edge detection for LFO freeze ─────────────────────────
    // Track LFO only while idle or gate-held. On release, freeze the
    // LED's LFO value so the pitch-envelope acceleration doesn't bleed
    // into the visual. This lets the user preview the LFO before pressing.
    if (gate && !pimpl_->prev_gate) {
        pimpl_->releasing = false;          // new press
        pimpl_->release_frames = 0;
    }
    if (!gate && pimpl_->prev_gate) {
        pimpl_->releasing = true;           // just released
        pimpl_->release_frames = 0;
    }
    pimpl_->prev_gate = gate;

    // Count release frames; after ~500ms (10 frames at 20 Hz) resume
    // LFO tracking so the idle preview works again.
    if (pimpl_->releasing) {
        if (++pimpl_->release_frames > 10)
            pimpl_->releasing = false;
    }

    // Update LED's LFO only when NOT in release tail
    if (!pimpl_->releasing)
        pimpl_->led_lfo = lfo_out;

    // ── Waveform color ─────────────────────────────────────────────
    int idx = static_cast<int>(wave);
    if (idx < 0 || idx >= NUM_COLORS) idx = 0;
    RGB color = WAVEFORM_COLORS[idx];

    // ── Pitch → saturation (smoothed) ──────────────────────────────
    // Low freq = vivid pure color, high freq = slightly washed out.
    // Reduced max desaturation (30%) to keep colors clean through diffusion.
    float freq_clamped = std::clamp(freq, FREQ_LO, FREQ_HI);
    float desat_target = std::log2(freq_clamped / FREQ_LO) / LOG_FREQ_RANGE;
    desat_target *= 0.30f;  // max 30% desaturation (was 60%)

    // Exponential smoothing: ~150ms time constant at 20 Hz update rate
    constexpr float SMOOTH_ALPHA = 0.12f;
    pimpl_->smooth_desat += SMOOTH_ALPHA * (desat_target - pimpl_->smooth_desat);
    color = desaturate(color, pimpl_->smooth_desat);

    // ── LFO → brightness modulation ───────────────────────────────
    // Use frozen LFO value. Map [-1,+1] to [0,1].
    float lfo_norm = (pimpl_->led_lfo + 1.0f) * 0.5f;

    // At full depth the LFO fully controls brightness (on/off blink for
    // square wave). At zero depth, steady brightness with no modulation.
    float brightness;
    if (gate) {
        // Gate held: base 30%, LFO sweeps full range [0, 0.45] scaled by depth
        float base = 0.30f * (1.0f - lfo_depth) + 0.02f * lfo_depth;
        float peak = 0.30f * (1.0f - lfo_depth) + 0.45f * lfo_depth;
        brightness = base + (peak - base) * lfo_norm;
    } else {
        // Idle: base 8%, LFO sweeps [0, 0.15] scaled by depth
        float base = 0.08f * (1.0f - lfo_depth) + 0.01f * lfo_depth;
        float peak = 0.08f * (1.0f - lfo_depth) + 0.15f * lfo_depth;
        brightness = base + (peak - base) * lfo_norm;
    }

    pimpl_->setLed(scale(color, brightness));
}

void LedDriver::blinkSave()
{
    // 3× white flash: ~130ms on, ~60ms off  (~400ms total)
    RGB white = {255, 255, 255};
    for (int i = 0; i < 3; i++) {
        pimpl_->setLed(scale(white, 0.25f));
        pimpl_->sleepMs(130);
        pimpl_->setLedOff();
        if (i < 2) pimpl_->sleepMs(60);
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
    constexpr float PEAK_POS = 0.6f;   // peak at 60% through animation

    RGB white = {255, 255, 255};

    for (int i = 0; i <= STEPS; i++) {
        float t = static_cast<float>(i) / STEPS;
        float brightness;

        if (t <= PEAK_POS) {
            // Approach: slow cubic rise — feels like something getting closer
            float tn = t / PEAK_POS;
            brightness = tn * tn * tn;
        } else if (t <= PEAK_POS + 0.1f) {
            // Peak: full brightness moment
            brightness = 1.0f;
        } else {
            // Departure: fast quadratic fall — whooshes past
            float tn = (t - PEAK_POS - 0.1f) / (1.0f - PEAK_POS - 0.1f);
            float inv = 1.0f - tn;
            brightness = inv * inv;
        }

        pimpl_->setLed(scale(white, brightness));
        pimpl_->sleepMs(STEP_MS);
    }

    pimpl_->setLedOff();
}

void LedDriver::playApExit()
{
    // Reverse Doppler: fast approach, slow departure — the transmission returns
    //
    // Mirror of enter: fast quadratic rise → peak → slow cubic fall

    constexpr int   STEPS    = 75;
    constexpr int   STEP_MS  = 20;
    constexpr float PEAK_POS = 0.3f;   // peak earlier (fast approach)

    RGB white = {255, 255, 255};

    for (int i = 0; i <= STEPS; i++) {
        float t = static_cast<float>(i) / STEPS;
        float brightness;

        if (t <= PEAK_POS) {
            // Fast approach: quadratic rise
            float tn = t / PEAK_POS;
            brightness = tn * tn;
        } else if (t <= PEAK_POS + 0.1f) {
            // Peak: full brightness
            brightness = 1.0f;
        } else {
            // Slow departure: cubic fall — fades into distance
            float tn = (t - PEAK_POS - 0.1f) / (1.0f - PEAK_POS - 0.1f);
            float inv = 1.0f - tn;
            brightness = inv * inv * inv;
        }

        pimpl_->setLed(scale(white, brightness));
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
