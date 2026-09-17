# Gritbaal Synth — Design & Architecture Document

## 1. Vision & Visual Aesthetic Strategy

**Gritbaal** is an über-gritty, aggressive, character-rich virtual analog synthesizer plugin. Inspired by the brutalist, biomechanical aesthetic depicted in `gritbaal_hero.jpg`, Gritbaal combines industrial steel, copper circuitry, and demonic fiery energy with warm, organic, harmonic analogue imperfection.

### 1.1 Visual & UI Theme (Inspired by `gritbaal_hero.jpg`)
* **Chassis & Panel:** Heavy dark forged iron and brushed gunmetal paneling with copper trim and visible mechanical gears.
* **Control Elements:** Solid metal knurled knobs, chunky retro toggle switches, industrial patch sockets, and amber/red backlighting.
* **Lighting & Indicators:** Glowing volcanic orange/red LEDs, illuminated VU meters, and dynamic embers/glow responding to audio drive and filter resonance.

---

## 2. Front-Panel UI Layout & Panels Sketch

The UI panel is structured into distinct recessed modular sections separated by heavy dark steel borders and copper rivets:

```text
+-------------------------------------------------------------------------------------------------------+
|  [GRITBAAL]  | PRESETS: [<] [01: Hellfire Bass ] [>] [SAVE] | TUNE: (o) OCT: [-1] | MASTER VOL: (o)  |
+-------------------------------------------------------------------------------------------------------+
|  VCO SECTION                 | MIX & DRIVE           | VCF SECTION           | ENVELOPES              |
| +--------------------------+ | +-------------------+ | +-------------------+ | +--------------------+ |
| | VCO1:                    | | | VCO1 VOL:   (o)   | | | CUTOFF:     (o)   | | | ENV1 (FILTER):    | |
| |  WAVE: [SAW/TRI/PULSE]   | | | VCO2 VOL:   (o)   | | | RESONANCE:  (o)   | | |  ATTACK:   (o)    | |
| |  PULSE WIDTH: (o)        | | | RING MOD:   (o)   | | | MODE: [24dB/12dB] | | |  DECAY:    (o)    | |
| | VCO2:                    | | | SUB VOL:    (o)   | | | ENV MOD:    (o)   | | |  SUSTAIN:  (o)    | |
| |  WAVE: [SAW/TRI/PULSE]   | | | NOISE VOL:  (o)   | | | DRIVE (PRE): (o)  | | |  RELEASE:  (o)    | |
| |  DETUNE:     (o)         | | |                   | | |                   | | | ENV2 (AMP):      | |
| |  PITCH/FM:   (o)         | | | OVERDRIVE (TUBE)  | | | DRIVE TYPE:       | | |  ATTACK:   (o)    | |
| |  SYNC: [OFF / ON]        | | | AMOUNT:     (o)   | | | [LADDER / MS20]   | | |  DECAY:    (o)    | |
| +--------------------------+ | +-------------------+ | +-------------------+ | |  SUSTAIN:  (o)    | |
|  MODULATION & LFO            | GLOBAL & DRIFT        | OUTPUT & FX           | |  RELEASE:  (o)    | |
| +--------------------------+ | +-------------------+ | +-------------------+ | +--------------------+ |
| | LFO1 RATE: (o) DEPTH: (o)| | | THERMAL DRIFT: (o)| | | PAN:        (o)   | | BORDER / PANEL FRAME |
| | LFO2 RATE: (o) DEPTH: (o)| | | POWER SAG:     (o)| | | WARMTH VOL: (o)   | | Dark iron & rivets   |
| +--------------------------+ | +-------------------+ | +-------------------+ | +--------------------+ |
+-------------------------------------------------------------------------------------------------------+
```

---

## 3. Audio Signal Flow & Architecture

The following ASCII diagram outlines the signal routing and modulation architecture of Gritbaal:

```text
+----------------------------------------------------------------------------------------------------+
|                                           GRITBAAL SYNTH ENGINE                                     |
+----------------------------------------------------------------------------------------------------+

  MODULATION & GLOBAL DRIFT
  +-----------------------------------------------------------------------------------------+
  |  [ LFO 1 / LFO 2 ] -----> Pitch / PW / Filter Cutoff / Pan                              |
  |  [ ENV 1 (Filter)] ----> Filter Cutoff / Drive Amount / Pitch Mod                       |
  |  [ ENV 2 (Amp)   ] ----> VCA Level / Drive Saturation                                 |
  |  [ Thermal Drift ] ----> Continuous Micro-pitch Walk & Component Mismatch (Per Voice)   |
  |  [ Power Sag     ] ----> Dynamic Rail Voltage Drop under Heavy Low-End Transient Load    |
  +-----------------------------------------------------------------------------------------+

  AUDIO PATH (Per-Voice)

   +----------------+
   | VCO 1          |----+
   | PolyBLEP Saw/Tri|   |
   | Hard Sync Ref  |   |    +---------------+
   +----------------+   +--->| Ring Modulator|-----+
                        |    +---------------+     |
   +----------------+   |                          |
   | VCO 2          |---+                          |
   | PolyBLEP Pulse |                              v
   | FM & Detune    |                        +------------+      +-------------------+      +---------------+
   +----------------+----------------------->| Voice Mixer|----->| Pre-Filter Drive  |----->| VCF Selector  |
                                             | & Sub/Noise|      | (Asymmetric Tanh) |      | (Ladder/MS-20)|
   +----------------+                        +------------+      +-------------------+      +-------+-------+
   | Sub / Noise    |------------------------------^                                                |
   | Sub-Oct / Pink |                                                                               v
   +----------------+                                                                       +---------------+
                                                                                            | Non-Linear    |
                                                                                            | ZDF Filter    |
                                                                                            | Stage         |
                                                                                            +-------+-------+
                                                                                                    |
                                                                                                    v
                                                                                            +---------------+
                                                                                            | Tube/Diode    |
                                                                                            | Post-Overdrive|
                                                                                            +-------+-------+
                                                                                                    |
                                                                                                    v
                                                                                            +---------------+
                                                                                            | VCA & Warmth  |
                                                                                            | Saturation    |
                                                                                            +-------+-------+
                                                                                                    |
                                                                                                    v
                                                                                            +---------------+
                                                                                            | Main Stereo   |
                                                                                            | Output        |
                                                                                            +---------------+
```

---

## 4. Recommended Oscillators & Subsystems

Based on the *Vintage Analog Synthesizer Modeling Compendium* (`docs/vintage_synth_modelleing_compendium.md`), Gritbaal implements component-informed oscillator models featuring:

### 4.1 Dual PolyBLEP VCOs
* **Anti-Aliased Band-Limited Oscillators:** PolyBLEP residual correction applied to Sawtooth, Triangle, Variable Pulse Width, and Ramp waveforms (Compendium Section 14).
* **Hard Sync & Cross Modulation:** Hard synchronization of VCO2 to VCO1, with high-frequency sync transient smoothing. Exponential FM from VCO2 to VCO1 cutoff/pitch.
* **Finite Switch Edge Speed:** Pulse wave transitions modeled with finite rise/fall times and voltage overshoot to add authentic edge character (Compendium Section 16).
* **Sub-Oscillator & Noise Generator:** Sub-octave square wave generator and a colored noise generator (selectable White/Pink noise with thermal leakage).

### 4.2 Thermal Drift & Per-Voice Mismatch Model
* **Static Tolerance Mismatch:** Random per-voice initial component offset (VCO pitch ±5 cents, filter cutoff ±8%, envelope attack/decay times ±10%) to simulate analog hardware component tolerances (Compendium Section 8).
* **Dynamic Thermal Drift:** Continuous low-frequency random walk (1/f noise process) simulating thermal variations across transistors (Compendium Section 9 & 15).
* **Warm-Up Drift Model:** Slow drift curve during the first 60 seconds after initialization (Compendium Section 44).

---

## 5. Recommended Filters & Non-Linearities

Gritbaal features a selectable dual-filter architecture designed for maximum grit, screeching self-oscillation, and fat low-end retention.

### 5.1 Topology 1: Non-Linear ZDF Transistor Ladder (Moog Style)
* **Topology:** 4-pole 24dB/octave zero-delay feedback (TPT/ZDF) ladder filter (Compendium Section 20 & 24).
* **Non-Linear Transistor Saturation:** Each 1-pole stage incorporates differential pair $\tanh(v / (2 V_T))$ non-linear voltage transfer curves (Compendium Section 3 & 52).
* **Resonance Drive & Saturation:** Non-linear feedback loop with soft-clipping diodes, causing resonance to compress smoothly and distort when driven hard.

### 5.2 Topology 2: Diode Ring / Sallen-Key Filter (Korg MS-20 Style)
* **Topology:** 2-pole 12dB/octave Sallen-Key diode bridge filter (Compendium Section 31).
* **Gritty Diode Clipping:** Diode limiter non-linearities in the feedback path producing the iconic MS-20 aggressive, screaming self-oscillation and raw harmonic bite.
* **Current status:** the Sallen-Key path is implemented and runs inside the same 4x oversampling loop as the ladder, but its feedback nonlinearity is still a symmetric `tanh`, not yet the asymmetric diode curve the real K35/MS-20 circuit has. Tracked in [filter-architecture-plan.md](filter-architecture-plan.md).

### 5.3 Toward Multiple Vintage Targets
`FilterType` today only distinguishes `TransistorLadder` and `SallenKey`, both used generically — there is no per-synth identity yet (e.g. a TB-303-flavored mismatched-capacitor ladder vs. a matched-capacitor Minimoog/ARP 2600 ladder). The plan for exposing distinct TB-303 / Minimoog / ARP 2600 (4012 & 4072) / MS-20 / Jupiter-8 / CS-80 character presets — grounded in each instrument's actual circuit per the compendium — lives in [filter-architecture-plan.md](filter-architecture-plan.md).

### 5.4 Overdrive & Power Supply Sag Modeling
* **Pre-Filter Drive:** Variable input gain stage pushing the filter into heavy harmonic saturation before filtering.
* **Post-Filter Tube/Diode Waveshaper:** Asymmetric waveshaper ($y = \tanh(x + 0.15 x^2)$) introducing even-harmonic tube warmth and heavy overdrive.
* **Power Supply Rail Sag:** Dynamic reduction of internal supply headroom under heavy bass transients, modulating high-frequency gain and creating dynamic compression (Compendium Section 10).

---

## 6. Modulation & Envelopes

### 6.1 RC Circuit-Modeled Envelopes
* **Analog RC Response:** Exponential ADSR curves based on capacitor charge/discharge equations ($V(t) = V_{target} + (V_0 - V_{target}) e^{-t / \tau}$) rather than linear slopes (Compendium Section 17).
* **Capacitor Memory & Retriggering:** Retriggering a note before decay finishes preserves residual capacitor voltage, producing organic attack transients (Compendium Section 5).

### 6.2 LFOs & Modulation Routing
* Dual LFOs with Saw, Triangle, Square, and Random S&H shapes.
* Syncable to host tempo (via CLAP transport interface).
* Direct modulation routing to Pitch, Pulse Width, Cutoff, Resonance, Drive, and Pan.

---

*Document prepared for Gritbaal Synth emulation project, based on `docs/vintage_synth_modelleing_compendium.md`.*
