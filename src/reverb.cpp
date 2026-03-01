// reverb.cpp — Schroeder algorithmic reverb
//
// 4 parallel feedback comb filters summed, then passed through
// 2 series allpass filters.  Input is pre-scaled to prevent buildup.

#include "reverb.h"
#include "dsp_utils.h"
#include <cmath>
#include <algorithm>

// Comb delay lengths (samples at 48 kHz, scaled in init)
static constexpr int COMB_LENGTHS[]    = {1687, 1753, 1619, 1553};
// Allpass delay lengths
static constexpr int AP_LENGTHS[]      = {241, 557};

static constexpr float INPUT_GAIN = 0.015f;   // pre-scale into combs
static constexpr float AP_GAIN    = 0.5f;     // allpass coefficient

// ─── Comb filter ────────────────────────────────────────────────────

float Reverb::Comb::process(float input)
{
    float out = buf[pos];

    buf[pos] = dsp::sanitize(input + out * feedback);

    pos = (pos + 1) % static_cast<int>(buf.size());
    return out;
}

void Reverb::Comb::reset()
{
    std::fill(buf.begin(), buf.end(), 0.0f);
}

// ─── Allpass filter ─────────────────────────────────────────────────

float Reverb::Allpass::process(float input)
{
    float delayed = buf[pos];
    float w       = input + AP_GAIN * delayed;

    buf[pos] = dsp::sanitize(w);

    pos = (pos + 1) % static_cast<int>(buf.size());
    return delayed - AP_GAIN * w;
}

void Reverb::Allpass::reset()
{
    std::fill(buf.begin(), buf.end(), 0.0f);
}

// ─── Reverb ─────────────────────────────────────────────────────────

void Reverb::init(float sampleRate)
{
    float scale = sampleRate / 48000.0f;

    for (int i = 0; i < 4; i++) {
        int len = std::max(1, static_cast<int>(COMB_LENGTHS[i] * scale));
        combs_[i].buf.assign(len, 0.0f);
        combs_[i].pos = 0;
    }
    for (int i = 0; i < 2; i++) {
        int len = std::max(1, static_cast<int>(AP_LENGTHS[i] * scale));
        aps_[i].buf.assign(len, 0.0f);
        aps_[i].pos = 0;
    }

    setSize(size_);
}

void Reverb::setSize(float size)
{
    size_ = size;
    float fb = 0.7f + size * 0.25f;        // 0.70 – 0.95
    for (auto& c : combs_) c.feedback = fb;
}

void Reverb::setMix(float mix) { mix_ = mix; }

void Reverb::reset()
{
    for (auto& c : combs_)  c.reset();
    for (auto& a : aps_)    a.reset();
}

float Reverb::process(float input)
{
    float wet    = 0.0f;
    float scaled = input * INPUT_GAIN;

    for (auto& c : combs_)
        wet += c.process(scaled);

    for (auto& a : aps_)
        wet = a.process(wet);

    return input * (1.0f - mix_) + wet * mix_;
}
