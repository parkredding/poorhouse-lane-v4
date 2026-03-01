// main.cpp — Milestone 5: Full DSP Engine + Parameter Mapping
//
// Usage:
//   dubsiren [--device <alsa_hw>] [--simulate]
//
// Signal chain:
//   LFO + Pitch-env → Oscillator → Envelope → Filter → Delay → Reverb → DAC
//
// Encoder banks (Shift toggles A / B):
//   Enc 1:  A = Base Freq (50–2000 Hz)     B = LFO Depth (0–100%)
//   Enc 2:  A = LFO Rate (0.1–20 Hz)       B = Reverb Size (0–100%)
//   Enc 3:  A = Filter Cutoff (20–20 kHz)   B = Filter Resonance (0–95%)
//   Enc 4:  A = Delay Time (1 ms–2.0 s)     B = Delay Mix (0–100%)
//   Enc 5:  A = Delay Feedback (0–95%)      B = Reverb Mix (0–100%)
//
// Buttons:
//   Trigger  (GPIO 4)   Gate volume envelope
//   Shift    (GPIO 15)  Hold for Bank B
//   Waveform (GPIO 5)   Cycle Sine → Square → Saw → Triangle
//
// Pitch envelope switch (GPIO 9/10):
//   Rise / Off / Fall — sweeps pitch while gate is held

#include <cstdio>
#include <cstring>
#include <csignal>
#include <cmath>
#include <atomic>
#include <algorithm>

#include "gpio_hw.h"
#include "oscillator.h"
#include "audio_engine.h"
#include "lfo.h"
#include "filter.h"
#include "delay.h"
#include "reverb.h"

static volatile sig_atomic_t g_running = 1;

static void on_signal(int) { g_running = 0; }

// ─── Shared state (main thread → audio thread) ─────────────────────

// Bank A
static std::atomic<float> g_freq{440.0f};
static std::atomic<float> g_lfo_rate{1.0f};
static std::atomic<float> g_filter_cutoff{20000.0f};
static std::atomic<float> g_delay_time{0.3f};
static std::atomic<float> g_delay_feedback{0.0f};

// Bank B
static std::atomic<float> g_lfo_depth{0.0f};
static std::atomic<float> g_reverb_size{0.3f};
static std::atomic<float> g_filter_reso{0.0f};
static std::atomic<float> g_delay_mix{0.0f};
static std::atomic<float> g_reverb_mix{0.0f};

// Controls
static std::atomic<int>   g_waveform{0};
static std::atomic<bool>  g_gate{false};
static std::atomic<bool>  g_shift{false};
static std::atomic<int>   g_pitch_env{0};     // –1 fall, 0 off, +1 rise

// ─── Constants ──────────────────────────────────────────────────────

static constexpr float SEMITONE     = 1.0594630943592953f;  // 2^(1/12)
static constexpr float INV_SEMITONE = 1.0f / SEMITONE;
static constexpr float LOG_STEP     = 1.10f;                // 10 % per click
static constexpr float INV_LOG_STEP = 1.0f / LOG_STEP;
static constexpr float PCT_STEP     = 0.02f;                // 2 % per click

// ─── Helpers ────────────────────────────────────────────────────────

static const char* waveform_name(int w)
{
    static const char* names[] = {"Sine", "Square", "Saw", "Triangle"};
    return names[w % static_cast<int>(Waveform::COUNT)];
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n"
        "\n"
        "  --device <hw>   ALSA device string  (default: hw:0,0)\n"
        "  --simulate      Bypass GPIO — use keyboard simulation\n"
        "  -h, --help      Show this help\n", prog);
}

// ─── main ───────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
    std::string device = "hw:0,0";
    bool simulate = false;

    // ── Parse CLI args ──────────────────────────────────────────────
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--device") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--device requires an argument\n");
                return 1;
            }
            device = argv[++i];
        } else if (strcmp(argv[i], "--simulate") == 0) {
            simulate = true;
        } else if (strcmp(argv[i], "-h") == 0 ||
                   strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    // ── Banner ──────────────────────────────────────────────────────
    printf("Poorhouse Lane - Siren V4\n");
    printf("Milestone 5: Full DSP Engine + Parameter Mapping\n");
    printf("  ALSA device : %s\n", device.c_str());
    printf("  Mode        : %s\n\n", simulate ? "SIMULATE" : "GPIO");

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    // ── Audio-thread-only DSP state ─────────────────────────────────
    //
    // These objects are captured by reference in the audio callback
    // and are only ever touched from the audio thread.

    Oscillator osc;
    LFO        lfo;
    MoogFilter filter;
    TapeDelay  delay;
    Reverb     reverb;

    float env_level      = 0.0f;
    float attack_rate    = 0.0f;
    float release_rate   = 0.0f;
    float pitch_env_mult = 1.0f;
    bool  prev_gate      = false;
    float rise_factor    = 1.0f;
    float fall_factor    = 1.0f;

    constexpr int SAMPLE_RATE = 48000;

    AudioEngine audio;

    // ── Audio callback ──────────────────────────────────────────────

    auto audio_cb = [&](float* buf, int frames) {
        namespace mo = std::memory_order;
        const auto rlx = std::memory_order_relaxed;

        // Load all parameters once per frame
        float base_freq = g_freq.load(rlx);
        int   waveform  = g_waveform.load(rlx);
        bool  gate      = g_gate.load(rlx);
        float lfo_r     = g_lfo_rate.load(rlx);
        float lfo_d     = g_lfo_depth.load(rlx);
        float cutoff    = g_filter_cutoff.load(rlx);
        float reso      = g_filter_reso.load(rlx);
        float dly_t     = g_delay_time.load(rlx);
        float dly_fb    = g_delay_feedback.load(rlx);
        float dly_mix   = g_delay_mix.load(rlx);
        float rev_sz    = g_reverb_size.load(rlx);
        float rev_mix   = g_reverb_mix.load(rlx);
        int   pe_mode   = g_pitch_env.load(rlx);

        // Update DSP modules (once per frame)
        lfo.setRate(lfo_r);
        osc.setWaveform(static_cast<Waveform>(waveform));
        filter.setCutoff(cutoff);
        filter.setResonance(reso);
        delay.setTime(dly_t);
        delay.setFeedback(dly_fb);
        delay.setMix(dly_mix);
        reverb.setSize(rev_sz);
        reverb.setMix(rev_mix);

        // Gate edge — reset pitch envelope on new trigger
        if (gate && !prev_gate)
            pitch_env_mult = 1.0f;
        prev_gate = gate;

        // Per-sample processing
        for (int i = 0; i < frames; i++) {

            // Pitch envelope (1 octave / sec)
            if (gate && pe_mode != 0) {
                pitch_env_mult *= (pe_mode > 0) ? rise_factor
                                                : fall_factor;
                pitch_env_mult = std::clamp(pitch_env_mult, 0.25f, 4.0f);
            }

            // LFO → exponential pitch modulation
            float lfo_out = lfo.tick();
            float freq = base_freq * pitch_env_mult
                       * std::exp2(lfo_out * lfo_d);
            freq = std::clamp(freq, 20.0f, 20000.0f);
            osc.setFrequency(freq);

            // Oscillator
            float s = osc.tick();

            // Volume envelope (linear attack / release)
            if (gate) {
                env_level += attack_rate;
                if (env_level > 1.0f) env_level = 1.0f;
            } else {
                env_level -= release_rate;
                if (env_level < 0.0f) env_level = 0.0f;
            }
            s *= env_level;

            // Effects chain
            s = filter.process(s);
            s = delay.process(s);
            s = reverb.process(s);

            buf[i] = s;
        }
    };

    // ── Init audio engine ───────────────────────────────────────────

    if (audio.init(device, SAMPLE_RATE, audio_cb)) {
        float sr = static_cast<float>(audio.sampleRate());
        osc.setSampleRate(sr);
        lfo.setSampleRate(sr);
        filter.setSampleRate(sr);
        delay.init(sr, 2.5f);
        reverb.init(sr);

        attack_rate  = 1.0f / (0.005f * sr);           // ~5 ms
        release_rate = 1.0f / (0.050f * sr);            // ~50 ms
        rise_factor  = std::pow(2.0f, 1.0f / sr);      // 1 oct/sec up
        fall_factor  = std::pow(0.5f, 1.0f / sr);      // 1 oct/sec down

        audio.start();
    } else {
        fprintf(stderr, "Audio engine init failed\n");
    }

    // ── GPIO callbacks ──────────────────────────────────────────────

    HwCallbacks cb;

    cb.on_encoder = [](int id, int dir) {
        bool shift = g_shift.load();

        if (!shift) {
            // ── Bank A ─────────────────────────────────────────────
            switch (id) {
            case 0: {
                float f = g_freq.load();
                f *= (dir > 0) ? SEMITONE : INV_SEMITONE;
                f = std::clamp(f, 50.0f, 2000.0f);
                g_freq.store(f);
                printf("  [A] FREQ     %.1f Hz\n", f);
                break;
            }
            case 1: {
                float r = g_lfo_rate.load();
                r *= (dir > 0) ? LOG_STEP : INV_LOG_STEP;
                r = std::clamp(r, 0.1f, 20.0f);
                g_lfo_rate.store(r);
                printf("  [A] LFO RATE %.1f Hz\n", r);
                break;
            }
            case 2: {
                float c = g_filter_cutoff.load();
                c *= (dir > 0) ? SEMITONE : INV_SEMITONE;
                c = std::clamp(c, 20.0f, 20000.0f);
                g_filter_cutoff.store(c);
                printf("  [A] CUTOFF   %.0f Hz\n", c);
                break;
            }
            case 3: {
                float t = g_delay_time.load();
                t *= (dir > 0) ? LOG_STEP : INV_LOG_STEP;
                t = std::clamp(t, 0.001f, 2.0f);
                g_delay_time.store(t);
                printf("  [A] DLY TIME %.0f ms\n", t * 1000.0f);
                break;
            }
            case 4: {
                float fb = g_delay_feedback.load() + dir * PCT_STEP;
                fb = std::clamp(fb, 0.0f, 0.95f);
                g_delay_feedback.store(fb);
                printf("  [A] DLY FB   %.0f%%\n", fb * 100.0f);
                break;
            }
            }

        } else {
            // ── Bank B ─────────────────────────────────────────────
            switch (id) {
            case 0: {
                float d = g_lfo_depth.load() + dir * PCT_STEP;
                d = std::clamp(d, 0.0f, 1.0f);
                g_lfo_depth.store(d);
                printf("  [B] LFO DEP  %.0f%%\n", d * 100.0f);
                break;
            }
            case 1: {
                float s = g_reverb_size.load() + dir * PCT_STEP;
                s = std::clamp(s, 0.0f, 1.0f);
                g_reverb_size.store(s);
                printf("  [B] REV SIZE %.0f%%\n", s * 100.0f);
                break;
            }
            case 2: {
                float r = g_filter_reso.load() + dir * PCT_STEP;
                r = std::clamp(r, 0.0f, 0.95f);
                g_filter_reso.store(r);
                printf("  [B] RESO     %.0f%%\n", r * 100.0f);
                break;
            }
            case 3: {
                float m = g_delay_mix.load() + dir * PCT_STEP;
                m = std::clamp(m, 0.0f, 1.0f);
                g_delay_mix.store(m);
                printf("  [B] DLY MIX  %.0f%%\n", m * 100.0f);
                break;
            }
            case 4: {
                float m = g_reverb_mix.load() + dir * PCT_STEP;
                m = std::clamp(m, 0.0f, 1.0f);
                g_reverb_mix.store(m);
                printf("  [B] REV MIX  %.0f%%\n", m * 100.0f);
                break;
            }
            }
        }
    };

    cb.on_button = [](int id, bool pressed) {
        static const char *btn_name[] = {"Trigger", "Shift", "Waveform"};

        switch (id) {
        case 0:
            // Trigger → gate
            g_gate.store(pressed);
            printf("  %-9s %s\n", btn_name[0],
                   pressed ? "GATE ON" : "GATE OFF");
            break;
        case 1:
            // Shift → bank select
            g_shift.store(pressed);
            printf("  BANK %s\n", pressed ? "B" : "A");
            break;
        case 2:
            // Waveform → cycle shape (on press only)
            if (pressed) {
                int w = (g_waveform.load() + 1)
                      % static_cast<int>(Waveform::COUNT);
                g_waveform.store(w);
                printf("  WAVE  %s\n", waveform_name(w));
            }
            break;
        }
    };

    cb.on_pitch_switch = [](int pos) {
        g_pitch_env.store(pos);
        static const char *lbl[] = {"FALL", "OFF", "RISE"};
        printf("  PITCH-ENV  %s\n", lbl[pos + 1]);
    };

    // ── Initialise GPIO / simulate ──────────────────────────────────

    GpioHw hw;
    if (!hw.init(simulate, cb)) {
        fprintf(stderr, "Failed to initialise hardware.\n");
        audio.shutdown();
        return 1;
    }

    printf("\nDefaults:\n");
    printf("  Freq: %.0f Hz  Wave: %s\n",
           g_freq.load(), waveform_name(g_waveform.load()));
    printf("  LFO: %.1f Hz @ %.0f%%  Filter: %.0f Hz / %.0f%%\n",
           g_lfo_rate.load(), g_lfo_depth.load() * 100.0f,
           g_filter_cutoff.load(), g_filter_reso.load() * 100.0f);
    printf("  Delay: %.0f ms  FB %.0f%%  Mix %.0f%%\n",
           g_delay_time.load() * 1000.0f,
           g_delay_feedback.load() * 100.0f,
           g_delay_mix.load() * 100.0f);
    printf("  Reverb: Size %.0f%%  Mix %.0f%%\n",
           g_reverb_size.load() * 100.0f,
           g_reverb_mix.load() * 100.0f);
    printf("\nListening for events (Ctrl-C to quit) ...\n\n");

    // ── Main loop ───────────────────────────────────────────────────
    while (g_running)
        hw.poll();

    printf("\nShutting down.\n");
    audio.shutdown();
    hw.shutdown();
    return 0;
}
