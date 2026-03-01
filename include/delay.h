#pragma once

#include <vector>

// ─── Tape delay ─────────────────────────────────────────────────────
//
// Circular-buffer delay with feedback and dry/wet mix.
// NaN/Inf-safe: resets buffer on detection in the feedback path.

class TapeDelay {
public:
    void  init(float sampleRate, float maxTimeSec);
    void  setTime(float sec);
    void  setFeedback(float fb);    // 0 – 0.95
    void  setMix(float mix);        // 0 – 1.0
    float process(float input);
    void  reset();

private:
    std::vector<float> buf_;
    int   writePos_     = 0;
    int   delaySamples_ = 1;
    float feedback_     = 0.0f;
    float mix_          = 0.0f;
    float sr_           = 48000.0f;
};
