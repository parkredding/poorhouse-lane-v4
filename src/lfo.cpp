// lfo.cpp — Low-frequency oscillator with 5 deterministic waveform shapes
//
// Sine, Triangle, Square, Ramp Up, Ramp Down.
// No antialiasing needed — LFO operates at sub-audio rates (0.1–20 Hz).

#include "lfo.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void LFO::setSampleRate(float sr) { sr_ = sr; updateInc(); }
void LFO::setRate(float hz)       { rate_ = hz; updateInc(); }
void LFO::setWaveform(LfoWave w)  { wave_ = w; }
void LFO::resetPhase()            { phase_ = 0.0f; }

void LFO::updateInc()
{
    phaseInc_ = rate_ / sr_;
}

float LFO::tick()
{
    float out = 0.0f;

    switch (wave_) {
    case LfoWave::Sine:
        out = std::sin(2.0f * static_cast<float>(M_PI) * phase_);
        break;
    case LfoWave::Triangle:
        out = 4.0f * std::fabs(phase_ - 0.5f) - 1.0f;
        break;
    case LfoWave::Square:
        out = (phase_ < 0.5f) ? 1.0f : -1.0f;
        break;
    case LfoWave::RampUp:
        out = 2.0f * phase_ - 1.0f;
        break;
    case LfoWave::RampDown:
        out = 1.0f - 2.0f * phase_;
        break;
    default:
        break;
    }

    // Advance phase
    phase_ += phaseInc_;
    if (phase_ >= 1.0f)
        phase_ -= 1.0f;

    return out;
}
