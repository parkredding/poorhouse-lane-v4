// main.cpp — Milestone 5: Full DSP Engine + Parameter Mapping
//
// Usage:
//   dubsiren [--device <alsa_hw>] [--simulate]
//
// Signal chain:
//   LFO + Pitch-env → Oscillator → Envelope → Filter(+env) → Delay → Reverb → Limiter → DAC
//
// Encoder banks (Shift toggles A / B):
//   Enc 1:  A = Base Freq (30–8000 Hz)     B = LFO Depth (0–100%)
//   Enc 2:  A = LFO Rate (0.1–20 Hz)       B = Release Time (10 ms–3 s)
//   Enc 3:  A = Filter Cutoff (20–20 kHz)   B = Filter Resonance (0–95%)
//   Enc 4:  A = Delay Time (1 ms–1.0 s)     B = Delay Mix (0–100%)
//   Enc 5:  A = Delay Feedback (0–95%)      B = Reverb Mix (0–100%)
//
// Buttons:
//   Trigger  (GPIO 4)   Gate volume envelope; resets LFO phase
//   Shift    (GPIO 15)  Hold for Bank B; triple-click = pitch-delay link
//   Preset   (GPIO 5)   Cycle dub siren preset; Shift+Preset = cycle LFO shape
//
// Presets (GPIO 5, Bank A):
//   1. Lickshot  2. Machine Gun  3. Droppa  4. Laser Sweep  5. Dub Siren
//
// LFO shapes (Shift+GPIO 5):
//   Sine → Triangle → Square → RampUp → RampDown
//
// Pitch envelope switch (GPIO 9/10):
//   Rise / Off / Fall — sweeps pitch AND filter on trigger release

#include <cstdio>
#include <cstring>
#include <csignal>
#include <cmath>
#include <atomic>
#include <algorithm>
#include <chrono>

#include "gpio_hw.h"
#include "oscillator.h"
#include "audio_engine.h"
#include "lfo.h"
#include "filter.h"
#include "delay.h"
#include "reverb.h"
#include "dsp_utils.h"

static volatile sig_atomic_t g_running = 1;

static void on_signal(int) { g_running = 0; }

// ─── Shared state (main thread → audio thread) ─────────────────────

// Bank A
static std::atomic<float> g_freq{440.0f};
static std::atomic<float> g_lfo_rate{0.35f};
static std::atomic<float> g_filter_cutoff{8000.0f};
static std::atomic<float> g_delay_time{0.375f};
static std::atomic<float> g_delay_feedback{0.55f};

// Bank B
static std::atomic<float> g_lfo_depth{0.35f};
static std::atomic<float> g_release_time{0.050f};  // 50 ms default
static std::atomic<float> g_filter_reso{0.0f};
static std::atomic<float> g_delay_mix{0.30f};
static std::atomic<float> g_reverb_mix{0.35f};

// Fixed (no encoder control)
static constexpr float REVERB_SIZE = 0.65f;

// Controls
static std::atomic<int>   g_waveform{0};
static std::atomic<int>   g_lfo_waveform{0};  // LfoWave index
static std::atomic<bool>  g_gate{false};
static std::atomic<bool>  g_shift{false};
static std::atomic<int>   g_pitch_env{0};     // –1 fall, 0 off, +1 rise

// Pitch-delay link (secret mode: triple-click Shift to toggle)
static std::atomic<bool>  g_delay_link{false};
static std::atomic<float> g_delay_time_eff{0.375f}; // effective delay (may be freq-scaled)

// ─── Per-parameter step sizes (base, before acceleration) ───────────
//
// Multiplicative params: acceleration raises base step to a power
//   e.g. pow(SEMITONE, accel) → 1–4 semitones per click
// Additive params:       acceleration multiplies the base step
//   e.g. 0.04 * accel → 4–16 % per click

// Bank A — multiplicative
static constexpr float FREQ_STEP       = 1.0594630943592953f;  // 1 semitone = 2^(1/12)
static constexpr float LFO_RATE_STEP   = 1.12f;                // 12 % per click
static constexpr float CUTOFF_STEP     = 1.122462048309373f;   // 2 semitones = 2^(1/6)
static constexpr float DELAY_TIME_STEP = 1.10f;                // 10 % per click

// Bank A — additive
static constexpr float DELAY_FB_STEP   = 0.04f;                // ~24 steps across 0–95%

// Bank B — additive
static constexpr float LFO_DEPTH_STEP   = 0.04f;               // 4 % per click
static constexpr float RELEASE_TIME_STEP = 1.15f;              // 15 % per click (log)
static constexpr float FILTER_RESO_STEP = 0.03f;               // 3 % (fine near self-osc)
static constexpr float DELAY_MIX_STEP  = 0.05f;                // 5 % per click
static constexpr float REVERB_MIX_STEP = 0.05f;                // 5 % per click

// Pitch-delay link mode
static constexpr float FREQ_STEP_LINKED = 1.165f;              // ~minor-third jumps
static constexpr float REF_FREQ         = 440.0f;              // neutral scaling point

static float update_delay_eff();   // forward declaration

// ─── Dub Siren Presets ──────────────────────────────────────────────
//
// GPIO 5 (Bank A) cycles through these presets.  Inspired by the
// Benidub DS71 — analog square-wave oscillator through 12dB/oct
// low-pass filter, aggressive LFO-driven siren sounds.  Fat and
// punchy with deterministic LFO shapes only.  All knobs remain
// live after selecting a preset so the performer can tweak from any
// starting point.

struct DubPreset {
    const char* name;
    int         waveform;       // Waveform enum index
    int         lfo_wave;       // LfoWave enum index
    float       freq;           // base frequency (Hz)
    float       lfo_rate;       // LFO rate (Hz)
    float       lfo_depth;      // LFO depth (0–1)
    float       filter_cutoff;  // filter cutoff (Hz)
    float       filter_reso;    // filter resonance (0–0.95)
    float       delay_time;     // delay time (seconds)
    float       delay_feedback; // delay feedback (0–0.95)
    float       delay_mix;      // delay wet/dry (0–1)
    float       reverb_mix;     // reverb wet/dry (0–1)
    float       release_time;   // envelope release (seconds)
};

static constexpr int NUM_PRESETS = 5;

static const DubPreset PRESETS[NUM_PRESETS] = {
    //  ── 1. Lickshot ───────────────────────────────────────────────
    //  The classic laser-gun sound.  Square wave with fast triangle
    //  LFO sweeping pitch and filter together.  Lower cutoff lets the
    //  filter sweep open and close audibly.  Punchy attack, spring
    //  reverb and tape echo for sound-system depth.
    {
        "Lickshot",
        1,              // Square
        1,              // LFO: Triangle
        800.0f,         // freq  (mid tone)
        8.0f,           // lfo_rate  (fast sweep)
        0.80f,          // lfo_depth  (deep — drives filter sweep hard)
        3500.0f,        // filter_cutoff  (low enough for audible sweep)
        0.40f,          // filter_reso  (squelchy bite on each sweep)
        0.300f,         // delay_time  (dub echo)
        0.55f,          // delay_feedback  (long repeats)
        0.40f,          // delay_mix  (prominent echo)
        0.35f,          // reverb_mix  (spring tank)
        0.200f,         // release_time  (punchy but with tail)
    },
    //  ── 2. Machine Gun ────────────────────────────────────────────
    //  Rapid-fire stutter.  Square LFO chops the pitch and filter
    //  between high-bright and low-dark for aggressive on/off bursts.
    //  Tight delay stutter, cavernous spring reverb.
    {
        "Machine Gun",
        1,              // Square
        2,              // LFO: Square
        1000.0f,        // freq  (punchy mid-high)
        14.0f,          // lfo_rate  (rapid fire)
        0.85f,          // lfo_depth  (extreme for hard cuts)
        4000.0f,        // filter_cutoff  (filter chops with pitch)
        0.30f,          // filter_reso  (adds bite to each burst)
        0.180f,         // delay_time  (tight stutter echo)
        0.60f,          // delay_feedback  (self-reinforcing bursts)
        0.40f,          // delay_mix  (heavy stutter)
        0.30f,          // reverb_mix  (room around the bursts)
        0.120f,         // release_time  (snappy cutoff)
    },
    //  ── 3. Droppa ───────────────────────────────────────────────────
    //  Descending siren wail.  Ramp-down LFO pulls pitch and filter
    //  down together — each cycle starts bright and falls into a
    //  fat, dark growl.  Heavy dub delay and reverb add weight.
    {
        "Droppa",
        1,              // Square
        4,              // LFO: RampDown
        1000.0f,        // freq  (starts high, drops)
        4.0f,           // lfo_rate  (medium sweep speed)
        0.75f,          // lfo_depth  (wide falling range)
        3000.0f,        // filter_cutoff  (drops into darkness)
        0.45f,          // filter_reso  (squelchy wah on descent)
        0.375f,         // delay_time  (dub echo)
        0.65f,          // delay_feedback  (long cascading trails)
        0.45f,          // delay_mix  (heavy dub echo)
        0.40f,          // reverb_mix  (deep spring wash)
        0.350f,         // release_time  (let the drop breathe)
    },
    //  ── 4. Laser Sweep ────────────────────────────────────────────
    //  Rising laser blast.  Ramp-up LFO sweeps pitch and filter
    //  upward — each cycle growls low then zaps bright.  Resonant
    //  filter adds squelch, delay trails scatter into space.
    {
        "Laser Sweep",
        1,              // Square
        3,              // LFO: RampUp
        900.0f,         // freq  (lower start for wider sweep)
        5.0f,           // lfo_rate  (medium sweep)
        0.80f,          // lfo_depth  (wide pitch + filter range)
        3000.0f,        // filter_cutoff  (opens up dramatically)
        0.50f,          // filter_reso  (heavy squelch on rise)
        0.300f,         // delay_time  (echo trails)
        0.60f,          // delay_feedback  (cascading zaps)
        0.40f,          // delay_mix  (prominent echo)
        0.35f,          // reverb_mix  (spring splash)
        0.280f,         // release_time  (sharp into reverb tail)
    },
    //  ── 5. Dub Siren ──────────────────────────────────────────────
    //  Classic sound-system siren.  Slow sine LFO sweeps pitch and
    //  filter together for that smooth wah character.  Heavy delay
    //  and reverb create the deep dub wash.
    {
        "Dub Siren",
        1,              // Square
        0,              // LFO: Sine
        700.0f,         // freq  (warm mid)
        3.5f,           // lfo_rate  (classic siren speed)
        0.65f,          // lfo_depth  (full sweep)
        2800.0f,        // filter_cutoff  (warm — sweep is audible)
        0.40f,          // filter_reso  (wah character)
        0.450f,         // delay_time  (wide dub echo)
        0.70f,          // delay_feedback  (long repeats)
        0.50f,          // delay_mix  (heavy echo)
        0.50f,          // reverb_mix  (deep spring wash)
        0.500f,         // release_time  (long tail into FX)
    },
};

static std::atomic<int> g_preset{0};    // current preset index (0–4)

static void apply_preset(int idx)
{
    const DubPreset& p = PRESETS[idx % NUM_PRESETS];

    g_waveform.store(p.waveform);
    g_lfo_waveform.store(p.lfo_wave);
    g_freq.store(p.freq);
    g_lfo_rate.store(p.lfo_rate);
    g_lfo_depth.store(p.lfo_depth);
    g_filter_cutoff.store(p.filter_cutoff);
    g_filter_reso.store(p.filter_reso);
    g_delay_time.store(p.delay_time);
    g_delay_feedback.store(p.delay_feedback);
    g_delay_mix.store(p.delay_mix);
    g_reverb_mix.store(p.reverb_mix);
    g_release_time.store(p.release_time);

    update_delay_eff();
}

// ─── Helpers ────────────────────────────────────────────────────────

static const char* waveform_name(int w)
{
    static const char* names[] = {"Sine", "Square", "Saw", "Triangle"};
    return names[w % static_cast<int>(Waveform::COUNT)];
}

static const char* lfo_wave_name(int w)
{
    static const char* names[] = {
        "Sine", "Triangle", "Square", "RampUp", "RampDown"
    };
    return names[w % static_cast<int>(LfoWave::COUNT)];
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

// ─── Encoder acceleration ───────────────────────────────────────────
//
// Returns a multiplier (1.0–4.0) based on how fast the encoder is
// being turned.  Slow deliberate turns → 1×, fast spins → 4×.

static float encoder_accel(int id)
{
    using clock = std::chrono::steady_clock;
    static clock::time_point prev[gpio::NUM_ENCODERS] = {};

    auto now   = clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now - prev[id]).count();
    prev[id] = now;

    if (delta > 200) return 1.0f;           // first event or long pause

    constexpr long  SLOW_MS  = 120;         // threshold for 1× (deliberate)
    constexpr long  FAST_MS  = 25;          // threshold for max (fast spin)
    constexpr float MAX_MULT = 4.0f;

    float t = static_cast<float>(SLOW_MS - delta)
            / static_cast<float>(SLOW_MS - FAST_MS);
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f + t * (MAX_MULT - 1.0f);
}

// ─── Pitch-delay link helper ─────────────────────────────────────────
//
// Recomputes the effective delay time from the base delay time and
// current frequency.  Called from the control layer whenever either
// value changes or linked mode is toggled.

static float update_delay_eff()
{
    float t = g_delay_time.load();
    if (g_delay_link.load()) {
        float freq = g_freq.load();
        t = t * (REF_FREQ / freq);
        t = std::clamp(t, 0.01f, 1.0f);
    }
    g_delay_time_eff.store(t);
    return t;
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
    float sr_f           = 48000.0f;    // set after init, read in callback
    float pitch_env_mult  = 1.0f;
    float filter_env_mult = 1.0f;
    bool  prev_gate       = false;
    bool  pe_active       = false;   // true after trigger release
    float rise_factor     = 1.0f;
    float fall_factor     = 1.0f;

    constexpr int SAMPLE_RATE = 48000;

    AudioEngine audio;

    // ── Audio callback ──────────────────────────────────────────────

    auto audio_cb = [&](float* buf, int frames) {
        const auto rlx = std::memory_order_relaxed;

        // Load all parameters once per frame
        float base_freq = g_freq.load(rlx);
        int   waveform  = g_waveform.load(rlx);
        bool  gate      = g_gate.load(rlx);
        float lfo_r     = g_lfo_rate.load(rlx);
        float lfo_d     = g_lfo_depth.load(rlx);
        float cutoff    = g_filter_cutoff.load(rlx);
        float reso      = g_filter_reso.load(rlx);
        float dly_t     = g_delay_time_eff.load(rlx);
        float dly_fb    = g_delay_feedback.load(rlx);
        float dly_mix   = g_delay_mix.load(rlx);
        float rev_mix   = g_reverb_mix.load(rlx);
        float rel_t     = g_release_time.load(rlx);
        int   pe_mode   = g_pitch_env.load(rlx);
        bool  linked    = g_delay_link.load(rlx);

        // Recompute release rate and pitch-envelope factors from release time.
        // Pitch sweeps exactly 3 octaves over the release duration so the
        // pitch slide and volume fade finish together.
        float rel_samples = rel_t * sr_f;
        release_rate = 1.0f / rel_samples;
        rise_factor  = std::pow(2.0f,  3.0f / rel_samples);  // 3 oct up
        fall_factor  = std::pow(2.0f, -3.0f / rel_samples);  // 3 oct down

        // Update DSP modules (once per frame)
        lfo.setRate(lfo_r);
        lfo.setWaveform(static_cast<LfoWave>(g_lfo_waveform.load(rlx)));
        osc.setWaveform(static_cast<Waveform>(waveform));
        filter.setResonance(reso);
        delay.setTime(dly_t);
        delay.setFeedback(dly_fb);
        delay.setMix(dly_mix);
        delay.setRepitchRate(linked ? 0.6f : 0.3f);
        reverb.setSize(REVERB_SIZE);
        reverb.setMix(rev_mix);

        // Gate edge — envelopes start on release, reset on press
        if (gate && !prev_gate) {
            pitch_env_mult  = 1.0f;     // reset on new trigger press
            filter_env_mult = 1.0f;
            pe_active = false;          // no sweep while held
            lfo.resetPhase();           // consistent attacks
        }
        if (!gate && prev_gate)
            pe_active = true;           // start sweep on release
        prev_gate = gate;

        // Per-sample processing
        for (int i = 0; i < frames; i++) {

            // Pitch + filter envelopes — sweep only after trigger release
            if (pe_active && pe_mode != 0) {
                pitch_env_mult  *= (pe_mode > 0) ? rise_factor
                                                 : fall_factor;
                pitch_env_mult   = std::clamp(pitch_env_mult, 0.125f, 8.0f);

                filter_env_mult *= (pe_mode > 0) ? rise_factor
                                                 : fall_factor;
                filter_env_mult  = std::clamp(filter_env_mult, 0.125f, 8.0f);
            }

            // LFO → exponential pitch modulation
            float lfo_out = lfo.tick();
            float lfo_mod = std::exp2(lfo_out * lfo_d);
            float freq = base_freq * pitch_env_mult * lfo_mod;
            freq = std::clamp(freq, 20.0f, 20000.0f);
            osc.setFrequency(freq);

            // Filter cutoff tracks LFO + envelope (DS71-style).
            // Filter follows 70% of the LFO pitch swing so the
            // tone brightens on rising sweeps and darkens on falls.
            float filt_lfo = std::exp2(lfo_out * lfo_d * 0.7f);
            float eff_cutoff = std::clamp(
                cutoff * filter_env_mult * filt_lfo, 20.0f, 20000.0f);
            filter.setCutoff(eff_cutoff);

            // Oscillator
            float s = osc.tick();

            // Volume envelope (linear attack / release)
            if (gate)
                env_level += attack_rate;
            else
                env_level -= release_rate;
            env_level = std::clamp(env_level, 0.0f, 1.0f);
            s *= env_level;

            // Effects chain
            s = filter.process(s);
            s = delay.process(s);
            s = reverb.process(s);

            // Output soft limiter — transparent at normal levels,
            // warm saturation when pushed (resonance, feedback, etc.)
            s = dsp::fast_tanh(s * 1.2f) * (1.0f / 1.1f);

            buf[i] = s;
        }
    };

    // ── Init audio engine ───────────────────────────────────────────

    if (audio.init(device, SAMPLE_RATE, audio_cb)) {
        float sr = static_cast<float>(audio.sampleRate());
        sr_f = sr;
        osc.setSampleRate(sr);
        lfo.setSampleRate(sr);
        filter.setSampleRate(sr);
        delay.init(sr, 1.5f);
        delay.setRepitchRate(0.3f);
        reverb.init(sr);

        attack_rate  = 1.0f / (0.005f * sr);           // ~5 ms
        release_rate = 1.0f / (0.050f * sr);            // ~50 ms

        audio.start();
    } else {
        fprintf(stderr, "Audio engine init failed\n");
    }

    // ── GPIO callbacks ──────────────────────────────────────────────

    HwCallbacks cb;

    cb.on_encoder = [](int id, int dir) {
        bool  shift = g_shift.load();
        float accel = encoder_accel(id);

        if (!shift) {
            // ── Bank A ─────────────────────────────────────────────
            switch (id) {
            case 0: {   // Freq — semitone base (or linked step)
                bool lk = g_delay_link.load();
                float base = lk ? FREQ_STEP_LINKED : FREQ_STEP;
                // Halve acceleration for cleaner pitch sweeps (1×–2.5×)
                float pitch_accel = 1.0f + (accel - 1.0f) * 0.5f;
                float step = std::pow(base, pitch_accel);
                float f = g_freq.load();
                f *= (dir > 0) ? step : (1.0f / step);
                f = std::clamp(f, 30.0f, 8000.0f);
                g_freq.store(f);
                if (lk) {
                    float eff = update_delay_eff();
                    printf("  [A] FREQ     %.1f Hz  (dly %.0f ms)\n",
                           f, eff * 1000.0f);
                } else {
                    printf("  [A] FREQ     %.1f Hz\n", f);
                }
                break;
            }
            case 1: {   // LFO Rate — 12 % base, log accel
                float step = std::pow(LFO_RATE_STEP, accel);
                float r = g_lfo_rate.load();
                r *= (dir > 0) ? step : (1.0f / step);
                r = std::clamp(r, 0.1f, 20.0f);
                g_lfo_rate.store(r);
                printf("  [A] LFO RATE %.1f Hz\n", r);
                break;
            }
            case 2: {   // Filter Cutoff — 2 semitones base, fast sweep
                float step = std::pow(CUTOFF_STEP, accel);
                float c = g_filter_cutoff.load();
                c *= (dir > 0) ? step : (1.0f / step);
                c = std::clamp(c, 20.0f, 20000.0f);
                g_filter_cutoff.store(c);
                printf("  [A] CUTOFF   %.0f Hz\n", c);
                break;
            }
            case 3: {   // Delay Time — 10 % base, log accel
                float step = std::pow(DELAY_TIME_STEP, accel);
                float t = g_delay_time.load();
                t *= (dir > 0) ? step : (1.0f / step);
                t = std::clamp(t, 0.001f, 1.0f);
                g_delay_time.store(t);
                float eff = update_delay_eff();
                if (g_delay_link.load()) {
                    printf("  [A] DLY TIME %.0f ms  (eff %.0f ms)\n",
                           t * 1000.0f, eff * 1000.0f);
                } else {
                    printf("  [A] DLY TIME %.0f ms\n", t * 1000.0f);
                }
                break;
            }
            case 4: {   // Delay Feedback — 3 % base (careful near runaway)
                float fb = g_delay_feedback.load()
                         + dir * DELAY_FB_STEP * accel;
                fb = std::clamp(fb, 0.0f, 0.95f);
                g_delay_feedback.store(fb);
                printf("  [A] DLY FB   %.0f%%\n", fb * 100.0f);
                break;
            }
            }

        } else {
            // ── Bank B ─────────────────────────────────────────────
            switch (id) {
            case 0: {   // LFO Depth — 4 % base
                float d = g_lfo_depth.load()
                        + dir * LFO_DEPTH_STEP * accel;
                d = std::clamp(d, 0.0f, 1.0f);
                g_lfo_depth.store(d);
                printf("  [B] LFO DEP  %.0f%%\n", d * 100.0f);
                break;
            }
            case 1: {   // Release Time — 15 % per click, log
                float step = std::pow(RELEASE_TIME_STEP, accel);
                float rt = g_release_time.load();
                rt *= (dir > 0) ? step : (1.0f / step);
                rt = std::clamp(rt, 0.010f, 3.5f);
                g_release_time.store(rt);
                printf("  [B] RELEASE  %.0f ms\n", rt * 1000.0f);
                break;
            }
            case 2: {   // Filter Resonance — 3 % base (fine near self-osc)
                float r = g_filter_reso.load()
                        + dir * FILTER_RESO_STEP * accel;
                r = std::clamp(r, 0.0f, 0.95f);
                g_filter_reso.store(r);
                printf("  [B] RESO     %.0f%%\n", r * 100.0f);
                break;
            }
            case 3: {   // Delay Mix — 5 % base
                float m = g_delay_mix.load()
                        + dir * DELAY_MIX_STEP * accel;
                m = std::clamp(m, 0.0f, 1.0f);
                g_delay_mix.store(m);
                printf("  [B] DLY MIX  %.0f%%\n", m * 100.0f);
                break;
            }
            case 4: {   // Reverb Mix — 5 % base
                float m = g_reverb_mix.load()
                        + dir * REVERB_MIX_STEP * accel;
                m = std::clamp(m, 0.0f, 1.0f);
                g_reverb_mix.store(m);
                printf("  [B] REV MIX  %.0f%%\n", m * 100.0f);
                break;
            }
            }
        }
    };

    cb.on_button = [](int id, bool pressed) {
        static const char *btn_name[] = {"Trigger", "Shift", "Preset"};

        switch (id) {
        case 0:
            // Trigger → gate
            g_gate.store(pressed);
            printf("  %-9s %s\n", btn_name[0],
                   pressed ? "GATE ON" : "GATE OFF");
            break;
        case 1: {
            // Shift → bank select (+ triple-click → linked delay)
            g_shift.store(pressed);

            if (pressed) {
                using clock = std::chrono::steady_clock;
                static clock::time_point clicks[3] = {};
                static int click_n = 0;

                auto now = clock::now();

                // Reset count if too long since first click
                if (click_n > 0) {
                    auto since = std::chrono::duration_cast<
                                     std::chrono::milliseconds>(
                                     now - clicks[0]).count();
                    if (since > 600) click_n = 0;
                }

                clicks[click_n++] = now;

                if (click_n >= 3) {
                    auto span = std::chrono::duration_cast<
                                    std::chrono::milliseconds>(
                                    clicks[2] - clicks[0]).count();
                    click_n = 0;

                    if (span < 500) {
                        bool lk = !g_delay_link.load();
                        g_delay_link.store(lk);
                        if (lk) {
                            float eff = update_delay_eff();
                            printf("  >>> PITCH-DELAY LINK ON"
                                   "  (dly %.0f ms)\n", eff * 1000.0f);
                        } else {
                            update_delay_eff();
                            printf("  >>> PITCH-DELAY LINK OFF\n");
                        }
                    }
                }
            }

            printf("  BANK %s\n", pressed ? "B" : "A");
            break;
        }
        case 2:
            // Preset / LFO cycle (on press only)
            // Shift held: cycle LFO waveform; otherwise: cycle dub preset
            if (pressed) {
                if (g_shift.load()) {
                    int lw = (g_lfo_waveform.load() + 1)
                           % static_cast<int>(LfoWave::COUNT);
                    g_lfo_waveform.store(lw);
                    printf("  LFO WAVE  %s\n", lfo_wave_name(lw));
                } else {
                    int idx = (g_preset.load() + 1) % NUM_PRESETS;
                    g_preset.store(idx);
                    apply_preset(idx);
                    const DubPreset& p = PRESETS[idx];
                    printf("  PRESET %d  \"%s\"\n", idx + 1, p.name);
                    printf("    %s  LFO:%s %.1fHz@%.0f%%  "
                           "Dly:%.0fms FB%.0f%% Mix%.0f%%  "
                           "Rev:%.0f%%\n",
                           waveform_name(p.waveform),
                           lfo_wave_name(p.lfo_wave),
                           p.lfo_rate, p.lfo_depth * 100.0f,
                           p.delay_time * 1000.0f,
                           p.delay_feedback * 100.0f,
                           p.delay_mix * 100.0f,
                           p.reverb_mix * 100.0f);
                }
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

    // Apply the first preset so all defaults are consistent
    apply_preset(0);

    {
        const DubPreset& p = PRESETS[g_preset.load()];
        printf("\nPreset %d: \"%s\"\n", g_preset.load() + 1, p.name);
        printf("  Freq: %.0f Hz  Wave: %s\n",
               g_freq.load(), waveform_name(g_waveform.load()));
        printf("  LFO: %.1f Hz @ %.0f%% [%s]  Filter: %.0f Hz / %.0f%%\n",
               g_lfo_rate.load(), g_lfo_depth.load() * 100.0f,
               lfo_wave_name(g_lfo_waveform.load()),
               g_filter_cutoff.load(), g_filter_reso.load() * 100.0f);
        printf("  Delay: %.0f ms  FB %.0f%%  Mix %.0f%%\n",
               g_delay_time.load() * 1000.0f,
               g_delay_feedback.load() * 100.0f,
               g_delay_mix.load() * 100.0f);
        printf("  Release: %.0f ms  Reverb: Mix %.0f%%\n",
               g_release_time.load() * 1000.0f,
               g_reverb_mix.load() * 100.0f);
    }
    printf("\nListening for events (Ctrl-C to quit) ...\n\n");

    // ── Main loop ───────────────────────────────────────────────────
    while (g_running)
        hw.poll();

    printf("\nShutting down.\n");
    audio.shutdown();
    hw.shutdown();
    return 0;
}
