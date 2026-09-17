# Gritbaal Filter Architecture — Multi-Synth Target Plan

This document turns the general reference material in
[`vintage_synth_modelleing_compendium.md`](vintage_synth_modelleing_compendium.md)
into a concrete plan for `src/core/Filter.cpp` / `src/core/Filter.hpp` /
`src/core/SynthEngine.cpp`. It originates from a design/review conversation
about those files and has been re-checked against the current code (the
filter DSP has changed substantially since that conversation — most of the
issues raised there are already fixed; this document reflects current
reality, not the original review).

## 1. Current implementation status

As of the current `src/core/Filter.cpp` / `Filter.hpp` / `SynthEngine.cpp`:

### Resolved

- **Real oversampling.** `Filter::processSample` runs a genuine 16-tap
  polyphase FIR upsample → process-at-4x → downsample, replacing the earlier
  linear-interpolation stand-in. Both filter topologies run inside the 4x
  loop via `processOversampledSample`.
- **ZDF ladder is the live path.** The 4-pole transistor-ladder core
  (`processOversampledSample`, non-`SallenKey` branch) and the Sallen-Key
  MS-20-style core (`processSallenKeySample`) are both TPT/ZDF one-pole
  chains solved with Newton-Raphson, not dead code behind an unused
  RK2 path.
- **Newton-Raphson with convergence check.** Both nonlinear feedback solves
  iterate up to 5 times and break early once the step size is below
  `1e-6`, instead of a fixed 3-iteration budget with no exit condition.
- **Nyquist-aware cutoff clamp.** `maxCutoff = min(18000, 0.49 * nyquist)`
  is computed against `oversampledRate_`, so the clamp stays safe even if
  the host sample rate drops.
- **Cutoff exponent fixed in `SynthEngine.cpp`.** `cv_total = 6.90689f *
  cTaper` (== `log2(18000/150)`), so the filter can reach its full range
  instead of topping out around ~1875 Hz.
- **LFO rate no longer sticks.** `lfo1_.setRate(...)` / `lfo2_.setRate(...)`
  are called unconditionally every sample.
- **Amp modulation scales rather than replaces the envelope.**
  `vcaEnvVal = env2Val * clamp(1 + modAmp, 0, 2)` preserves ADSR shaping
  under Amp-target modulation.
- **Single source of truth.** The earlier `syrebas`-namespace second
  `Filter.cpp` (a relabeled BJT ladder mis-described as a TB-303 model) is
  gone; there is one `gritbaal::Filter`.

### Still open

- **No oversampling on the post-filter waveshapers.** `warmthAmount` and
  `overdriveAmount` (`SynthEngine.cpp`, after `filter_.processSample`
  returns) run at the base sample rate, after the filter's internal 4x
  buffers have already downsampled back down. These are `tanh`/polynomial
  shapers pushed deliberately hard for character, so they are at least as
  alias-prone as the filter core was before its oversampling fix — this is
  the largest remaining gap from the original review.
- **Resonance-scaling constants are hand-tuned, not calibrated.**
  `resGain = resNorm * 16.5f` (ladder) and `k = resNorm * 2.2f`
  (Sallen-Key) aren't checked against a measured or published
  self-oscillation threshold for any specific instrument. Per compendium
  §41/§68, these should become named calibration parameters with an
  explicit confidence level, not implicit constants.
- **MS-20 path uses a symmetric saturator.** `processSallenKeySample`'s
  feedback nonlinearity is `tanh(k * y2)`. The real K35/MS-20 circuit is
  diode-based and asymmetric (compendium §3.3, §31); a symmetric `tanh`
  will not produce the even-harmonic content real diode asymmetry gives.
- **No per-synth-identity mapping.** `FilterType` only has two values —
  `TransistorLadder` and `SallenKey` — used generically. There is no
  concept yet of "this ladder instance is a TB-303" vs "this ladder
  instance is a Minimoog" vs "this ladder instance is an ARP 2600 4012",
  even though those are materially different targets per the compendium
  (§30, §24, §77).
- **Capacitor mismatch is a single hard-coded pattern, applied to every
  ladder use.** `Filter.hpp`'s `capScale1_..capScale4_` are
  `{1.0, 0.6667, 0.3030, 1.0}` — a deliberate stage mismatch, permanently
  baked into the one `TransistorLadder` topology. This happens to be
  TB-303-flavored (compendium §30.3), but it means every use of
  `TransistorLadder` — including a hypothetical clean Minimoog/ARP2600/
  Jupiter-8/CS-80 preset — inherits the same mismatch. See §3 below.

## 2. Saturator / Solver / Topology axes

The compendium's per-synth reference (§24–§30, §77) and the "Recommended
saturator/solver pairing per synth" analysis reduce to three largely
independent axes:

1. **Saturator family** — `tanh` (correct closed form for a matched BJT
   differential pair: Minimoog, ARP 2600 both revisions, Jupiter-8,
   CS-80) vs. an asymmetric/diode-derived curve (TB-303, MS-20 — real
   circuits with diodes, which are asymmetric and generate even
   harmonics that `tanh` cannot).
2. **Solver** — fixed iteration count / plain ZDF (cheap, fine when not
   pushed into self-oscillation) vs. Newton-Raphson with more iterations
   (needed when resonance/drive is pushed hard and convergence quality
   matters more).
3. **Topology** — stage count, per-stage capacitor scale, and whether
   resonance is fed from an external network (IR3109-style) or an
   internal one (ladder/Sallen-Key).

Newton-Raphson needs the saturator's derivative, so a saturator policy
should expose both `value(x)` and `deriv(x)`:

```cpp
struct TanhSaturator {
    static float value(float x) { return std::tanh(x); }
    static float deriv(float x) { float t = std::tanh(x); return 1.0f - t*t; }
};

struct AsymmetricSaturator {
    static float value(float x) {
        return x >= 0.0f ? std::tanh(x)
                          : (std::exp(2*0.7f*x)-1.0f)/(std::exp(2*0.7f*x)+1.0f)*1.3f;
    }
    static float deriv(float x) { /* piecewise */ return 0.0f; /* TODO */ }
};
```

This is a refactor target, not a blocker — the current inline
`std::tanh(...)` calls in `Filter.cpp` are correct for the two topologies
that exist today. Templating only pays off once a third saturator family
(diode/Shockley for MS-20, or a distinct one for TB-303) is added.

## 3. Recommended per-synth pairing

| Synth | Perf-tier combo | Accuracy-tier combo | Rationale |
|---|---|---|---|
| TB-303 | `AsymmetricSaturator` + `ZDFSolver<1>` | `ShockleySaturator` (true diode I-V) + `NewtonSolver<3-4>`, mismatched per-stage `capScale` | Mono — accuracy is cheap. The capacitor mismatch (§30.3 of the compendium) is the sound; a symmetric tanh ladder can't get there at any solver quality. |
| Minimoog | `TanhSaturator` + `ZDFSolver<2>` | `TanhSaturator` + `NewtonSolver<4+>` or WDF ladder | Mono. `tanh` is already correct; the accuracy lever is solver quality, not saturator swap. Needs *matched* `capScale` (unlike TB-303). |
| ARP 2600 (4012) | `TanhSaturator` + `ZDFSolver<2>` | `TanhSaturator` + `NewtonSolver<4+>` / WDF | Same family/reasoning as Minimoog — the 4012 is a Moog-derived ladder. Mono, so accuracy is cheap. |
| ARP 2600 (4072) | `TanhSaturator` + `ZDFSolver<2>`, lower control-current ceiling | `TanhSaturator` + `NewtonSolver<4+>` | Same saturator family as 4012, but distinct pole-scaling/gain-staging so the ~7-12kHz practical ceiling emerges from the topology, not a post-hoc low-pass trim. |
| Korg MS-20 | `AsymmetricSaturator` + `ZDFSolver<2>` | `ShockleySaturator` + `NewtonSolver<3-4>` | Mono, self-oscillating diode-asymmetric behavior — `tanh` underserves it at any solver quality. |
| Jupiter-8 | `TanhSaturator` + `ZDFSolver<1>` (or linear TPT, no iteration) | `TanhSaturator` + `NewtonSolver<2-3>` | Polyphonic (8 voices) — tightest CPU budget. Stock unit avoids the self-osc regime, so convergence quality matters less. |
| CS-80 | `TanhSaturator` + `ZDFSolver<1>` | `TanhSaturator` + `NewtonSolver<2-3>` | Polyphonic, same reasoning as Jupiter-8. |

Only two saturator families are actually needed (symmetric BJT-style
`tanh` vs. asymmetric diode-style), crossed with a solver-quality dial —
not one bespoke nonlinearity per synth. Solver iteration count is mostly
a performance lever, not an authenticity one; the accuracy-sensitive axis
is the saturator family, driven by whether the real circuit had diodes
(TB-303, MS-20) or matched differential pairs (everything else in this
table).

## 4. Concrete next steps for `Filter.hpp` / `Filter.cpp`

Given the current two-topology, hard-coded-constants implementation
(§1 above), the incremental path to the multi-synth-target plan is:

1. **Make `capScale1_..capScale4_` a preset, not a constant.** Move the
   `{1.0, 0.6667, 0.3030, 1.0}` mismatch pattern behind a named TB-303
   preset; give Minimoog/ARP2600/Jupiter-8/CS-80 presets a matched
   `{1,1,1,1}` (or near-matched, tolerance-level) pattern instead. Today
   every `TransistorLadder` use — regardless of which synth it's meant to
   represent — inherits the TB-303-flavored mismatch.
2. **Add an asymmetric/diode saturator for the Sallen-Key path** and use
   it for MS-20 instead of the current symmetric `tanh(k * y2)`.
3. **Turn `resGain`/`k` into named, documented calibration constants**
   per preset rather than one global formula, per compendium §41/§68 —
   at minimum, comment each with its confidence level (A/B/C) and what it
   was tuned against.
4. **Route `warmthAmount`/`overdriveAmount` through oversampling.** These
   post-filter shapers in `SynthEngine.cpp` are the largest remaining
   aliasing gap; they should ride through the same 4x FIR buffers already
   built for the filter core (structurally, run them *inside*
   `Filter::processSample`'s oversampling loop, or expose a second
   `processSample`-shaped entry point that includes them before the final
   decimation stage).
5. **Only once presets 1-4 exist for at least three synths**, consider
   introducing the templated `Saturator`/`Solver`/topology split from §2 —
   it earns its complexity once there's a real cross-product to manage,
   not before.

## 5. Dev vs. release build split (future)

Once there is more than one saturator/topology pairing to A/B test, gate
the full combinatorial matrix behind a build flag so release builds only
link the curated, per-synth-preset set (type resolved once per block via
`std::visit`, not per sample):

```cmake
option(FILTER_DEV_EXPLORATION "Compile full filter combinatorial matrix for A/B testing" OFF)
if(FILTER_DEV_EXPLORATION)
    target_compile_definitions(gritbaal PRIVATE FILTER_DEV_EXPLORATION)
endif()
```

This is not urgent while there are only two topologies; revisit once
step 5 above is underway.

## 6. Cross-references

- Hardware/circuit background for each target synth: see
  [`vintage_synth_modelleing_compendium.md`](vintage_synth_modelleing_compendium.md)
  §24 (Minimoog), §30 (TB-303), §31 (MS-20), §28 (Jupiter-8), §25 (CS-80),
  §77 (ARP 2600).
- Roadmap tracking: see
  [`implementation_roadmap.md`](implementation_roadmap.md) Phase 2.
