// lfo.cpp — Sine-wave low-frequency oscillator

#include "lfo.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void LFO::setSampleRate(float sr) { sr_ = sr; updateInc(); }
void LFO::setRate(float hz)       { rate_ = hz; updateInc(); }

void LFO::updateInc()
{
    phaseInc_ = rate_ / sr_;
}

float LFO::tick()
{
    float out = std::sin(2.0f * static_cast<float>(M_PI) * phase_);
    phase_ += phaseInc_;
    if (phase_ >= 1.0f) phase_ -= 1.0f;
    return out;
}
