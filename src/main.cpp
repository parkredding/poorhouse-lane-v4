// main.cpp — Milestone 2: CLI parsing + GPIO hardware test
//
// Usage:
//   dubsiren [--device <alsa_hw>] [--simulate]
//
// In GPIO mode it prints encoder turns, button presses, and switch
// toggles read from the physical controls via libgpiod v2.
//
// In simulate mode it maps keyboard keys to the same events so the
// app can be tested on a desktop without any GPIO hardware.

#include <cstdio>
#include <cstring>
#include <csignal>
#include <string>

#include "gpio_hw.h"

static volatile sig_atomic_t g_running = 1;

static void on_signal(int) { g_running = 0; }

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n"
        "\n"
        "  --device <hw>   ALSA device string  (default: hw:0,0)\n"
        "  --simulate      Bypass GPIO — use keyboard simulation\n"
        "  -h, --help      Show this help\n", prog);
}

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
    printf("Milestone 2: CLI & GPIO Hardware Test\n");
    printf("  ALSA device : %s\n", device.c_str());
    printf("  Mode        : %s\n", simulate ? "SIMULATE" : "GPIO");
    printf("\n");

    // ── Signal handlers ─────────────────────────────────────────────
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    // ── Event callbacks — just print to console ─────────────────────
    static const char *btn_name[] = {"Trigger", "Shift", "Waveform"};
    static const char *sw_label[] = {"FALL", "OFF", "RISE"};

    HwCallbacks cb;

    cb.on_encoder = [](int id, int dir) {
        printf("  ENC %d  %s\n", id + 1, dir > 0 ? "CW  >>" : "CCW <<");
    };

    cb.on_button = [](int id, bool pressed) {
        printf("  BTN %-9s  %s\n", btn_name[id],
               pressed ? "PRESSED" : "released");
    };

    cb.on_pitch_switch = [](int pos) {
        printf("  SW  Pitch      %s\n", sw_label[pos + 1]);
    };

    // ── Initialise GPIO / simulate ──────────────────────────────────
    GpioHw hw;
    if (!hw.init(simulate, cb)) {
        fprintf(stderr, "Failed to initialise hardware.\n");
        return 1;
    }

    printf("Listening for events (Ctrl-C to quit) ...\n\n");

    // ── Main loop ───────────────────────────────────────────────────
    while (g_running) {
        hw.poll();
    }

    printf("\nShutting down.\n");
    hw.shutdown();
    return 0;
}
