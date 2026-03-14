// led_driver.cpp — APA106 LED driver (WS2812-compatible)
//
// One jewel-tone colour per LFO waveform, pulsing with LFO rate/depth.
// Dim when trigger is not pressed, brighter when pressed.
//
// Compiles as empty stubs when HAS_WS2811 is not defined (desktop builds).

#include "led_driver.h"
#include <cstdio>
#include <algorithm>
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
static constexpr int NUM_COLORS = 8;

// Brightness levels (0.0–1.0)
static constexpr float BASE_DIM    = 0.08f;  // trigger not pressed
static constexpr float BASE_BRIGHT = 0.25f;  // trigger pressed

// ─── Brightness calculation ──────────────────────────────────────────
//
// base       = gate ? 0.25 : 0.08
// lfo_uni    = (lfo_output + 1) * 0.5        [0, 1]
// modulation = lfo_uni * lfo_depth
// brightness = base + modulation * (1 - base)

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

void LedDriver::update(int waveform_index, float lfo_output,
                        float lfo_depth, bool gate)
{
    if (!pimpl_->initialised) return;

    // Clamp waveform index
    int idx = std::clamp(waveform_index, 0, NUM_COLORS - 1);
    uint32_t color = WAVEFORM_COLORS[idx];

    // Brightness calculation
    float base   = gate ? BASE_BRIGHT : BASE_DIM;
    float lfo_u  = (lfo_output + 1.0f) * 0.5f;   // [-1,+1] → [0,1]
    float mod    = lfo_u * lfo_depth;
    float bright = base + mod * (1.0f - base);

    uint32_t pixel = apply_brightness(color, bright);

    // Set all LEDs to the same colour
    for (int i = 0; i < pimpl_->num_leds; i++)
        pimpl_->ledstring.channel[0].leds[i] = pixel;

    ws2811_render(&pimpl_->ledstring);
}

void LedDriver::blinkSave()
{
    if (!pimpl_->initialised) return;

    // White at 25% brightness
    uint32_t white = apply_brightness(0xFFFFFF, BASE_BRIGHT);

    // 3 blinks: 80 ms on, 80 ms off
    for (int blink = 0; blink < 3; blink++) {
        // On — white
        for (int i = 0; i < pimpl_->num_leds; i++)
            pimpl_->ledstring.channel[0].leds[i] = white;
        ws2811_render(&pimpl_->ledstring);
        std::this_thread::sleep_for(std::chrono::milliseconds(80));

        // Off
        for (int i = 0; i < pimpl_->num_leds; i++)
            pimpl_->ledstring.channel[0].leds[i] = 0;
        ws2811_render(&pimpl_->ledstring);
        if (blink < 2)
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
}

void LedDriver::shutdown()
{
    if (pimpl_ && pimpl_->initialised) {
        // Turn off LEDs
        for (int i = 0; i < pimpl_->num_leds; i++)
            pimpl_->ledstring.channel[0].leds[i] = 0;
        ws2811_render(&pimpl_->ledstring);

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

void LedDriver::update(int, float, float, bool) {}
void LedDriver::blinkSave() {}
void LedDriver::shutdown() {}

#endif // HAS_WS2811
