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
//   Enc 2:  A = LFO Rate (0.1–20 Hz)       B = Release Time (10 ms–5 s)
//   Enc 3:  A = Filter Cutoff (20–20 kHz)   B = Filter Resonance (0–95%)
//   Enc 4:  A = Delay Time (1 ms–1.0 s)     B = Delay Mix (0–100%)
//   Enc 5:  A = Delay Feedback (0–95%)      B = Reverb Mix (0–100%)
//
// Buttons:
//   Trigger  (GPIO 4)   Gate volume envelope; resets LFO phase
//   Shift    (GPIO 15)  Hold for Bank B; double-click = standard bank
//                        triple-click = experimental bank
//   Preset   (GPIO 5)   Cycle dub siren preset; Shift+Preset = cycle LFO shape
//                        Long-press (3s) = save to user bank
//
// Preset banks (4 presets each):
//   User:         Boot default — 4 saveable slots (long-press Preset to save)
//   Standard:     Double-click Shift — factory presets
//   Experimental: Triple-click Shift — extreme/crazy presets
//
// LFO shapes (Shift+GPIO 5):
//   Sine → Triangle → Square → RampUp → RampDown → S&H → ExpRise → ExpFall
//
// Pitch envelope switch (GPIO 9/10):
//   Rise / Off / Fall — sweeps pitch on trigger release
//   Filter always darkens on release (DS71-style), independent of switch
//   Preset loads set pitch env; physical switch overrides until next load
//   Triple-tap to Fall = toggle LFO-pitch link (LFO rate follows envelope)
//   Shift + triple-tap to Fall = toggle super drip reverb (heavy dub spring)

#include <cstdio>
#include <cstring>
#include <csignal>
#include <cmath>
#include <cstdlib>
#include <atomic>
#include <algorithm>
#include <chrono>

#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>
#include <cerrno>
#include <climits>

#include "gpio_hw.h"
#include "oscillator.h"
#include "audio_engine.h"
#include "lfo.h"
#include "filter.h"
#include "delay.h"
#include "reverb.h"
#include "dsp_utils.h"
#include "led_driver.h"

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
static std::atomic<float> g_sweep_dir{-1.0f};  // filter sweep: -1=Down, 0=Flat, +1=Up

// Fixed (no encoder control)
static constexpr float REVERB_SIZE = 0.65f;

// Controls
static std::atomic<int>   g_waveform{0};
static std::atomic<int>   g_lfo_waveform{0};  // LfoWave index
static std::atomic<bool>  g_gate{false};
static std::atomic<bool>  g_shift{false};
static std::atomic<int>   g_pitch_env{0};     // –1 fall, 0 off, +1 rise

// Super drip reverb (default on; secret: hold Shift + triple-tap fall to toggle)
static std::atomic<bool>  g_super_drip{true};
// LFO-pitch-envelope link (default on; secret: triple-tap pitch switch to fall to toggle)
static std::atomic<bool>  g_lfo_pitch_link{true};
static std::atomic<float> g_delay_time_eff{0.375f}; // effective delay time
static std::atomic<float> g_lfo_rate_eff{0.35f};    // effective LFO rate (freq-scaled)
static std::atomic<float> g_lfo_out{0.0f};          // LFO output [-1,+1] for LED
static std::atomic<bool>  g_led_save_blink{false};  // trigger save-confirmation blink

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

static constexpr float REF_FREQ         = 440.0f;              // neutral scaling point

static void update_link_eff();           // forward declaration
static const char* waveform_name(int w); // forward declaration
static const char* lfo_wave_name(int w); // forward declaration

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
    float       sweep_dir;      // filter sweep on release: -1=Down, 0=Flat, +1=Up
    int         pitch_env;      // pitch envelope: -1=Fall, 0=Off, +1=Rise
};

static constexpr int NUM_PRESETS = 4;

static const DubPreset PRESETS[NUM_PRESETS] = {
    //  ── 1. Slow Wail ──────────────────────────────────────────────
    //  Classic slow siren wail — also the boot sound.  Sine LFO at
    //  low rate for a long, smooth sweep.  Sawtooth oscillator for a
    //  richer harmonic spectrum.  Long 2-second release lets the sound
    //  fade out slowly with filter sweeping down.
    {
        "Slow Wail",
        2,              // Saw
        0,              // LFO: Sine
        550.0f,         // freq  (warm mid, not too high)
        0.1f,           // lfo_rate  (very slow, 10-second sweep)
        1.00f,          // lfo_depth  (full 100% sweep)
        2500.0f,        // filter_cutoff  (warm, sweep audible)
        0.35f,          // filter_reso  (gentle wah)
        0.400f,         // delay_time  (dub echo spacing)
        0.60f,          // delay_feedback  (long trails)
        0.40f,          // delay_mix  (present echo)
        0.45f,          // reverb_mix  (spring wash)
        2.000f,         // release_time  (long 2-second fade — boot signature)
        -1.0f,          // sweep_dir  (Down — canonical dub waaah on release)
        -1,             // pitch_env  (Fall — pitch drops on release)
    },
    //  ── 2. Machine Gun ────────────────────────────────────────────
    //  Rapid-fire stutter.  Square LFO chops the pitch and filter
    //  between high-bright and low-dark for aggressive on/off bursts.
    //  Tight delay stutter, longer release lets the stuttered echoes
    //  ring out with filter sweep closing them down.
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
        0.500f,         // release_time  (longer tail — stutter fades through filter)
        -1.0f,          // sweep_dir  (Down — filter closes as stutter fades out)
        0,              // pitch_env  (Off — let the stutter do the work)
    },
    //  ── 3. Lickshot ───────────────────────────────────────────────
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
        0.350f,         // release_time  (medium tail — filter sweep audible)
        -1.0f,          // sweep_dir  (Down — filter darkens into delay on release)
        -1,             // pitch_env  (Fall — laser drops on release)
    },
    //  ── 4. Droppa ───────────────────────────────────────────────────
    //  Descending siren wail.  Ramp-down LFO pulls pitch and filter
    //  down together — each cycle starts bright and falls into a
    //  fat, dark growl.  Longer release with fast filter sweep gives
    //  a dramatic darkening tail as the drop fades.
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
        0.800f,         // release_time  (longer tail — drop fades into darkness)
        -1.0f,          // sweep_dir  (Down — filter falls with the pitch, completes the drop)
        -1,             // pitch_env  (Fall — completes the descending drop)
    },
};

// ─── Experimental Presets ────────────────────────────────────────────
//
// Wild, extreme parameter combinations that push the synth to its
// limits.  Accessible via triple-tap of the Shift (bank) button.

static constexpr int NUM_EXPERIMENTAL = 4;

static const DubPreset EXPERIMENTAL[NUM_EXPERIMENTAL] = {
    //  ── 1. Insect Swarm ───────────────────────────────────────────
    //  Ultra-fast S&H LFO creates chaotic random pitch jumps.  High
    //  resonance near self-oscillation, heavy feedback delay creates
    //  cascading metallic insect-like buzzing.
    {
        "Insect Swarm",
        1,              // Square
        5,              // LFO: S&H (sample & hold — random steps)
        2000.0f,        // freq  (high, piercing)
        18.0f,          // lfo_rate  (near-max — frantic random jumps)
        0.95f,          // lfo_depth  (extreme range)
        6000.0f,        // filter_cutoff  (bright, lets harmonics through)
        0.85f,          // filter_reso  (near self-oscillation — screaming)
        0.050f,         // delay_time  (very short — comb filter territory)
        0.90f,          // delay_feedback  (near runaway — metallic buildup)
        0.70f,          // delay_mix  (heavy wet signal)
        0.60f,          // reverb_mix  (washy chaos)
        0.150f,         // release_time  (short — tight bursts)
        +1.0f,          // sweep_dir  (Up — filter opens into bright noise)
        +1,             // pitch_env  (Rise — pitch shoots up on release)
    },
    //  ── 2. Depth Charge ───────────────────────────────────────────
    //  Sub-bass sine wave with exponential fall LFO.  Extreme depth
    //  pulls pitch down into seismic territory.  Long delay with max
    //  feedback creates booming underwater explosions.
    {
        "Depth Charge",
        0,              // Sine
        7,              // LFO: ExpFall
        120.0f,         // freq  (low bass)
        0.5f,           // lfo_rate  (slow — ponderous drops)
        1.00f,          // lfo_depth  (full range — massive pitch sweep)
        800.0f,         // filter_cutoff  (very dark, subterranean)
        0.70f,          // filter_reso  (boomy resonance)
        0.800f,         // delay_time  (long — cavernous echoes)
        0.92f,          // delay_feedback  (near-infinite — rolling thunder)
        0.80f,          // delay_mix  (mostly wet — underwater)
        0.85f,          // reverb_mix  (massive space)
        3.000f,         // release_time  (very long decay)
        -1.0f,          // sweep_dir  (Down — sinks into the deep)
        -1,             // pitch_env  (Fall — descends into the abyss)
    },
    //  ── 3. Glitch Storm ───────────────────────────────────────────
    //  Triangle wave with exponential rise LFO at high rate.  Rapid
    //  upward pitch sweeps with short delay creates stuttering,
    //  glitchy digital-sounding mayhem.  Filter resonance near max.
    {
        "Glitch Storm",
        3,              // Triangle
        6,              // LFO: ExpRise
        600.0f,         // freq  (mid — room to sweep both ways)
        15.0f,          // lfo_rate  (fast — frantic)
        0.90f,          // lfo_depth  (extreme pitch range)
        12000.0f,       // filter_cutoff  (bright — all harmonics)
        0.90f,          // filter_reso  (screaming, near self-osc)
        0.120f,         // delay_time  (short — stuttering)
        0.85f,          // delay_feedback  (heavy — self-reinforcing glitches)
        0.65f,          // delay_mix  (prominent echo mayhem)
        0.20f,          // reverb_mix  (tight — keeps glitches defined)
        0.100f,         // release_time  (very short — percussive hits)
        +1.0f,          // sweep_dir  (Up — filter opens into brightness)
        0,              // pitch_env  (Off — let the LFO do the chaos)
    },
    //  ── 4. Foghorn From Hell ──────────────────────────────────────
    //  Sawtooth at very low frequency with slow ramp-up LFO.  Rich
    //  harmonics through resonant filter with maximum reverb and
    //  heavy delay feedback — a monstrous droning foghorn that fills
    //  the entire space.
    {
        "Foghorn From Hell",
        2,              // Saw
        3,              // LFO: RampUp
        55.0f,          // freq  (very low — sub A1)
        0.3f,           // lfo_rate  (very slow — ominous creep)
        0.85f,          // lfo_depth  (wide — enormous pitch range)
        1200.0f,        // filter_cutoff  (low — dark and thick)
        0.80f,          // filter_reso  (resonant growl)
        0.600f,         // delay_time  (long — massive echo)
        0.88f,          // delay_feedback  (near-infinite — droning buildup)
        0.75f,          // delay_mix  (heavy wet — wall of sound)
        0.90f,          // reverb_mix  (cathedral of doom)
        4.000f,         // release_time  (very long — hangs in the air)
        -1.0f,          // sweep_dir  (Down — darkens into oblivion)
        -1,             // pitch_env  (Fall — drops into the void)
    },
};

static std::atomic<int> g_preset{0};    // current preset index (0–3)

static void apply_dub_preset(const DubPreset& p)
{
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
    g_sweep_dir.store(p.sweep_dir);
    g_pitch_env.store(p.pitch_env);

    update_link_eff();
}

static void apply_preset(int idx)
{
    apply_dub_preset(PRESETS[idx % NUM_PRESETS]);
}

static void apply_experimental(int idx)
{
    apply_dub_preset(EXPERIMENTAL[idx % NUM_EXPERIMENTAL]);
}

// ─── User preset bank ────────────────────────────────────────────────

static constexpr int NUM_USER_PRESETS = 4;
static constexpr char PRESET_V2_HEADER[] = "DUBSIREN_PRESETS_V2";
static constexpr char PRESET_V1_HEADER[] = "DUBSIREN_PRESETS_V1";

struct UserPreset {
    bool saved;             // true = has user data, false = factory copy
    int  waveform;
    int  lfo_wave;
    float freq;
    float lfo_rate;
    float lfo_depth;
    float filter_cutoff;
    float filter_reso;
    float delay_time;
    float delay_feedback;
    float delay_mix;
    float reverb_mix;
    float release_time;
    float sweep_dir;
    int   pitch_env;        // -1=Fall, 0=Off, +1=Rise
    bool  super_drip;
};

static UserPreset g_user_presets[NUM_USER_PRESETS];

// Bank mode: USER (boot default), STANDARD (factory), EXPERIMENTAL
enum class BankMode { USER, STANDARD, EXPERIMENTAL };
static std::atomic<int> g_bank_mode{static_cast<int>(BankMode::USER)};

// Pending double-click state for bank toggle on Shift button
static std::atomic<bool> g_shift_dblclick_pending{false};
static std::chrono::steady_clock::time_point g_shift_dblclick_time{};
static int g_shift_clicks = 0;
static std::chrono::steady_clock::time_point g_shift_first_click{};

// ─── Overlay / mount-point helpers ───────────────────────────────────

// True when the root filesystem is overlayFS (kiosk / read-only mode).
// Writes to the overlay go to a RAM-backed upper layer and vanish on
// power loss, so we must detect this and route presets elsewhere.
static bool is_overlay_root()
{
    FILE* f = fopen("/proc/mounts", "r");
    if (!f) return false;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        // /proc/mounts format: device mount_point fs_type options ...
        // Parse fields explicitly to avoid false positives from
        // "overlay" appearing in mount options of a non-overlay root.
        char device[256], mount_point[256], fs_type[256];
        if (sscanf(line, "%255s %255s %255s", device, mount_point, fs_type) == 3) {
            if (strcmp(mount_point, "/") == 0 && strcmp(fs_type, "overlay") == 0) {
                fclose(f);
                return true;
            }
        }
    }
    fclose(f);
    return false;
}

// True when `path` is a mount point (device differs from its parent).
// Used to detect an active bind-mount at <install_dir>/data.
static bool is_mount_point(const char* dir_path)
{
    struct stat st, parent_st;
    if (stat(dir_path, &st) != 0) return false;

    char parent[PATH_MAX];
    snprintf(parent, sizeof(parent), "%s/..", dir_path);
    if (stat(parent, &parent_st) != 0) return false;

    return st.st_dev != parent_st.st_dev;
}

// Flag set when preset storage is known to be volatile (overlay RAM).
static bool g_volatile_presets = false;

// ─── Preset file path ────────────────────────────────────────────────

static const char* preset_file_path()
{
    static char path[PATH_MAX] = {};
    if (path[0]) return path;

    const bool overlay = is_overlay_root();
    if (overlay)
        printf("  Overlay FS detected — looking for persistent storage\n");

    // Resolve install dir from executable path:
    //   /home/<user>/dubsiren/build/dubsiren  →  /home/<user>/dubsiren
    // base buffer leaves room for appending suffixes when no persist mount.
    // persist buffer leaves room for "/mnt/persist" + base, and the result
    // must still fit suffixes up to 30 chars in path[PATH_MAX].
    static constexpr size_t BASE_ROOM = 48;           // room for suffixes
    static constexpr size_t PERSIST_ROOM = 31;        // 30-char suffix + NUL
    char base[PATH_MAX - BASE_ROOM];
    ssize_t len = readlink("/proc/self/exe", base, sizeof(base) - 1);
    if (len > 0 && len < (ssize_t)sizeof(base) - 1) {
        base[len] = '\0';
        // Strip "/build/dubsiren" (go up two path components)
        char* slash = strrchr(base, '/');         // strip binary name
        if (slash) *slash = '\0';
        slash = strrchr(base, '/');               // strip "build"
        if (slash) *slash = '\0';

        // When overlayFS is active, the bind-mount at <base>/data may not
        // be available (race, mount failure, running outside systemd).
        // Writes to the overlay look fine but vanish on power loss.
        // Bypass the overlay entirely by writing straight to the persist
        // mount if it exists — this is the real read-write disk.
        bool use_persist = false;
        char persist[PATH_MAX - PERSIST_ROOM];
        if (strlen(base) + sizeof("/mnt/persist") < sizeof(persist)) {
            snprintf(persist, sizeof(persist), "/mnt/persist%s", base);

            // Ensure the directory tree exists on the persist mount
            // BEFORE checking stat().  Without this, a mounted /mnt/persist
            // that lacks the install-dir subtree would cause stat() to fail
            // and presets would silently fall through to volatile overlay RAM.
            if (overlay) {
                char pd[PATH_MAX];
                snprintf(pd, sizeof(pd), "%s/data", persist);
                if (mkdir(pd, 0755) != 0 && errno != EEXIST)
                    fprintf(stderr, "  !!! Failed to create directory %s: %s\n", pd, strerror(errno));
                snprintf(pd, sizeof(pd), "%s/data/presets", persist);
                if (mkdir(pd, 0755) != 0 && errno != EEXIST)
                    fprintf(stderr, "  !!! Failed to create directory %s: %s\n", pd, strerror(errno));
            }

            struct stat st;
            use_persist = (stat(persist, &st) == 0 && S_ISDIR(st.st_mode));

            // On overlay FS the persist mount is critical.  If it isn't
            // visible yet (boot race / slow mount) retry a few times.
            if (!use_persist && overlay) {
                for (int attempt = 1; attempt <= 3 && !use_persist; attempt++) {
                    fprintf(stderr,
                        "  !!! Persist mount not ready — retry %d/3 ...\n",
                        attempt);
                    sleep(1);
                    use_persist = (stat(persist, &st) == 0
                                   && S_ISDIR(st.st_mode));
                }
                if (!use_persist) {
                    fprintf(stderr,
                        "  !!! /mnt/persist not available — ensure-persist.sh may not have run.\n"
                        "  !!! To set up manually:  sudo /usr/local/lib/dubsiren/ensure-persist.sh %s\n",
                        base);
                }
            }
        }

        // Determine the storage root.
        // Priority: 1) bind-mount at base/data  2) /mnt/persist  3) base
        // The bind-mount is preferred because ensure-persist.sh sets it up
        // with correct ownership.  The persist mount is a fallback that
        // writes directly to the real disk.
        const char* root = nullptr;
        const char* storage_label = nullptr;

        if (overlay) {
            // Check the systemd bind-mount at <base>/data first — this
            // is what ensure-persist.sh sets up with correct permissions.
            char data_dir[PATH_MAX];
            snprintf(data_dir, sizeof(data_dir), "%s/data", base);
            if (is_mount_point(data_dir)) {
                root = base;
                storage_label = "bind-mount — durable";
            } else if (use_persist) {
                // Bind mount unavailable, but persist mount exists.
                // Fall back to writing directly to the persist mount.
                root = persist;
                storage_label = "persist mount — durable";
            } else {
                // No persistent storage available.  Writes will go to
                // the overlay RAM layer and be LOST on power cycle.
                root = base;
                storage_label = "VOLATILE (overlay RAM) — WILL NOT PERSIST";
                g_volatile_presets = true;
                fprintf(stderr,
                    "\n"
                    "  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                    "  !!!  WARNING: No persistent storage available  !!!\n"
                    "  !!!  User presets will be LOST on power cycle  !!!\n"
                    "  !!!  Check mnt-persist.mount / data bind mount !!!\n"
                    "  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                    "\n");
            }
        } else {
            // Not on overlay FS — regular read-write filesystem
            root = base;
            storage_label = "local filesystem — durable";
        }

        // Create required directories
        snprintf(path, sizeof(path), "%s/data", root);
        if (mkdir(path, 0755) != 0 && errno != EEXIST)
            fprintf(stderr, "  !!! Failed to create directory: %s\n", path);
        snprintf(path, sizeof(path), "%s/data/presets", root);
        if (mkdir(path, 0755) != 0 && errno != EEXIST)
            fprintf(stderr, "  !!! Failed to create directory: %s\n", path);

        // Write-test: verify the selected path is actually writable.
        // If not (permission issues), fall through to the next option.
        // Uses mkstemp to avoid predictable filenames.
        auto write_test = [](const char* dir) -> bool {
            char tmpl[PATH_MAX];
            snprintf(tmpl, sizeof(tmpl), "%s/.write-test-XXXXXX", dir);
            int fd = mkstemp(tmpl);
            if (fd >= 0) {
                close(fd);
                unlink(tmpl);
                return true;
            }
            return false;
        };

        if (overlay && !g_volatile_presets) {
            char presets_dir[PATH_MAX];
            snprintf(presets_dir, sizeof(presets_dir),
                     "%s/data/presets", root);
            if (!write_test(presets_dir)) {
                fprintf(stderr,
                    "  !!! Write test FAILED for %s: %s\n"
                    "  !!! Trying next storage option...\n",
                    root, strerror(errno));

                // Try the other durable option
                if (root == base && use_persist) {
                    root = persist;
                    storage_label = "persist mount — durable (fallback)";
                } else if (root == persist) {
                    char data_dir2[PATH_MAX];
                    snprintf(data_dir2, sizeof(data_dir2), "%s/data", base);
                    if (is_mount_point(data_dir2)) {
                        root = base;
                        storage_label = "bind-mount — durable (fallback)";
                    }
                }

                // Ensure dirs exist for the fallback root (idempotent)
                snprintf(path, sizeof(path), "%s/data", root);
                mkdir(path, 0755);
                snprintf(path, sizeof(path), "%s/data/presets", root);
                mkdir(path, 0755);

                snprintf(presets_dir, sizeof(presets_dir),
                         "%s/data/presets", root);
                if (!write_test(presets_dir)) {
                    fprintf(stderr,
                        "  !!! Write test FAILED for fallback too: %s\n",
                        strerror(errno));
                    root = base;
                    storage_label =
                        "VOLATILE (overlay RAM) — WILL NOT PERSIST";
                    g_volatile_presets = true;
                    fprintf(stderr,
                        "\n"
                        "  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                        "  !!!  WARNING: No writable storage available    !!!\n"
                        "  !!!  User presets will be LOST on power cycle  !!!\n"
                        "  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                        "\n");
                }
            }
        }

        snprintf(path, sizeof(path), "%s/data/presets/user_presets.txt", root);
        printf("  Preset file: %s\n", path);
        printf("  Storage:     %s\n", storage_label);
        return path;
    }

    // Fallback: use HOME-based path (non-overlay systems / development)
    const char* home = getenv("HOME");
    if (!home || home[0] == '\0') {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (!home || home[0] == '\0') home = "/tmp";
    snprintf(path, sizeof(path), "%s/.config", home);
    if (mkdir(path, 0755) != 0 && errno != EEXIST)
        fprintf(stderr, "  !!! Failed to create directory: %s\n", path);
    snprintf(path, sizeof(path), "%s/.config/dubsiren", home);
    if (mkdir(path, 0755) != 0 && errno != EEXIST)
        fprintf(stderr, "  !!! Failed to create directory: %s\n", path);
    snprintf(path, sizeof(path), "%s/.config/dubsiren/user_presets.txt", home);

    if (overlay) {
        g_volatile_presets = true;
        fprintf(stderr,
            "  !!! HOME fallback on overlay FS — presets WILL NOT PERSIST\n");
    }
    printf("  Preset file: %s (HOME fallback%s)\n", path,
           overlay ? " — VOLATILE" : "");
    return path;
}

// ─── Snapshot current state into a UserPreset ────────────────────────

static UserPreset snapshot_current()
{
    UserPreset u;
    u.saved         = true;
    u.waveform      = g_waveform.load();
    u.lfo_wave      = g_lfo_waveform.load();
    u.freq          = g_freq.load();
    u.lfo_rate      = g_lfo_rate.load();
    u.lfo_depth     = g_lfo_depth.load();
    u.filter_cutoff = g_filter_cutoff.load();
    u.filter_reso   = g_filter_reso.load();
    u.delay_time    = g_delay_time.load();
    u.delay_feedback= g_delay_feedback.load();
    u.delay_mix     = g_delay_mix.load();
    u.reverb_mix    = g_reverb_mix.load();
    u.release_time  = g_release_time.load();
    u.sweep_dir     = g_sweep_dir.load();
    u.pitch_env     = g_pitch_env.load();
    u.super_drip    = g_super_drip.load();
    return u;
}

// Verify that the preset file was written correctly (read-back check)
static bool verify_preset_file(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "  !!! Verify FAILED: cannot reopen %s: %s\n",
                path, strerror(errno));
        return false;
    }
    char header[64];
    if (!fgets(header, sizeof(header), f) ||
        strncmp(header, PRESET_V2_HEADER, sizeof(PRESET_V2_HEADER) - 1) != 0) {
        fprintf(stderr, "  !!! Verify FAILED: bad header in %s\n", path);
        fclose(f);
        return false;
    }
    int lines = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) lines++;
    fclose(f);
    if (lines != NUM_USER_PRESETS) {
        fprintf(stderr, "  !!! Verify FAILED: expected %d presets, got %d in %s\n",
                NUM_USER_PRESETS, lines, path);
        return false;
    }
    return true;
}

// ─── Save / load user presets to disk ────────────────────────────────

static void save_user_presets()
{
    const char* path = preset_file_path();

    // Write to temp file, then rename for power-safety
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE* f = fopen(tmp, "w");
    if (!f) {
        fprintf(stderr, "  !!! Failed to save presets: %s\n", tmp);
        return;
    }

    fprintf(f, "%s\n", PRESET_V2_HEADER);
    for (int i = 0; i < NUM_USER_PRESETS; i++) {
        const UserPreset& u = g_user_presets[i];
        fprintf(f, "%d %d %d %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %d %d\n",
                u.saved ? 1 : 0,
                u.waveform, u.lfo_wave,
                u.freq, u.lfo_rate, u.lfo_depth,
                u.filter_cutoff, u.filter_reso,
                u.delay_time, u.delay_feedback, u.delay_mix,
                u.reverb_mix, u.release_time, u.sweep_dir,
                u.pitch_env,
                u.super_drip ? 1 : 0);
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);
    if (rename(tmp, path) != 0) {
        fprintf(stderr, "  !!! Failed to rename preset file: %s → %s (%s)\n",
                tmp, path, strerror(errno));
        return;
    }

    // Sync the directory so the rename is durable across power loss
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        int dfd = open(dir, O_RDONLY);
        if (dfd >= 0) { fsync(dfd); close(dfd); }
    }

    printf("  Presets written to %s\n", path);

    if (!verify_preset_file(path))
        fprintf(stderr, "  !!! WARNING: Preset file verification failed!\n");

    if (g_volatile_presets) {
        fprintf(stderr,
            "  !!! WARNING: Preset saved to volatile storage — "
            "will be lost on power cycle!\n");
    }
}

// Validate that a loaded preset's enum fields are in range
static bool validate_preset(const UserPreset& u, int pitch_env, int index)
{
    if (u.waveform < 0 || u.waveform >= static_cast<int>(Waveform::COUNT) ||
        u.lfo_wave < 0 || u.lfo_wave >= static_cast<int>(LfoWave::COUNT) ||
        u.sweep_dir < -1 || u.sweep_dir > 1 ||
        pitch_env < -1 || pitch_env > 1) {
        fprintf(stderr, "  !!! Preset %d has invalid values — skipping\n", index);
        return false;
    }
    return true;
}

static void load_user_presets()
{
    const char* path = preset_file_path();
    FILE* f = fopen(path, "r");
    if (!f) {
        printf("  No saved presets at %s — using factory defaults\n", path);
        return;
    }

    char header[64];
    if (!fgets(header, sizeof(header), f)) {
        fprintf(stderr, "  !!! Empty preset file — ignoring\n");
        fclose(f);
        return;
    }

    bool v2 = (strncmp(header, PRESET_V2_HEADER, sizeof(PRESET_V2_HEADER) - 1) == 0);
    bool v1 = (strncmp(header, PRESET_V1_HEADER, sizeof(PRESET_V1_HEADER) - 1) == 0);
    if (!v1 && !v2) {
        fprintf(stderr, "  !!! Invalid preset file — ignoring\n");
        fclose(f);
        return;
    }

    // V1 had 8 slots, V2 has 4 — only load what fits
    int slots_in_file = v1 ? 8 : NUM_USER_PRESETS;

    for (int i = 0; i < slots_in_file; i++) {
        // V1 files have 8 presets but we only have 4 slots now
        bool skip = (i >= NUM_USER_PRESETS);
        UserPreset tmp;
        UserPreset& u = skip ? tmp : g_user_presets[i];

        if (v2) {
            int saved, pe, sd;
            int n = fscanf(f, "%d %d %d %f %f %f %f %f %f %f %f %f %f %f %d %d",
                           &saved,
                           &u.waveform, &u.lfo_wave,
                           &u.freq, &u.lfo_rate, &u.lfo_depth,
                           &u.filter_cutoff, &u.filter_reso,
                           &u.delay_time, &u.delay_feedback, &u.delay_mix,
                           &u.reverb_mix, &u.release_time, &u.sweep_dir,
                           &pe, &sd);
            if (n == 16) {
                if (!validate_preset(u, pe, i)) { u.saved = false; continue; }
                u.saved      = (saved != 0);
                u.pitch_env  = pe;
                u.super_drip = (sd != 0);
            } else {
                break;
            }
        } else {
            // V1 format: 17 fields (includes delay_link, lfo_pitch_link)
            int saved, dl, lpl, sd;
            int n = fscanf(f, "%d %d %d %f %f %f %f %f %f %f %f %f %f %f %d %d %d",
                           &saved,
                           &u.waveform, &u.lfo_wave,
                           &u.freq, &u.lfo_rate, &u.lfo_depth,
                           &u.filter_cutoff, &u.filter_reso,
                           &u.delay_time, &u.delay_feedback, &u.delay_mix,
                           &u.reverb_mix, &u.release_time, &u.sweep_dir,
                           &dl, &lpl, &sd);
            if (n == 17) {
                if (!validate_preset(u, 0, i)) { u.saved = false; continue; }
                u.saved      = (saved != 0);
                u.pitch_env  = 0;  // V1 didn't store pitch_env, default off
                u.super_drip = (sd != 0);
            } else {
                break;
            }
        }
    }

    fclose(f);
    int loaded_count = 0;
    for (int j = 0; j < NUM_USER_PRESETS; j++)
        if (g_user_presets[j].saved) loaded_count++;
    printf("  User presets loaded from %s (%s, %d/%d slots populated)\n",
           path, v2 ? "V2" : "V1→V2 migration", loaded_count, NUM_USER_PRESETS);
}

static void init_user_presets()
{
    // Start with factory defaults in all user slots
    for (int i = 0; i < NUM_USER_PRESETS; i++) {
        const DubPreset& f = PRESETS[i % NUM_PRESETS];
        UserPreset& u = g_user_presets[i];
        u.saved         = false;
        u.waveform      = f.waveform;
        u.lfo_wave      = f.lfo_wave;
        u.freq          = f.freq;
        u.lfo_rate      = f.lfo_rate;
        u.lfo_depth     = f.lfo_depth;
        u.filter_cutoff = f.filter_cutoff;
        u.filter_reso   = f.filter_reso;
        u.delay_time    = f.delay_time;
        u.delay_feedback= f.delay_feedback;
        u.delay_mix     = f.delay_mix;
        u.reverb_mix    = f.reverb_mix;
        u.release_time  = f.release_time;
        u.sweep_dir     = f.sweep_dir;
        u.pitch_env     = f.pitch_env;
        u.super_drip    = true;
    }

    // Migrate from old ~/.config/dubsiren/ location if new path has no file
    const char* new_path = preset_file_path();
    if (access(new_path, F_OK) != 0) {
        const char* home = getenv("HOME");
        if (!home || home[0] == '\0') {
            struct passwd* pw = getpwuid(getuid());
            if (pw) home = pw->pw_dir;
        }
        if (home && home[0] != '\0') {
            char old_path[PATH_MAX];
            snprintf(old_path, sizeof(old_path),
                     "%s/.config/dubsiren/user_presets.txt", home);
            if (access(old_path, R_OK) == 0) {
                // Copy old file to new persistent location
                FILE* src = fopen(old_path, "r");
                FILE* dst = fopen(new_path, "w");
                if (src && dst) {
                    char buf[4096];
                    size_t n;
                    while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
                        fwrite(buf, 1, n, dst);
                    fflush(dst);
                    fsync(fileno(dst));
                    printf("  Migrated presets from %s\n", old_path);
                }
                if (src) fclose(src);
                if (dst) fclose(dst);
            }
        }
    }

    // Override with saved data from disk
    load_user_presets();
}

// ─── Apply user preset ──────────────────────────────────────────────

static void apply_user_preset(const UserPreset& u)
{
    g_waveform.store(u.waveform);
    g_lfo_waveform.store(u.lfo_wave);
    g_freq.store(u.freq);
    g_lfo_rate.store(u.lfo_rate);
    g_lfo_depth.store(u.lfo_depth);
    g_filter_cutoff.store(u.filter_cutoff);
    g_filter_reso.store(u.filter_reso);
    g_delay_time.store(u.delay_time);
    g_delay_feedback.store(u.delay_feedback);
    g_delay_mix.store(u.delay_mix);
    g_reverb_mix.store(u.reverb_mix);
    g_release_time.store(u.release_time);
    g_sweep_dir.store(u.sweep_dir);
    g_pitch_env.store(u.pitch_env);
    g_super_drip.store(u.super_drip);

    update_link_eff();
}

// ─── Bank name helper ─────────────────────────────────────────────────

static const char* bank_name(BankMode m)
{
    switch (m) {
    case BankMode::USER:         return "USER";
    case BankMode::STANDARD:     return "STANDARD";
    case BankMode::EXPERIMENTAL: return "EXPERIMENTAL";
    }
    return "?";
}

// ─── Switch to a specific bank ────────────────────────────────────────

static void switch_bank(BankMode mode)
{
    g_bank_mode.store(static_cast<int>(mode));
    int idx = g_preset.load();
    switch (mode) {
    case BankMode::USER:
        // Clamp index to user preset count
        if (idx >= NUM_USER_PRESETS) { idx = 0; g_preset.store(idx); }
        apply_user_preset(g_user_presets[idx]);
        printf("  BANK: USER  slot %d%s\n",
               idx + 1,
               g_user_presets[idx].saved ? "" : "  (factory copy)");
        break;
    case BankMode::STANDARD:
        if (idx >= NUM_PRESETS) { idx = 0; g_preset.store(idx); }
        apply_preset(idx);
        printf("  BANK: STANDARD  \"%s\"\n", PRESETS[idx].name);
        break;
    case BankMode::EXPERIMENTAL:
        if (idx >= NUM_EXPERIMENTAL) { idx = 0; g_preset.store(idx); }
        apply_experimental(idx);
        printf("  BANK: EXPERIMENTAL  \"%s\"\n", EXPERIMENTAL[idx].name);
        break;
    }
}

// ─── Toggle bank: switch to target, or back to USER if already there ─

static void toggle_bank(BankMode target)
{
    auto current = static_cast<BankMode>(g_bank_mode.load());
    switch_bank(current == target ? BankMode::USER : target);
}

// ─── Cycle to next preset in current bank ────────────────────────────

// ─── Save current state to user bank slot ─────────────────────────────

static void save_current_to_user_bank()
{
    int idx = g_preset.load();
    if (idx >= NUM_USER_PRESETS) idx = 0;
    g_user_presets[idx] = snapshot_current();
    save_user_presets();

    // Switch to user bank if not already
    auto bm = static_cast<BankMode>(g_bank_mode.load());
    if (bm != BankMode::USER) {
        g_preset.store(idx);
        g_bank_mode.store(static_cast<int>(BankMode::USER));
    }

    g_led_save_blink.store(true);

    printf("  >>> SAVED to USER %d  (Freq:%.0fHz %s LFO:%s)\n",
           idx + 1,
           g_freq.load(),
           waveform_name(g_waveform.load()),
           lfo_wave_name(g_lfo_waveform.load()));
}

// ─── Cycle to next preset in current bank ────────────────────────────

static void cycle_preset()
{
    auto mode = static_cast<BankMode>(g_bank_mode.load());
    int count = (mode == BankMode::USER)         ? NUM_USER_PRESETS
              : (mode == BankMode::STANDARD)     ? NUM_PRESETS
              :                                    NUM_EXPERIMENTAL;
    int idx = (g_preset.load() + 1) % count;
    g_preset.store(idx);

    switch (mode) {
    case BankMode::USER:
        apply_user_preset(g_user_presets[idx]);
        printf("  USER %d%s  %s LFO:%s %.1fHz@%.0f%%  "
               "Dly:%.0fms FB%.0f%% Mix%.0f%%  Rev:%.0f%%\n",
               idx + 1,
               g_user_presets[idx].saved ? "" : " (factory copy)",
               waveform_name(g_user_presets[idx].waveform),
               lfo_wave_name(g_user_presets[idx].lfo_wave),
               g_user_presets[idx].lfo_rate,
               g_user_presets[idx].lfo_depth * 100.0f,
               g_user_presets[idx].delay_time * 1000.0f,
               g_user_presets[idx].delay_feedback * 100.0f,
               g_user_presets[idx].delay_mix * 100.0f,
               g_user_presets[idx].reverb_mix * 100.0f);
        break;
    case BankMode::STANDARD: {
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
        break;
    }
    case BankMode::EXPERIMENTAL: {
        apply_experimental(idx);
        const DubPreset& p = EXPERIMENTAL[idx];
        printf("  EXP %d  \"%s\"\n", idx + 1, p.name);
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
        break;
    }
    }
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
        "Sine", "Triangle", "Square", "RampUp", "RampDown",
        "S&H", "ExpRise", "ExpFall"
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

// ─── Multi-click detector ────────────────────────────────────────────
//
// Reusable helper for triple-click (or N-click) detection on buttons
// and switches.  Tracks timestamps and fires when N clicks land within
// the given time window.

struct MultiClickDetector {
    int  required;        // number of clicks to trigger
    long window_ms;       // max ms from first to last click
    long reset_ms;        // reset if idle longer than this after first click

    std::chrono::steady_clock::time_point stamps[4] = {};
    int count = 0;

    // Call on each click.  Returns true when the pattern fires.
    bool click()
    {
        using clock = std::chrono::steady_clock;
        auto now = clock::now();

        if (count > 0) {
            auto since = std::chrono::duration_cast<
                             std::chrono::milliseconds>(
                             now - stamps[0]).count();
            if (since > reset_ms) count = 0;
        }

        stamps[count++] = now;

        if (count >= required) {
            auto span = std::chrono::duration_cast<
                            std::chrono::milliseconds>(
                            stamps[count - 1] - stamps[0]).count();
            count = 0;
            return span < window_ms;
        }
        return false;
    }
};

// ─── LFO-pitch scaling helper ────────────────────────────────────────
//
// Recomputes the effective delay time and LFO rate from their base
// values and the current frequency.  Called from the control layer
// whenever freq, delay time, or LFO rate changes.
//
// LFO rate always tracks pitch (faster modulation at higher pitches).

static void update_link_eff()
{
    float t = g_delay_time.load();
    float r = g_lfo_rate.load();
    float freq = g_freq.load();
    float ratio = freq / REF_FREQ;

    // LFO rate always tracks pitch
    r = r * ratio;
    r = std::clamp(r, 0.1f, 20.0f);

    g_delay_time_eff.store(t);
    g_lfo_rate_eff.store(r);
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
    float sweep_phase     = 0.0f;   // 0→1 linear ramp during release
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
        float lfo_r     = g_lfo_rate_eff.load(rlx);
        float lfo_d     = g_lfo_depth.load(rlx);
        float cutoff    = g_filter_cutoff.load(rlx);
        float reso      = g_filter_reso.load(rlx);
        float dly_t     = g_delay_time_eff.load(rlx);
        float dly_fb    = g_delay_feedback.load(rlx);
        float dly_mix   = g_delay_mix.load(rlx);
        float rev_mix   = g_reverb_mix.load(rlx);
        float rel_t     = g_release_time.load(rlx);
        int   pe_mode   = g_pitch_env.load(rlx);
        bool  lfo_link  = g_lfo_pitch_link.load(rlx);
        float sweep_dir = g_sweep_dir.load(rlx);

        // Recompute release rate and pitch-envelope factors from release time.
        // Pitch sweeps exactly 3 octaves over the release duration so the
        // pitch slide and volume fade finish together.
        float rel_samples = rel_t * sr_f;
        release_rate = 1.0f / rel_samples;
        rise_factor  = std::pow(2.0f,  3.0f / rel_samples);  // 3 oct up
        fall_factor  = std::pow(2.0f, -3.0f / rel_samples);  // 3 oct down
        float sweep_inc  = 1.0f / rel_samples;               // linear phase increment

        // Update DSP modules (once per frame)
        // lfo rate set per-sample below (optionally tracks pitch envelope)
        lfo.setWaveform(static_cast<LfoWave>(g_lfo_waveform.load(rlx)));
        osc.setWaveform(static_cast<Waveform>(waveform));
        filter.setResonance(reso);
        delay.setTime(dly_t);
        delay.setFeedback(dly_fb);
        delay.setMix(dly_mix);
        delay.setRepitchRate(0.3f);
        reverb.setSize(REVERB_SIZE);
        reverb.setMix(rev_mix);
        reverb.setSuperDrip(g_super_drip.load(rlx));

        // Gate edge — envelopes start on release, reset on press
        if (gate && !prev_gate) {
            pitch_env_mult = 1.0f;      // reset on new trigger press
            sweep_phase    = 0.0f;      // filter sweep restarts from cutoff
            pe_active      = false;     // no sweep while held
            lfo.resetPhase();           // consistent attacks
        }
        if (!gate && prev_gate)
            pe_active = true;           // start sweep on release
        prev_gate = gate;

        // Per-sample processing
        for (int i = 0; i < frames; i++) {

            // Pitch envelope — sweep only after trigger release, only
            // when the pitch switch is set to Rise or Fall.
            if (pe_active && pe_mode != 0) {
                pitch_env_mult *= (pe_mode > 0) ? rise_factor
                                                : fall_factor;
                pitch_env_mult  = std::clamp(pitch_env_mult, 0.125f, 8.0f);
            }

            // Sweep phase: 0 while held (static cutoff), ramps 0→1 on release.
            if (pe_active)
                sweep_phase = std::min(sweep_phase + sweep_inc, 1.0f);

            // LFO rate optionally tracks pitch envelope (secret mode)
            lfo.setRate(lfo_link ? lfo_r * pitch_env_mult : lfo_r);

            // LFO → exponential pitch modulation (pitch only, not filter)
            float lfo_out = lfo.tick();
            float lfo_mod = std::exp2(lfo_out * lfo_d);
            float freq = base_freq * pitch_env_mult * lfo_mod;
            freq = std::clamp(freq, 20.0f, 20000.0f);
            osc.setFrequency(freq);

            // Filter: static at cutoff while held; sweeps directionally on release.
            // sweep_dir * 3 octaves at sweep_phase=1: +1→8x brighter, -1→1/8 darker.
            float eff_cutoff = std::clamp(
                cutoff * std::exp2(sweep_dir * 3.0f * sweep_phase),
                20.0f, 20000.0f);
            filter.setCutoff(eff_cutoff);

            // Oscillator
            float s = osc.tick();

            // Filter before volume envelope so the sweep is
            // audible through the full release tail
            s = filter.process(s);

            // Volume envelope (linear attack / release)
            if (gate)
                env_level += attack_rate;
            else
                env_level -= release_rate;
            env_level = std::clamp(env_level, 0.0f, 1.0f);
            s *= env_level;

            // Effects chain
            s = delay.process(s);
            s = reverb.process(s);

            // Output soft limiter — transparent at normal levels,
            // warm saturation when pushed (resonance, feedback, etc.)
            s = dsp::fast_tanh(s * 1.2f) * (1.0f / 1.1f);

            // Export LFO value for LED pulsing (read by main thread)
            g_lfo_out.store(lfo_out, std::memory_order_relaxed);

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
        reverb.setSuperDrip(true);

        attack_rate  = 1.0f / (0.005f * sr);           // ~5 ms
        release_rate = 1.0f / (0.050f * sr);            // ~50 ms

        audio.start();
    } else {
        fprintf(stderr, "Audio engine init failed — exiting so systemd can restart\n");
        return 1;
    }

    // ── GPIO callbacks ──────────────────────────────────────────────

    HwCallbacks cb;

    cb.on_encoder = [](int id, int dir) {
        bool  shift = g_shift.load();
        float accel = encoder_accel(id);

        if (!shift) {
            // ── Bank A ─────────────────────────────────────────────
            switch (id) {
            case 0: {   // Freq — semitone base
                // Halve acceleration for cleaner pitch sweeps (1×–2.5×)
                float pitch_accel = 1.0f + (accel - 1.0f) * 0.5f;
                float step = std::pow(FREQ_STEP, pitch_accel);
                float f = g_freq.load();
                f *= (dir > 0) ? step : (1.0f / step);
                f = std::clamp(f, 30.0f, 8000.0f);
                g_freq.store(f);
                update_link_eff();
                printf("  [A] FREQ     %.1f Hz  (lfo %.1f Hz)\n",
                       f, g_lfo_rate_eff.load());
                break;
            }
            case 1: {   // LFO Rate — 12 % base, log accel
                float step = std::pow(LFO_RATE_STEP, accel);
                float r = g_lfo_rate.load();
                r *= (dir > 0) ? step : (1.0f / step);
                r = std::clamp(r, 0.1f, 20.0f);
                g_lfo_rate.store(r);
                update_link_eff();
                printf("  [A] LFO RATE %.1f Hz  (eff %.1f Hz)\n",
                       r, g_lfo_rate_eff.load());
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
                update_link_eff();
                printf("  [A] DLY TIME %.0f ms\n", t * 1000.0f);
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
                rt = std::clamp(rt, 0.010f, 5.0f);
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
            // Shift → bank select
            //   Double-click: toggle standard ↔ user preset bank
            //   Triple-click: toggle experimental ↔ user preset bank
            g_shift.store(pressed);

            if (pressed) {
                auto now = std::chrono::steady_clock::now();

                // Reset if too long since first click
                if (g_shift_clicks > 0) {
                    auto since = std::chrono::duration_cast<
                        std::chrono::milliseconds>(
                            now - g_shift_first_click).count();
                    if (since > 600) g_shift_clicks = 0;
                }

                if (g_shift_clicks == 0) g_shift_first_click = now;
                g_shift_clicks++;

                if (g_shift_clicks >= 3) {
                    // Triple-click: switch to experimental bank
                    g_shift_dblclick_pending.store(false);
                    auto span = std::chrono::duration_cast<
                        std::chrono::milliseconds>(
                            now - g_shift_first_click).count();
                    g_shift_clicks = 0;
                    if (span < 500) {
                        toggle_bank(BankMode::EXPERIMENTAL);
                    }
                } else if (g_shift_clicks == 2) {
                    // Pending double-click (may become triple)
                    g_shift_dblclick_pending.store(true);
                    g_shift_dblclick_time = now;
                }
            }

            printf("  BANK %s\n", pressed ? "B" : "A");
            break;
        }
        case 2: {
            // Preset button: cycle presets, save (long-press)
            //   Shift+press:  cycle LFO waveform (immediate)
            //   Short press:  cycle preset (immediate)
            //   Long-press (3s): save current state to user bank slot

            static auto btn2_press_time = std::chrono::steady_clock::time_point{};
            static bool btn2_shift_at_press = false;

            if (pressed) {
                btn2_shift_at_press = g_shift.load();
                if (btn2_shift_at_press) {
                    // Shift+Press: cycle LFO waveform (fires immediately)
                    int lw = (g_lfo_waveform.load() + 1)
                           % static_cast<int>(LfoWave::COUNT);
                    g_lfo_waveform.store(lw);
                    printf("  LFO WAVE  %s\n", lfo_wave_name(lw));
                } else {
                    btn2_press_time = std::chrono::steady_clock::now();
                }
            } else {
                // Released
                if (!btn2_shift_at_press) {
                    auto now = std::chrono::steady_clock::now();
                    auto hold_ms = std::chrono::duration_cast<
                        std::chrono::milliseconds>(
                            now - btn2_press_time).count();

                    if (hold_ms >= 3000) {
                        // ── Long press: save to user bank ──
                        save_current_to_user_bank();
                    } else {
                        // ── Short press: cycle preset immediately ──
                        cycle_preset();
                    }
                }
            }
            break;
        }
        }
    };

    cb.on_pitch_switch = [](int pos) {
        g_pitch_env.store(pos);
        static const char *lbl[] = {"FALL", "OFF", "RISE"};
        printf("  PITCH-ENV  %s\n", lbl[pos + 1]);

        // Secret modes on triple-tap to FALL:
        //   Shift held  → super drip reverb (heavy dub spring)
        //   Shift free  → LFO-pitch-envelope link
        if (pos == -1) {
            if (g_shift.load()) {
                static MultiClickDetector det{3, 700, 800};
                if (det.click()) {
                    bool sd = !g_super_drip.load();
                    g_super_drip.store(sd);
                    printf("  >>> SUPER DRIP REVERB %s\n",
                           sd ? "ON" : "OFF");
                }
            } else {
                static MultiClickDetector det{3, 700, 800};
                if (det.click()) {
                    bool lk = !g_lfo_pitch_link.load();
                    g_lfo_pitch_link.store(lk);
                    printf("  >>> LFO-PITCH LINK %s\n",
                           lk ? "ON" : "OFF");
                }
            }
        }
    };

    // ── Keyboard-only shortcuts (simulate mode) ─────────────────────

    cb.on_save_preset = []() { save_current_to_user_bank(); };

    cb.on_toggle_bank = []() {
        // Keyboard shortcut cycles: USER → STANDARD → EXPERIMENTAL → USER
        auto cur = static_cast<BankMode>(g_bank_mode.load());
        BankMode next;
        switch (cur) {
        case BankMode::USER:         next = BankMode::STANDARD;     break;
        case BankMode::STANDARD:     next = BankMode::EXPERIMENTAL; break;
        case BankMode::EXPERIMENTAL: next = BankMode::USER;         break;
        default:                     next = BankMode::USER;         break;
        }
        switch_bank(next);
    };

    // ── Initialise GPIO / simulate ──────────────────────────────────

    GpioHw hw;
    if (!hw.init(simulate, cb)) {
        fprintf(stderr, "Failed to initialise hardware.\n");
        audio.shutdown();
        return 1;
    }

    // ── Initialise LED ─────────────────────────────────────────────
    LedDriver led;
    led.init(12, 1);   // GPIO 12 (PWM0 alt), 1 APA106

    // Initialise user presets (factory defaults + saved overrides)
    init_user_presets();

    // Boot into user bank (identical to standard on first boot)
    apply_user_preset(g_user_presets[0]);

    {
        int idx = g_preset.load();
        const UserPreset& u = g_user_presets[idx];
        printf("\nBank: USER  Preset %d%s\n",
               idx + 1, u.saved ? "" : " (factory copy)");
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
        static const char *pe_lbl[] = {"Fall", "Off", "Rise"};
        printf("  Pitch Env: %s\n", pe_lbl[g_pitch_env.load() + 1]);
    }
    printf("\nListening for events (Ctrl-C to quit) ...\n\n");

    // ── Main loop ───────────────────────────────────────────────────
    while (g_running) {
        hw.poll();

        // LED: triple white blink on preset save, otherwise normal update
        if (g_led_save_blink.exchange(false))
            led.blinkSave();
        else
            led.update(g_lfo_waveform.load(), g_lfo_out.load(),
                       g_lfo_depth.load(), g_gate.load(),
                       g_freq.load());

        // Resolve pending shift double-click (fires 350 ms after
        // 2nd press if no 3rd click arrives for triple-click)
        if (g_shift_dblclick_pending.load()) {
            auto elapsed = std::chrono::duration_cast<
                std::chrono::milliseconds>(
                    std::chrono::steady_clock::now()
                    - g_shift_dblclick_time).count();
            if (elapsed > 350) {
                g_shift_dblclick_pending.store(false);
                g_shift_clicks = 0;
                toggle_bank(BankMode::STANDARD);
            }
        }
    }

    printf("\nShutting down.\n");
    led.shutdown();
    audio.shutdown();
    hw.shutdown();
    return 0;
}
