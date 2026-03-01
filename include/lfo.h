#pragma once

// ─── Low-frequency oscillator (sine) ────────────────────────────────

class LFO {
public:
    void setSampleRate(float sr);
    void setRate(float hz);

    // Returns next sample in [–1, +1]
    float tick();

private:
    float phase_    = 0.0f;
    float phaseInc_ = 0.0f;
    float rate_     = 1.0f;
    float sr_       = 48000.0f;

    void updateInc();
};
