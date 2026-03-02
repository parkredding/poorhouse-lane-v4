#pragma once

#include <cstdint>

// ─── LFO waveform shapes ──────────────────────────────────────────
enum class LfoWave { Sine, Triangle, Square, RampUp, RampDown, SandH, COUNT };

// ─── Low-frequency oscillator ──────────────────────────────────────
//
// 6 waveform shapes with phase retrigger and sample-and-hold.

class LFO {
public:
    void setSampleRate(float sr);
    void setRate(float hz);
    void setWaveform(LfoWave w);

    LfoWave waveform() const { return wave_; }

    // Reset phase to 0 (call on trigger press for consistent attacks)
    void resetPhase();

    // Returns next sample in [–1, +1]
    float tick();

private:
    float    phase_    = 0.0f;
    float    phaseInc_ = 0.0f;
    float    rate_     = 1.0f;
    float    sr_       = 48000.0f;
    LfoWave  wave_     = LfoWave::Sine;

    // Sample-and-Hold state
    float    shValue_  = 0.0f;
    uint32_t rngState_ = 123456789u;

    void updateInc();
};
