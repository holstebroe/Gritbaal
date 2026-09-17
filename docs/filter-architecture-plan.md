# Gritbaal Filter Architecture — Multi-Synth Target Plan

This document turns the general reference material in
[`vintage_synth_modelleing_compendium.md`](vintage_synth_modelleing_compendium.md)
into a concrete plan for `src/core/Filter.cpp` / `src/core/Filter.hpp` /
`src/core/SynthEngine.cpp`. It originates from a design/review conversation
about those files and has been re-checked against the current code multiple
times as the implementation progressed; it reflects current reality, not
the history of how it got there.

**Scope decision (current):** focus on monophonic targets first —
Minimoog, ARP 2600, TB-303, MS-20. Jupiter-8 and CS-80 are polyphonic and
explicitly deferred; revisit them once the mono targets are calibrated.
"ARP 2600" here means the "4012" board specifically (the Moog-derived,
better-sounding of its two production VCF revisions) — the "4072" board
is a later addition, not yet modeled.

## 1. Current implementation status

`Filter.hpp`/`Filter.cpp` now implement the templated Saturator/Solver/
Topology split described in §2-3 below, wired up for the four mono targets.

### Implemented

- **Saturator policies:** `TanhSaturator` (matched-BJT-pair family —
  Minimoog, ARP 2600) and `AsymmetricSaturator` (diode-asymmetry family —
  TB-303, MS-20), each exposing `value(x)`/`deriv(x)` for Newton-Raphson.
- **Solver policies:** `NewtonSolver<Saturator, Iterations>` (accuracy
  tier — wired into every shipped preset, since all four targets are
  monophonic and the extra iterations/derivative are cheap) and
  `FixedPointSolver<Saturator, Iterations>` (performance tier — a real,
  compiled building block, not yet wired into a shipped preset; reserved
  for when Jupiter-8/CS-80 polyphony work resumes and CPU budget matters).
- **Topology templates:** `LadderFilter<Saturator, Solver>` (4-pole,
  HPF-filtered resonance feedback) and `SallenKeyFilter<Saturator, Solver>`
  (2-pole diode feedback), both generalized from the previous
  monolithic `processOversampledSample`/`processSallenKeySample` code —
  same math, parameterized nonlinearity and solver.
- **`VintageFilterModel` enum + 4 concrete cores in `Filter`:**
  - `Minimoog` → `LadderFilter<TanhSaturator, NewtonSolver<...>>`, matched
    capacitor scale `{1,1,1,1}`.
  - `Arp2600` → same template instantiation as Minimoog (shared saturator
    family, per the pairing table), separate instance/state, matched
    capacitor scale. Currently uses the *same* tuning constants as
    Minimoog — see "Still open" below.
  - `TB303` → `LadderFilter<AsymmetricSaturator, NewtonSolver<...>>` with
    the mismatched capacitor scale `{1.0, 0.6667, 0.3030, 1.0}` (compendium
    §30.3 / Stinchcombe's analysis).
  - `MS20` → `SallenKeyFilter<AsymmetricSaturator, NewtonSolver<...>>` —
    this also fixes the previously-open "MS-20 uses a symmetric saturator"
    gap.
  - Switching `VintageFilterModel` resets the newly active core so stale
    capacitor/feedback state doesn't resurface mid-performance.
- **GUI:** the old binary LADDER/MS20 toggle switch is replaced by a
  drag-through-list selector ("VCF MODEL": MINIMOOG / ARP2600 / TB-303 /
  MS-20), reusing the same click-and-drag-to-cycle interaction already used
  for the LFO/ENV mod-target selectors, but without their right-click
  mod-target-assignment behavior (see `ControlType::OptionSelector` in
  `src/gui/GuiWindow.hpp`/`.cpp`).
- Everything from the previous round (real FIR oversampling, Newton
  convergence checks, Nyquist-aware clamp, cutoff exponent, LFO
  sticking, amp-modulation-replaces-envelope) remains fixed — see git
  history for that pass.

### Still open

- **No oversampling on the post-filter waveshapers.** `warmthAmount` and
  `overdriveAmount` (`SynthEngine.cpp`, after `filter_.processSample`
  returns) still run at the base sample rate, after the filter's internal
  4x buffers have already downsampled back down. Largest remaining
  aliasing gap.
- **ARP 2600 is not yet acoustically distinguished from Minimoog.** Both
  use identical capacitor scale and resonance-gain constants today. The
  compendium (§77) doesn't give a precise numeric distinction for the
  "4012" board beyond "closely related to a Moog ladder", so no
  differentiating constant has been invented — this needs either a SPICE
  reconstruction or a service-manual/measurement cross-check before
  fabricating one. `arp2600Core_` is already a separate instance so this
  is a pure calibration change, not another refactor, once real data
  exists.
- **Resonance-scaling constants are still hand-tuned, not calibrated.**
  `resGainScale_` (16.5 for the ladder, 2.2 for Sallen-Key) is shared
  across all presets that use a given topology, rather than measured per
  instrument. Per compendium §41/§68, these should become named,
  per-preset calibration constants with an explicit confidence level.
- **`AsymmetricSaturator` is a cheap approximation, not a true diode
  solve.** It's a reasonable, differentiable stand-in for diode-clipper
  asymmetry (positive half unity `tanh`, negative half scaled/shallower
  `tanh`), not a Shockley-equation solve. Good enough for the "only two
  saturator families are needed" conclusion below; a from-schematic diode
  model would be a separate, much larger effort with its own confidence
  caveats.
- **`FixedPointSolver` is unused in the shipped build.** It compiles and
  is unit-tested implicitly by the type system, but no preset instantiates
  it yet — there's no CPU pressure to justify it while every target is
  monophonic.
- **Jupiter-8 / CS-80 remain unimplemented.** Deferred by scope decision,
  not forgotten — see priority matrix in the compendium §73.

## 2. Saturator / Solver / Topology axes

Three largely independent axes, implemented as C++ policy templates in
`Filter.hpp`:

1. **Saturator family** — `TanhSaturator` (correct closed form for a
   matched BJT differential pair: Minimoog, ARP 2600) vs.
   `AsymmetricSaturator` (diode-asymmetry approximation: TB-303, MS-20).
2. **Solver** — `NewtonSolver<Saturator, Iterations>` (accuracy tier,
   needs the saturator's derivative) vs. `FixedPointSolver<Saturator,
   Iterations>` (performance tier, derivative-free, cheaper per
   iteration but needs more of them).
3. **Topology** — `LadderFilter<Saturator, Solver>` (4-pole,
   HPF-filtered resonance feedback) vs. `SallenKeyFilter<Saturator,
   Solver>` (2-pole diode feedback). Per-stage capacitor scale and
   resonance-gain scale are *runtime* configuration on the topology
   class, not template parameters — that's what lets Minimoog and ARP
   2600 share one `LadderFilter<TanhSaturator, ...>` instantiation while
   differing (once calibrated) only in their numeric tuning.

```cpp
struct TanhSaturator {
    static float value(float x) { return std::tanh(x); }
    static float deriv(float x) { float t = std::tanh(x); return 1.0f - t*t; }
};

struct AsymmetricSaturator {
    static constexpr float kNegSlope = 0.7f;
    static constexpr float kNegScale = 1.3f;
    static float value(float x) {
        return x >= 0.0f ? std::tanh(x) : kNegScale * std::tanh(kNegSlope * x);
    }
    static float deriv(float x) { /* piecewise, see Filter.hpp */ }
};

template <typename Saturator, int Iterations>
struct NewtonSolver {
    static float solve(float f0, float G, float k, float initialGuess);
    // solves: y + G * Saturator::value(k*y) - f0 == 0
};
```

Both `LadderFilter` and `SallenKeyFilter` reduce their nonlinear feedback
solve to that same `y + G*Sat(k*y) - f0 = 0` form (with different `G`/`k`
expressions derived from the topology), so one `Solver::solve(f0, G, k,
guess)` signature serves both topologies.

## 3. Per-synth pairing (implemented subset)

| Synth | Saturator | Solver (shipped) | Topology | capScale | Status |
|---|---|---|---|---|---|
| Minimoog | `TanhSaturator` | `NewtonSolver<5>` | `LadderFilter` | `{1,1,1,1}` | Implemented |
| ARP 2600 (4012) | `TanhSaturator` | `NewtonSolver<5>` | `LadderFilter` | `{1,1,1,1}` | Implemented, shares Minimoog's tuning pending calibration data |
| TB-303 | `AsymmetricSaturator` | `NewtonSolver<5>` | `LadderFilter` | `{1.0, 0.6667, 0.3030, 1.0}` | Implemented |
| Korg MS-20 | `AsymmetricSaturator` | `NewtonSolver<5>` | `SallenKeyFilter` | n/a | Implemented |
| Jupiter-8 | `TanhSaturator` | `FixedPointSolver` (perf) | `LadderFilter`-derived, external resonance | n/a | Deferred — polyphonic |
| CS-80 | `TanhSaturator` | `FixedPointSolver` (perf) | 2-pole SVF (not yet modeled) | n/a | Deferred — polyphonic |

Only two saturator families are needed (matched-BJT `tanh` vs.
diode-asymmetric), crossed with a solver-quality dial — not one bespoke
nonlinearity per synth. Solver choice is mostly a performance lever, not
an authenticity one; the accuracy-sensitive axis is the saturator family,
which is why every shipped mono preset uses the accuracy-tier
`NewtonSolver` — the CPU cost difference against `FixedPointSolver` only
matters once voices multiply (Jupiter-8/CS-80).

## 4. Next steps

1. **Calibrate ARP 2600 away from Minimoog's exact tuning**, once a
   source (SPICE reconstruction, service manual, or measurement) gives a
   real distinguishing constant — see "Still open" above.
2. **Route `warmthAmount`/`overdriveAmount` through oversampling** —
   still the largest aliasing gap; run them inside `Filter::processSample`'s
   existing 4x FIR loop instead of after it.
3. **Turn `resGainScale_` into a documented, per-preset calibration
   value** instead of the current shared 16.5/2.2 defaults, per compendium
   §41/§68.
4. **When resuming Jupiter-8/CS-80:** wire `FixedPointSolver` into an
   actual preset for the first time, and add the IR3109-style
   external-resonance-network topology and the CS-80 2-pole SVF topology
   (neither exists yet — `LadderFilter`/`SallenKeyFilter` don't cover
   them).

## 5. Dev vs. release build split (future)

Not needed yet — there are only 6 template instantiations in the shipped
build (2 Ladder × 2 saturator families is actually just 2, since Minimoog
and ARP2600 share one; plus 1 SallenKey instantiation; plus the
currently-unused `FixedPointSolver` variants, which don't get instantiated
unless something references them). Revisit the `FILTER_DEV_EXPLORATION`
build-flag idea from earlier drafts of this document only if a much larger
combinatorial matrix needs A/B testing later (e.g. once Jupiter-8/CS-80
and a from-schematic diode saturator are both in play).

## 6. Cross-references

- Hardware/circuit background for each target synth: see
  [`vintage_synth_modelleing_compendium.md`](vintage_synth_modelleing_compendium.md)
  §24 (Minimoog), §30 (TB-303), §31 (MS-20), §28 (Jupiter-8), §25 (CS-80),
  §77 (ARP 2600).
- Roadmap tracking: see
  [`implementation_roadmap.md`](implementation_roadmap.md) Phase 2.
- Implementation: `src/core/Filter.hpp` / `Filter.cpp`, GUI selector in
  `src/gui/GuiWindow.hpp` / `.cpp` (`ControlType::OptionSelector`).
