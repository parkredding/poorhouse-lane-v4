// delay.cpp — Tape delay with feedback and NaN/Inf protection

#include "delay.h"
#include <algorithm>
#include <cmath>

void TapeDelay::init(float sampleRate, float maxTimeSec)
{
    sr_ = sampleRate;
    int maxSamples = static_cast<int>(sampleRate * maxTimeSec) + 1;
    buf_.assign(maxSamples, 0.0f);
    writePos_     = 0;
    delaySamples_ = 1;
}

void TapeDelay::setTime(float sec)
{
    int samples = static_cast<int>(sec * sr_);
    delaySamples_ = std::clamp(samples, 1,
                               static_cast<int>(buf_.size()) - 1);
}

void TapeDelay::setFeedback(float fb)
{
    feedback_ = std::clamp(fb, 0.0f, 0.95f);
}

void TapeDelay::setMix(float mix)
{
    mix_ = std::clamp(mix, 0.0f, 1.0f);
}

void TapeDelay::reset()
{
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    writePos_ = 0;
}

float TapeDelay::process(float input)
{
    int readPos = writePos_ - delaySamples_;
    if (readPos < 0) readPos += static_cast<int>(buf_.size());

    float wet      = buf_[readPos];
    float writeVal = input + wet * feedback_;

    // NaN/Inf protection — reset the entire buffer
    if (!std::isfinite(writeVal)) {
        reset();
        return input;
    }

    buf_[writePos_] = writeVal;
    writePos_ = (writePos_ + 1) % static_cast<int>(buf_.size());

    return input * (1.0f - mix_) + wet * mix_;
}
