#pragma once

// ─── APA106 LED driver (WS2812-compatible via rpi_ws281x) ────────────
//
// Displays one jewel-tone color per LFO waveform.
// Pulses with LFO rate/depth; brighter when trigger is held.

#include <memory>
#include "lfo.h"

class LedDriver {
public:
    LedDriver();
    ~LedDriver();

    // Initialise LED strip.
    //   gpio_pin: BCM pin for data (default 12 = PWM0 alt, since 18 is I2S)
    //   num_leds: number of APA106 LEDs in chain
    // Returns true on success.
    bool init(int gpio_pin = 12, int num_leds = 1);

    // Update LED colour + brightness.  Call from main loop (~20 Hz).
    //   waveform:   LFO waveform (selects jewel-tone colour)
    //   lfo_output: raw LFO value in [–1, +1]
    //   lfo_depth:  0.0–1.0
    //   gate:       true when trigger is held
    //   freq:       oscillator base frequency in Hz (30–8000)
    //               higher pitch → desaturated (pastel)
    void update(LfoWave waveform, float lfo_output,
                float lfo_depth, bool gate, float freq);

    // Triple white blink at 25% brightness (blocking, ~400 ms).
    // Call on preset save for visual confirmation.
    void blinkSave();

    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};
