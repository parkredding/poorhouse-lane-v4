// main.cpp — Milestone 3: ALSA Audio & Core Oscillator
//
// Usage:
//   dubsiren [--device <alsa_hw>] [--simulate]
//
// Controls:
//   Trigger  (GPIO 4)    Hold to gate the volume envelope
//   Encoder 1            Base frequency (semitone steps)
//   Waveform (GPIO 5)    Cycle: Sine → Square → Saw → Triangle

#include <cstdio>
#include <cstring>
#include <csignal>
#include <atomic>

#include "gpio_hw.h"
#include "oscillator.h"
#include "audio_engine.h"

static volatile sig_atomic_t g_running = 1;

static void on_signal(int) { g_running = 0; }

// ─── Shared state (main thread → audio thread) ─────────────────────

static std::atomic<float> g_freq{440.0f};
static std::atomic<int>   g_waveform{0};
static std::atomic<bool>  g_gate{false};

// ─── Helpers ────────────────────────────────────────────────────────

static constexpr float SEMITONE    = 1.0594630943592953f;  // 2^(1/12)
static constexpr float FREQ_MIN    = 30.0f;
static constexpr float FREQ_MAX    = 4000.0f;

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
    printf("Milestone 3: ALSA Audio & Core Oscillator\n");
    printf("  ALSA device : %s\n", device.c_str());
    printf("  Mode        : %s\n\n", simulate ? "SIMULATE" : "GPIO");

    // ── Signal handlers ─────────────────────────────────────────────
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    // ── Audio engine ────────────────────────────────────────────────
    //
    // The oscillator and envelope live in the audio callback and are
    // only ever touched from the audio thread.  Shared parameters
    // (freq, waveform, gate) flow in via atomics.

    Oscillator osc;
    float env_level    = 0.0f;
    float attack_rate  = 0.0f;
    float release_rate = 0.0f;

    constexpr int SAMPLE_RATE = 48000;

    AudioEngine audio;

    auto audio_cb = [&](float* buf, int frames) {
        osc.setFrequency(g_freq.load(std::memory_order_relaxed));
        osc.setWaveform(static_cast<Waveform>(
            g_waveform.load(std::memory_order_relaxed)));
        bool gate = g_gate.load(std::memory_order_relaxed);

        for (int i = 0; i < frames; i++) {
            // Simple linear attack / release envelope
            if (gate) {
                env_level += attack_rate;
                if (env_level > 1.0f) env_level = 1.0f;
            } else {
                env_level -= release_rate;
                if (env_level < 0.0f) env_level = 0.0f;
            }
            buf[i] = osc.tick() * env_level;
        }
    };

    if (audio.init(device, SAMPLE_RATE, audio_cb)) {
        float sr = static_cast<float>(audio.sampleRate());
        osc.setSampleRate(sr);
        attack_rate  = 1.0f / (0.005f * sr);   // ~5 ms attack
        release_rate = 1.0f / (0.050f * sr);   // ~50 ms release
        audio.start();
    } else {
        fprintf(stderr, "Audio engine init failed\n");
    }

    // ── GPIO callbacks ──────────────────────────────────────────────

    static const char *btn_name[] = {"Trigger", "Shift", "Waveform"};
    static const char *sw_label[] = {"FALL", "OFF", "RISE"};

    HwCallbacks cb;

    cb.on_encoder = [](int id, int dir) {
        if (id == 0) {
            // Encoder 1 → base frequency (semitone steps)
            float f = g_freq.load();
            f *= (dir > 0) ? SEMITONE : (1.0f / SEMITONE);
            if (f < FREQ_MIN) f = FREQ_MIN;
            if (f > FREQ_MAX) f = FREQ_MAX;
            g_freq.store(f);
            printf("  FREQ  %.1f Hz\n", f);
        } else {
            printf("  ENC %d  %s\n", id + 1,
                   dir > 0 ? "CW  >>" : "CCW <<");
        }
    };

    cb.on_button = [](int id, bool pressed) {
        if (id == 0) {
            // Trigger → gate the envelope
            g_gate.store(pressed);
            printf("  %-9s  %s\n", btn_name[id],
                   pressed ? "GATE ON" : "GATE OFF");
        } else if (id == 2 && pressed) {
            // Waveform → cycle shape
            int w = (g_waveform.load() + 1)
                  % static_cast<int>(Waveform::COUNT);
            g_waveform.store(w);
            printf("  WAVE  %s\n", waveform_name(w));
        } else {
            printf("  %-9s  %s\n", btn_name[id],
                   pressed ? "PRESSED" : "released");
        }
    };

    cb.on_pitch_switch = [](int pos) {
        printf("  PITCH-ENV  %s\n", sw_label[pos + 1]);
    };

    // ── Initialise GPIO / simulate ──────────────────────────────────
    GpioHw hw;
    if (!hw.init(simulate, cb)) {
        fprintf(stderr, "Failed to initialise hardware.\n");
        audio.shutdown();
        return 1;
    }

    printf("Base freq: %.1f Hz  Waveform: %s\n", g_freq.load(),
           waveform_name(g_waveform.load()));
    printf("Listening for events (Ctrl-C to quit) ...\n\n");

    // ── Main loop ───────────────────────────────────────────────────
    while (g_running)
        hw.poll();

    printf("\nShutting down.\n");
    audio.shutdown();
    hw.shutdown();
    return 0;
}
