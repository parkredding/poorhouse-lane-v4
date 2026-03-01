// delay.cpp — Analog tape delay with slew, modulation, and feedback filtering

#include "delay.h"
#include "dsp_utils.h"
#include <algorithm>
#include <cmath>

static constexpr float TWO_PI = 6.28318530718f;

void TapeDelay::init(float sampleRate, float maxTimeSec)
{
    sr_ = sampleRate;
    maxSamples_ = static_cast<int>(sampleRate * maxTimeSec) + 1;
    buf_.assign(maxSamples_, 0.0f);
    writePos_    = 0;
    readPos_     = 1.0f;
    targetDelay_ = 1.0f;

    // Default slew (overridden by setRepitchRate)
    slewRate_ = 4.0f;

    // Tape modulation
    wobblePhase_  = 0.0f;
    flutterPhase_ = 0.0f;
    wobbleInc_    = 0.5f  / sr_;           // 0.5 Hz
    flutterInc_   = 3.5f  / sr_;           // 3.5 Hz
    wobbleDepth_  = 0.003f * sr_;           // ±3 ms in samples (tape transport drift)
    flutterDepth_ = 0.001f * sr_;          // ±1 ms in samples (tape tension variation)

    // Feedback HP 80 Hz:  y[n] = c * (y[n-1] + x[n] - x[n-1])
    hpCoeff_  = std::exp(-TWO_PI * 80.0f / sr_);
    hpState_  = 0.0f;
    hpPrevIn_ = 0.0f;

    // Feedback LP 5 kHz:  y[n] = y[n-1] + c * (x[n] - y[n-1])
    lpCoeff_  = 1.0f - std::exp(-TWO_PI * 5000.0f / sr_);
    lpState_  = 0.0f;
}

void TapeDelay::setTime(float sec)
{
    float samples = sec * sr_;
    targetDelay_ = std::clamp(samples, 1.0f,
                              static_cast<float>(maxSamples_ - 1));
}

void TapeDelay::setFeedback(float fb)
{
    feedback_ = std::clamp(fb, 0.0f, 0.95f);
}

void TapeDelay::setMix(float mix)
{
    mix_ = std::clamp(mix, 0.0f, 1.0f);
}

void TapeDelay::setRepitchRate(float rate)
{
    rate = std::clamp(rate, 0.001f, 1.0f);
    slewRate_ = static_cast<float>(maxSamples_)
              / (2.0f * rate * sr_);
}

void TapeDelay::reset()
{
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    writePos_  = 0;
    readPos_   = targetDelay_;
    hpState_   = 0.0f;
    hpPrevIn_  = 0.0f;
    lpState_   = 0.0f;
}

float TapeDelay::process(float input)
{
    // ── Tape modulation ──────────────────────────────────────────────
    float wobble  = std::sin(wobblePhase_  * TWO_PI) * wobbleDepth_;
    float flutter = std::sin(flutterPhase_ * TWO_PI) * flutterDepth_;
    wobblePhase_  += wobbleInc_;
    flutterPhase_ += flutterInc_;
    if (wobblePhase_  >= 1.0f) wobblePhase_  -= 1.0f;
    if (flutterPhase_ >= 1.0f) flutterPhase_ -= 1.0f;

    float modulatedTarget = targetDelay_ + wobble + flutter;
    modulatedTarget = std::clamp(modulatedTarget, 1.0f,
                                 static_cast<float>(maxSamples_ - 1));

    // ── Slew read position toward modulated target ───────────────────
    float diff = modulatedTarget - readPos_;
    if (std::abs(diff) <= slewRate_) {
        readPos_ = modulatedTarget;
    } else {
        readPos_ += (diff > 0.0f) ? slewRate_ : -slewRate_;
    }

    // ── Read with linear interpolation ───────────────────────────────
    float readIdx = static_cast<float>(writePos_) - readPos_;
    if (readIdx < 0.0f) readIdx += static_cast<float>(maxSamples_);

    int   idx0 = static_cast<int>(readIdx);
    float frac = readIdx - static_cast<float>(idx0);
    int   idx1 = (idx0 + 1) % maxSamples_;

    float wet = buf_[idx0] * (1.0f - frac) + buf_[idx1] * frac;

    // ── Feedback path: HP → LP → saturation ─────────────────────────
    float fb = wet * feedback_;

    // High-pass 80 Hz (prevent mud buildup)
    hpState_ = hpCoeff_ * (hpState_ + fb - hpPrevIn_);
    hpPrevIn_ = fb;
    fb = hpState_;

    // Low-pass 5 kHz (tape damping)
    lpState_ += lpCoeff_ * (fb - lpState_);
    fb = lpState_;

    // Tape saturation — 30% driven blend preserves dynamics while
    // compressing peaks, lets high feedback sing rather than clip
    constexpr float TAPE_SAT = 0.3f;
    float driven = dsp::fast_tanh(fb * (1.0f + TAPE_SAT * 2.0f));
    fb = fb * (1.0f - TAPE_SAT) + driven * TAPE_SAT;

    // NaN/Inf protection
    if (!std::isfinite(fb)) {
        reset();
        return input;
    }

    // ── Write to buffer (±10 headroom lets feedback build before clamping)
    float writeVal = input + fb;
    buf_[writePos_] = std::clamp(dsp::sanitize(writeVal), -10.0f, 10.0f);
    writePos_ = (writePos_ + 1) % maxSamples_;

    return input * (1.0f - mix_) + wet * mix_;
}
