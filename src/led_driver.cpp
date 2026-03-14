// led_driver.cpp — APA106 LED driver (WS2812-compatible)
//
// One jewel-tone colour per LFO waveform, pulsing with LFO rate/depth.
// Dim when trigger is not pressed, brighter when pressed.
//
// Compiles as empty stubs when HAS_WS2811 is not defined (desktop builds).

#include "led_driver.h"
#include <cstdio>
#include <algorithm>
#include <iterator>
#include <thread>
#include <chrono>

// ─── Jewel-tone colour table (one per LfoWave, RGB) ──────────────────
//
//   0 Sine        → Ruby       0xE0115F
//   1 Triangle    → Emerald    0x50C878
//   2 Square      → Sapphire   0x0F52BA
//   3 RampUp      → Topaz      0xFFC87C
//   4 RampDown    → Amethyst   0x9966CC
//   5 SampleHold  → Citrine    0xE4D00A
//   6 ExpRise     → Garnet     0x9B111E
//   7 ExpFall     → Tanzanite  0x5A4FCF

static constexpr uint32_t WAVEFORM_COLORS[] = {
    0xE0115F,   // Sine        — Ruby
    0x50C878,   // Triangle    — Emerald
    0x0F52BA,   // Square      — Sapphire
    0xFFC87C,   // RampUp      — Topaz
    0x9966CC,   // RampDown    — Amethyst
    0xE4D00A,   // SampleHold  — Citrine
    0x9B111E,   // ExpRise     — Garnet
    0x5A4FCF,   // ExpFall     — Tanzanite
};
static constexpr int NUM_COLORS = static_cast<int>(std::size(WAVEFORM_COLORS));

// Brightness levels (0.0–1.0)
static constexpr float BASE_DIM    = 0.08f;  // trigger not pressed
static constexpr float BASE_BRIGHT = 0.25f;  // trigger pressed

// ─── Brightness calculation ──────────────────────────────────────────
//
// base       = gate ? 0.25 : 0.08
// lfo_uni    = (lfo_output + 1) * 0.5        [0, 1]
// modulation = lfo_uni * lfo_depth
// brightness = base + modulation * (1 - base)

// ─── Pitch → desaturation ─────────────────────────────────────────────
//
// Low pitch (30 Hz)   → fully saturated jewel tone  (desat = 0)
// High pitch (8000 Hz) → pastel / washed-out         (desat = 1)
// Log-scaled to match pitch perception.

static constexpr float FREQ_MIN = 30.0f;
static constexpr float FREQ_MAX = 8000.0f;

static uint32_t desaturate(uint32_t color, float amount)
{
    // Lerp each channel toward white (255) by amount [0,1]
    float r = static_cast<float>((color >> 16) & 0xFF);
    float g = static_cast<float>((color >> 8)  & 0xFF);
    float b = static_cast<float>((color)       & 0xFF);

    r += (255.0f - r) * amount;
    g += (255.0f - g) * amount;
    b += (255.0f - b) * amount;

    return (static_cast<uint32_t>(r) << 16)
         | (static_cast<uint32_t>(g) << 8)
         |  static_cast<uint32_t>(b);
}

static uint32_t apply_brightness(uint32_t color, float brightness)
{
    float r = static_cast<float>((color >> 16) & 0xFF);
    float g = static_cast<float>((color >> 8)  & 0xFF);
    float b = static_cast<float>((color)       & 0xFF);

    auto scale = [&](float ch) -> uint8_t {
        return static_cast<uint8_t>(std::clamp(ch * brightness, 0.0f, 255.0f));
    };

    return (static_cast<uint32_t>(scale(r)) << 16)
         | (static_cast<uint32_t>(scale(g)) << 8)
         |  static_cast<uint32_t>(scale(b));
}

// =====================================================================
#ifdef HAS_WS2811
// =====================================================================

#include <ws2811.h>

struct LedDriver::Impl {
    ws2811_t ledstring{};
    int      num_leds = 0;
    bool     initialised = false;

    // Set all LEDs to the same colour and render
    void setAllLeds(uint32_t color) {
        for (int i = 0; i < num_leds; i++)
            ledstring.channel[0].leds[i] = color;
        ws2811_render(&ledstring);
    }
};

LedDriver::LedDriver()  : pimpl_(std::make_unique<Impl>()) {}
LedDriver::~LedDriver() { shutdown(); }

bool LedDriver::init(int gpio_pin, int num_leds)
{
    pimpl_->num_leds = num_leds;

    ws2811_t& ls = pimpl_->ledstring;
    ls.freq   = WS2811_TARGET_FREQ;   // 800 kHz
    ls.dmanum = 10;                   // DMA channel 10 (safe default)

    ls.channel[0].gpionum    = gpio_pin;
    ls.channel[0].invert     = 0;
    ls.channel[0].count      = num_leds;
    ls.channel[0].strip_type = WS2812_STRIP;  // GRB byte order
    ls.channel[0].brightness = 255;           // software-controlled

    // Unused second channel
    ls.channel[1].gpionum    = 0;
    ls.channel[1].invert     = 0;
    ls.channel[1].count      = 0;
    ls.channel[1].brightness = 0;

    ws2811_return_t rc = ws2811_init(&ls);
    if (rc != WS2811_SUCCESS) {
        fprintf(stderr, "LED: ws2811_init failed: %s\n",
                ws2811_get_return_t_str(rc));
        return false;
    }

    pimpl_->initialised = true;
    printf("LED: %d APA106 on GPIO %d (PWM0)\n", num_leds, gpio_pin);
    return true;
}

void LedDriver::update(LfoWave waveform, float lfo_output,
                        float lfo_depth, bool gate, float freq)
{
    if (!pimpl_->initialised) return;

    int idx = std::clamp(static_cast<int>(waveform), 0, NUM_COLORS - 1);
    uint32_t color = WAVEFORM_COLORS[idx];

    // Pitch → desaturation (log-scaled)
    float f = std::clamp(freq, FREQ_MIN, FREQ_MAX);
    float desat = (std::log2(f) - std::log2(FREQ_MIN))
                / (std::log2(FREQ_MAX) - std::log2(FREQ_MIN));
    color = desaturate(color, desat);

    // Brightness calculation
    float base   = gate ? BASE_BRIGHT : BASE_DIM;
    float lfo_u  = (lfo_output + 1.0f) * 0.5f;   // [-1,+1] → [0,1]
    float mod    = lfo_u * lfo_depth;
    float bright = base + mod * (1.0f - base);

    pimpl_->setAllLeds(apply_brightness(color, bright));
}

void LedDriver::blinkSave()
{
    if (!pimpl_->initialised) return;

    uint32_t white = apply_brightness(0xFFFFFF, BASE_BRIGHT);

    // 3 blinks: 80 ms on, 80 ms off
    for (int blink = 0; blink < 3; blink++) {
        pimpl_->setAllLeds(white);
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        pimpl_->setAllLeds(0);
        if (blink < 2)
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
}

void LedDriver::shutdown()
{
    if (pimpl_ && pimpl_->initialised) {
        pimpl_->setAllLeds(0);
        ws2811_fini(&pimpl_->ledstring);
        pimpl_->initialised = false;
        printf("LED: shutdown\n");
    }
}

// =====================================================================
#else  // !HAS_WS2811 — desktop stubs
// =====================================================================

struct LedDriver::Impl {};

LedDriver::LedDriver()  : pimpl_(std::make_unique<Impl>()) {}
LedDriver::~LedDriver() { shutdown(); }

bool LedDriver::init(int, int)
{
    printf("LED: ws2811 not available — LED disabled\n");
    return false;
}

void LedDriver::update(LfoWave, float, float, bool, float) {}
void LedDriver::blinkSave() {}
void LedDriver::shutdown() {}

#endif // HAS_WS2811
