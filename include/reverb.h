#pragma once

#include <vector>

// ─── Schroeder algorithmic reverb ───────────────────────────────────
//
// 4 parallel feedback comb filters → 2 series allpass filters.
// NaN/Inf-safe: each comb and allpass resets on detection.

class Reverb {
public:
    void  init(float sampleRate);
    void  setSize(float size);      // 0 – 1.0
    void  setMix(float mix);        // 0 – 1.0
    float process(float input);
    void  reset();

private:
    struct Comb {
        std::vector<float> buf;
        int   pos      = 0;
        float feedback = 0.0f;
        float process(float input);
        void  reset();
    };

    struct Allpass {
        std::vector<float> buf;
        int pos = 0;
        float process(float input);
        void  reset();
    };

    Comb    combs_[4];
    Allpass aps_[2];
    float   size_ = 0.3f;
    float   mix_  = 0.0f;
};
