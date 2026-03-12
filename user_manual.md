# Dub Siren User Manual

A single-voice synthesizer for live dub and reggae sound-system use. Produces classic siren wails, laser shots, machine-gun stutters, and deep sub-bass effects on a Raspberry Pi.

---

## Control Surface Layout

```
    ╔══════════════════════════════════════════════════════════════╗
    ║                                                              ║
    ║     ┌───┐          ┌───┐          ┌───┐                      ║
    ║     │ ◎ │          │ ◎ │          │ ◎ │                      ║
    ║     └───┘          └───┘          └───┘                      ║
    ║    KNOB 1          KNOB 2         KNOB 3                     ║
    ║   Frequency       LFO Rate     Filter Cutoff                 ║
    ║  +Bank: Depth   +Bank: Release +Bank: Resonance              ║
    ║                                                              ║
    ║     ┌───┐          ┌───┐         ╔═══════╗                   ║
    ║     │ ◎ │          │ ◎ │         ║ Rise  ║ ▲                 ║
    ║     └───┘          └───┘         ║-------║                   ║
    ║    KNOB 4          KNOB 5        ║  Off  ║ ● ← Toggle       ║
    ║   Delay Time    Delay Feedback   ║-------║     Switch        ║
    ║  +Bank: Dly Mix +Bank: Rev Mix   ║ Fall  ║ ▼                 ║
    ║                                  ╚═══════╝                   ║
    ║                               PITCH ENVELOPE                 ║
    ║                                                              ║
    ║    ╭─────────╮   ╭─────────╮   ╭───────────────╮             ║
    ║    │ PRESET  │   │  BANK   │   │   TRIGGER     │             ║
    ║    ╰─────────╯   ╰─────────╯   ╰───────────────╯             ║
    ║    Tap: Next     Hold: Layer B   Hold: Sound ON              ║
    ║    preset        Dbl: User bank  Release: Fade               ║
    ║    Hold 3s: Save Tri: Exp bank                               ║
    ║                                                              ║
    ╚══════════════════════════════════════════════════════════════╝
```

---

## Signal Flow

```
Trigger Button ──── gate on/off
       │
       ▼
 ┌───────────┐       ┌───────────┐
 │    LFO    │──────▶│   Base    │
 │ (8 shapes)│       │ Frequency │
 └───────────┘       └─────┬─────┘
                           │
                     ┌─────▼─────┐
                     │   Pitch   │  Rise / Off / Fall
                     │ Envelope  │
                     └─────┬─────┘
                           │
                     ┌─────▼─────┐
                     │ Oscillator│  Sine / Square / Saw / Triangle
                     └─────┬─────┘
                           │
                     ┌─────▼─────┐
                     │  Moog LP  │  4-pole low-pass filter
                     │  Filter   │  (sweeps down on release)
                     └─────┬─────┘
                           │
                     ┌─────▼─────┐
                     │  Volume   │  5 ms attack
                     │ Envelope  │  variable release
                     └─────┬─────┘
                           │
                     ┌─────▼─────┐
                     │Tape Delay │  wobble + flutter
                     └─────┬─────┘
                           │
                     ┌─────▼─────┐
                     │  Spring   │  dual-spring tank
                     │  Reverb   │
                     └─────┬─────┘
                           │
                     ┌─────▼─────┐
                     │   Soft    │  warm saturation
                     │  Limiter  │
                     └─────┬─────┘
                           │
                           ▼
                    Stereo Output
                   (48 kHz, 24-bit)
```

---

## Buttons

### TRIGGER

Hold to produce sound. Release to let it fade out (fade duration set by Release Time). The LFO phase resets each time you press it, so the siren sweep starts from the same point every time.

### BANK / SHIFT

- **Hold** while turning any knob to access the Layer B parameter for that knob (see table below).
- **Double-click** to toggle between User and Standard preset banks.
- **Triple-click** to toggle between User and Experimental preset banks.

### PRESET

- **Tap** to cycle through presets in the current bank (0 &rarr; 1 &rarr; 2 &rarr; 3 &rarr; 0).
- **Hold Bank + Tap Preset** to cycle the LFO waveform.
- **Hold for 3 seconds** to save the current sound to the active User preset slot.

---

## Knobs (Rotary Encoders)

Every knob has two functions: **Layer A** (normal) and **Layer B** (hold Bank while turning).

Turn slowly for fine adjustments, turn quickly for larger jumps (acceleration).

### Layer A (Bank NOT held)

| Knob | Parameter | Range | What it does |
|------|-----------|-------|--------------|
| 1 | **Base Frequency** | 30 -- 8,000 Hz | Pitch of the siren, in semitone steps |
| 2 | **LFO Rate** | 0.1 -- 20 Hz | How fast the pitch wobbles. Slow = sweep, fast = stutter |
| 3 | **Filter Cutoff** | 20 Hz -- 20 kHz | Opens / closes the low-pass filter. Right = bright, left = dark |
| 4 | **Delay Time** | 1 ms -- 1 sec | Echo spacing. Short = slapback, long = spacious |
| 5 | **Delay Feedback** | 0 -- 95% | Number of echo repeats. High values build up -- be careful above 85% |

### Layer B (hold Bank while turning)

| Knob | Parameter | Range | What it does |
|------|-----------|-------|--------------|
| 1 | **LFO Depth** | 0 -- 100% | How much the LFO sweeps the pitch. 0% = none, 100% = full |
| 2 | **Release Time** | 10 ms -- 5 sec | Fade-out duration after releasing Trigger |
| 3 | **Filter Resonance** | 0 -- 95% | Boost at the cutoff point. Above ~85% the filter self-oscillates |
| 4 | **Delay Mix** | 0 -- 100% | Wet/dry balance for the echo effect |
| 5 | **Reverb Mix** | 0 -- 100% | Wet/dry balance for the spring reverb |

---

## Pitch Envelope Switch

A 3-position toggle switch that controls what happens to the pitch when you release the Trigger:

| Position | Effect |
|----------|--------|
| **Rise** (up) | Pitch slides UP 3 octaves while fading -- ascending laser effect |
| **Off** (centre) | No pitch change -- LFO does all the movement |
| **Fall** (down) | Pitch slides DOWN 3 octaves -- classic descending dub siren |

The filter also sweeps downward on release regardless of this switch position, adding the characteristic dub "dive" to the tail.

---

## LFO Waveforms

Cycle through these with **Bank + Tap Preset**:

| # | Shape | Character |
|---|-------|-----------|
| 1 | Sine | Smooth, classic siren sweep |
| 2 | Triangle | Linear ramps, slightly edgier than sine |
| 3 | Square | Hard chop between two pitches, stutter effect |
| 4 | Ramp Up | Pitch rises steadily, snaps back down |
| 5 | Ramp Down | Pitch falls steadily, snaps back up |
| 6 | Sample & Hold | Random stepped pitches, robotic chaos |
| 7 | Exp Rise | Slow start, fast finish upward |
| 8 | Exp Fall | Slow start, fast finish downward |

---

## Preset Banks

### User Bank (default on boot)

Four editable slots loaded at startup. These begin as copies of the Standard presets and can be overwritten by holding the Preset button for 3 seconds. On kiosk systems with persistent storage configured, your edits survive power cycles.

### Standard Bank (double-click Bank)

| Slot | Name | Sound |
|------|------|-------|
| 0 | Slow Wail | Classic 10-second sweeping siren with deep delay and long reverb |
| 1 | Machine Gun | Rapid-fire stutter -- square LFO chopping at 14 Hz |
| 2 | Lickshot | Laser-gun zaps with fast triangle LFO |
| 3 | Droppa | Descending siren wail with heavy feedback |

### Experimental Bank (triple-click Bank)

| Slot | Name | Sound |
|------|------|-------|
| 0 | Insect Swarm | Chaotic S&H LFO at 18 Hz, near-self-oscillating filter |
| 1 | Depth Charge | Sub-bass sine with extreme delay feedback -- underwater explosions |
| 2 | Glitch Storm | Rapid upward pitch sweeps, stuttering digital chaos |
| 3 | Foghorn From Hell | Monstrous 55 Hz sawtooth, maximum reverb, 4-second release |

---

## Tips for Live Use

- **Quick sound swap**: Tap Preset to jump between your four saved sounds instantly.
- **Ride the filter**: Knob 3 is your most expressive real-time control. Sweep it while holding Trigger for dramatic filter effects.
- **Dub echo buildup**: Crank Delay Feedback (Knob 5) past 80% to create self-sustaining echo buildups. Back it off before it runs away.
- **Self-oscillating filter**: Hold Bank + turn Knob 3 past 85% resonance. The filter becomes a pure tone generator -- sweep the cutoff to play it like an instrument.
- **Stutter effects**: Set LFO to Square waveform with a fast rate (10+ Hz) for rhythmic chopping.
- **Sub drops**: Set frequency low (60-120 Hz), pitch envelope to Fall, long release. Press and release Trigger for floor-shaking descending sub-bass.
- **Save before you lose it**: When you dial in a sound you like, hold Preset for 3 seconds to save it to the current User slot.

---

## Boot Defaults

| Parameter | Default Value |
|-----------|---------------|
| Frequency | 440 Hz |
| LFO Rate | 0.35 Hz |
| LFO Depth | 35% |
| LFO Shape | Sine |
| Filter Cutoff | 8,000 Hz |
| Filter Resonance | 0% |
| Delay Time | 375 ms |
| Delay Feedback | 55% |
| Delay Mix | 30% |
| Reverb Mix | 35% |
| Release Time | 50 ms |
| Pitch Envelope | Fall |
