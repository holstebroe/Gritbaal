# Vintage Analog Synthesizer Modeling Compendium

## Circuit-Level Character, Imperfections, Non-Linearities, Fast Approximations, and Reference Architectures

**Purpose:** this document is a reusable engineering reference for developing virtual-analog synthesizers and individual modules/agents whose behavior is intentionally imperfect, nonlinear, revision-specific, and characterful in the way vintage analogue hardware is imperfect.

The emphasis is not on making a technically pristine synthesizer. The emphasis is on reproducing the *causes* of the sound: circuit topology, finite headroom, transistor/diode/OTA transfer functions, capacitor memory, control-voltage bleed, component mismatch, power-supply coupling, oscillator drift, envelope timing errors, switching transients, and interactions between modules.

The document provides two implementation levels throughout:

- **High-accuracy / component-informed model:** model the relevant circuit equations, nonlinear devices, continuous state variables and zero-delay feedback paths. Use this where the circuit topology itself is an important contributor to the instrument identity.
- **Fast approximation / phenomenological model:** use calibrated nonlinear curves, TPT/ZDF filters, oversampled waveshapers, per-voice mismatch and slow drift. Use this where CPU cost or incomplete circuit documentation prevents a full model.

Where exact original component values or measurements are unavailable, a value is explicitly described as **nominal**, **typical**, **approximate**, or **calibration target**. Do not silently treat an approximation as an original factory specification.

---

# 1. What “Analog Warmth” Actually Is

“Warmth” is not one effect. A vintage synthesizer is a collection of nonlinear, time-varying, imperfect analogue subsystems whose errors interact.

A useful mental model is:

```text
                 +--------------------+
Pitch CV ------> | VCO                |
                 | drift / nonlinear  |
                 +---------+----------+
                           |
                           v
                    +-------------+
Audio sources ----> | mixer / VCA  |
                    +------+------+ 
                           |
                           v
                    +-------------+
Env / CV ---------->| VCF         |<----- resonance feedback
                    | nonlinear   |
                    +------+------+ 
                           |
                           v
                    +-------------+
Env / velocity ---> | VCA         |
                    +------+------+ 
                           |
                           v
                    +-------------+
                    | output / FX  |
                    +-------------+

          Shared analogue environment
          -----------------------------
          supply impedance
          thermal state
          leakage
          crosstalk
          noise
          component mismatch
```

The audible consequences include:

- small continuous pitch motion;
- voice-to-voice cutoff differences;
- different envelope times between voices;
- changing phase relationships between oscillators;
- slight harmonic asymmetry;
- resonance-dependent gain compression;
- frequency-dependent drive response;
- soft clipping before the final output;
- finite switch edge speed;
- capacitor memory;
- DC offsets transformed into even harmonics by nonlinear stages;
- power rail modulation under load;
- BBD clock/feedthrough noise in analogue chorus/delay systems.

A good model therefore stores and evolves **physical state**, not just “parameter values”.

---

# 2. Thermal and Electrical Constants

## 2.1 Thermal voltage

For a bipolar junction transistor:

```text
V_T = kT/q
```

At 25 °C:

```text
V_T ≈ 25.693 mV
```

A commonly sufficient DSP approximation is:

```cpp
constexpr float VT25 = 0.025693f;
```

For a high-accuracy temperature-aware model:

```cpp
float thermalVoltage(float temperatureK)
{
    constexpr double k = 1.380649e-23;
    constexpr double q = 1.602176634e-19;
    return float(k * temperatureK / q);
}
```

Typical room-temperature values are around 25.7–26 mV. Using 26 mV everywhere is acceptable for a fast approximation but not for a temperature-accurate transistor model.

## 2.2 Temperature

A useful reference point is:

```text
T0 = 298.15 K = 25 °C
```

An analogue model can represent temperature as:

```cpp
struct ThermalState
{
    float temperatureK = 298.15f;
    float targetK      = 298.15f;
    float timeConstant = 20.0f; // seconds; deliberately slow
};

void updateThermal(ThermalState& t, float dt)
{
    const float a = 1.0f - std::exp(-dt / t.timeConstant);
    t.temperatureK += a * (t.targetK - t.temperatureK);
}
```

For a synthesizer plugin, thermal state should normally evolve over **seconds to minutes**, not at audio rate with random modulation.

## 2.3 Semiconductor temperature coefficients

BJT base-emitter voltage varies strongly with temperature. A useful engineering approximation is roughly:

```text
ΔV_BE / ΔT ≈ -2 mV/°C
```

for a biased silicon BJT, although the exact coefficient depends on current and device construction.

This is important in:

- exponential VCO converters;
- transistor ladder filters;
- differential pairs;
- analog envelope threshold circuits;
- VCA current converters.

Do not use the above as a universal exact constant. It is a useful starting point for a phenomenological model.

---

# 3. Transistor Nonlinearity

## 3.1 Differential pair

A matched BJT differential pair has the fundamental relationship:

```text
I_diff = I_tail * tanh(V_diff / (2*V_T))
```

This is the origin of the extremely useful `tanh()` approximation in analogue filter modeling.

```cpp
inline float differentialPair(float vDiff, float tailCurrent, float temperatureK)
{
    const float VT = thermalVoltage(temperatureK);
    return tailCurrent * std::tanh(vDiff / (2.0f * VT));
}
```

This equation describes **current difference**, not an arbitrary audio soft clipper.

That distinction matters. The function belongs where a differential transistor pair exists.

## 3.2 Large-signal BJT model

For higher accuracy, use exponential BJT equations. A simplified forward-active model is:

```text
I_C = I_S * exp(V_BE / (N*V_T))
```

and a more complete Ebers–Moll model includes both junctions.

The practical problem in DSP is stiffness: exponential equations can overflow or create numerical problems. Clamp internal voltage arguments and use log-domain forms where appropriate.

```cpp
inline float safeExp(float x)
{
    return std::exp(std::clamp(x, -20.0f, 20.0f));
}

inline float bjtIc(float vbe, float is, float n, float VT)
{
    return is * safeExp(vbe / (n * VT));
}
```

This is useful for a high-fidelity VCO exponential converter or OTA input model.

## 3.3 Asymmetry

Real circuits are biased around a DC operating point. Consequently the nonlinear transfer is often not perfectly odd.

For a useful approximation:

```cpp
float asymmetricSaturation(float x, float drive,
                           float positiveBias,
                           float negativeBias)
{
    const float y = x + positiveBias;
    float z = std::tanh(y * drive);
    z -= 0.15f * std::tanh(negativeBias * drive);
    return z;
}
```

Do not use this unless the reference topology actually implies offset/asymmetry. A random DC offset everywhere is not a substitute for circuit modeling.

---

# 4. OTA Nonlinearity

Many vintage synths use operational transconductance amplifiers rather than transistor ladders.

Ideal OTA:

```text
I_out = g_m * V_diff
```

For an LM/CA-style differential input stage, the transconductance becomes nonlinear with signal amplitude.

A useful normalized approximation is:

```text
I_out = I_bias * tanh(V_diff/(2*V_T))
```

followed by output-current scaling.

A more complete current-cell model uses bias current and exponential transistor relationships.

```cpp
struct OTA
{
    float biasCurrent = 1.0e-3f;
    float temperatureK = 298.15f;

    float process(float vPlus, float vMinus) const
    {
        const float VT = thermalVoltage(temperatureK);
        return biasCurrent * std::tanh((vPlus - vMinus) / (2.0f * VT));
    }
};
```

The **CEM3320** is particularly useful as a reference OTA-filter device: its datasheet describes a four-pole VCF, exponential pole-frequency control, current-controlled gain cells and substantial temperature coefficients. For the 3320, the published typical pole-frequency control scale is about 60 mV/decade and the pole-frequency control tempco is specified around 3300 ppm/°C. The device also specifies non-ideal gain-cell behavior and a finite buffer slew rate. [CEM3320 datasheet]

These published values are more useful for a component-informed model than inventing generic “warmth” constants.

---

# 5. Capacitors: The Hidden Source of Memory

## 5.1 Ideal capacitor

```text
I = C * dV/dt
```

Discrete-time state:

```cpp
struct Capacitor
{
    float voltage = 0.0f;
    float C = 1e-6f;

    void integrateCurrent(float current, float dt)
    {
        voltage += current * dt / C;
    }
};
```

## 5.2 Equivalent series resistance

An ESR can be represented by adding:

```text
V = V_C + I * ESR
```

For most audio-frequency film capacitors used in vintage synth filters, ESR is usually a second-order effect compared with topology and capacitance value. For BBD circuits and large electrolytics it can matter more.

## 5.3 Dielectric absorption

Dielectric absorption can be approximated by one or more hidden capacitor states:

```text
main C
  |
  +---- C1 -- R1 ----+
  |                   |
  +---- C2 -- R2 ----+---- node
  |                   |
  +---- Cmain --------+
```

A simple software approximation:

```cpp
struct MemoryCap
{
    float main = 0.0f;
    float mem1 = 0.0f;
    float mem2 = 0.0f;

    float process(float input, float aMain,
                  float a1, float a2)
    {
        main += aMain * (input - main);
        mem1 += a1 * (main  - mem1);
        mem2 += a2 * (mem1  - mem2);
        return main + 0.02f * mem1 + 0.01f * mem2;
    }
};
```

The coefficients should be derived from actual capacitor technology if a specific component is being modeled.

## 5.4 Voltage-dependent capacitance

This is important for nonlinear ceramic capacitors and semiconductor junction capacitances, but should **not** be indiscriminately added to high-grade film capacitors.

A generic approximation is:

```text
C(V) = C0 * (1 + a1*V + a2*V^2)
```

rather than assuming all capacitors follow the same `C0*(1-alpha*V^2)` law.

---

# 6. Resistors and Passive Imperfections

For audio-frequency synthesis filters, ideal resistors are usually a sufficiently accurate approximation.

Important second-order effects include:

- tolerance;
- temperature coefficient;
- Johnson noise;
- voltage coefficient in some resistor technologies;
- parasitic capacitance at high impedance nodes.

Johnson noise density:

```text
v_n = sqrt(4*k*T*R*B)
```

where `B` is bandwidth.

In a real-time plugin, it is usually better to generate resistor noise through a calibrated broadband source at the equivalent node rather than instantiate an explicit noise generator for every physical resistor.

---

# 7. Potentiometers and Front-Panel Controls

The physical knob position is not necessarily the same as the audible parameter.

Recommended model:

```text
knob x
  |
  v
pot resistance/taper
  |
  v
control voltage
  |
  v
V-to-I converter / bias network
  |
  v
actual circuit parameter
```

Useful normalized tapers:

```cpp
float linearPot(float x) { return x; }
float logLikePot(float x) { return std::pow(x, 2.5f); }
float antiLogLikePot(float x) { return 1.0f - std::pow(1.0f - x, 2.5f); }
```

Use these only as *approximations*. If a service manual specifies a potentiometer resistance and topology, model the resistance network instead.

---

# 8. Component Tolerances and Voice Matching

Vintage polyphonic synths are particularly sensitive to voice-to-voice variation because every voice contains its own analogue oscillator/filter/VCA circuitry.

Typical manufacturing tolerances can be thought of in broad classes:

| Component | Useful nominal variation for a software “vintage” population model | Comment |
|---|---:|---|
| 1% resistor | ±1% | use as fixed manufacturing variation |
| 5% resistor | ±5% | common in older analogue circuitry |
| 10% capacitor | ±10% | often important in older RC timing circuits |
| 20% electrolytic | ±20% | use cautiously; many timing circuits are broad by design |
| transistor V_BE | device-specific | match or randomize around a calibrated mean |
| transistor beta | device-specific | often more important for gain/staging than ideal equations suggest |
| oscillator scale | calibrated per voice | important in VCO polyphony |
| filter frequency | calibrated per voice | often differs from oscillator spread |
| envelope time | calibrated per voice | can create subtle articulation differences |

These are **modeling ranges**, not a claim that every vintage synthesizer used those exact tolerance classes.

Recommended:

```cpp
struct VoiceVariation
{
    float oscScale;
    float oscOffset;
    float filterScale;
    float envTimeScale;
    float vcaScale;
    float nonlinearScale;
};
```

Generate these once for a voice/device identity and preserve them. Do not regenerate them every note.

---

# 9. Drift: Static Variation vs Dynamic Drift

These are different effects.

## Static mismatch

Voice 1 may always be slightly sharp relative to voice 2.

## Dynamic drift

Voice 1 itself slowly wanders over time.

A useful drift process is an Ornstein–Uhlenbeck-like low-frequency random walk:

```cpp
struct Drift
{
    float value = 0.0f;
    float velocity = 0.0f;

    float process(float dt, float target,
                  float tau, float noiseAmount,
                  std::mt19937& rng)
    {
        std::normal_distribution<float> n(0.0f, 1.0f);
        const float a = std::exp(-dt / tau);
        value = target + a * (value - target)
              + noiseAmount * std::sqrt(1.0f - a*a) * n(rng);
        return value;
    }
};
```

Typical audible pitch drift should normally be expressed as **cents or fractional scale**, not a fixed ±Hz offset, because the same thermal/current change produces larger absolute frequency excursions at higher frequencies.

Do not use independent white noise on oscillator pitch. It sounds like vibrato/noise rather than ageing analogue circuitry.

---

# 10. Power Supply Modeling

The power supply is shared by multiple modules, so load-dependent rail movement can couple voices.

A simple rail model is:

```text
Vrail(t) = Vnominal - R_supply * I_total(t)
```

with a decoupling capacitor:

```text
C_supply * dVrail/dt = I_regulator - I_load
```

```cpp
struct SupplyRail
{
    float voltage = 15.0f;
    float nominal = 15.0f;
    float sourceResistance = 0.5f;
    float capacitance = 4700e-6f;
    float regulatorStrength = 20.0f;

    void process(float loadCurrent, float dt)
    {
        const float regulatorCurrent =
            regulatorStrength * (nominal - voltage);

        voltage += dt * (regulatorCurrent - loadCurrent) / capacitance;
    }
};
```

This is deliberately simplified. In a high-accuracy model, reproduce the regulator, rectifier, filter capacitors and actual supply impedance from the service documentation.

Supply sag becomes most useful when the same rail feeds:

- oscillator bias;
- VCF bias;
- VCA current converters;
- envelope thresholds.

Then one loud chord can subtly alter the entire instrument.

Avoid exaggerated voltage sag. A plugin that audibly drops several semitones whenever a chord is played is not a realistic stock vintage synth.

---

# 11. Control-Voltage Crosstalk

Adjacent analogue traces and high-impedance nodes can couple through:

- PCB capacitance;
- shared resistors;
- supply impedance;
- transistor junction capacitance;
- switch charge injection.

A useful approximation is to inject a very small filtered cross-coupled signal:

```cpp
float crosstalk(float aggressor, float state,
                float amount, float dt,
                float tau)
{
    const float a = 1.0f - std::exp(-dt / tau);
    state += a * (aggressor - state);
    return amount * state;
}
```

Use this on control paths rather than globally adding oscillator bleed to the audio output.

---

# 12. Switching Transients

Analogue synthesizers contain many electronically controlled switches.

An ideal digital switch:

```text
0 → 1 instantly
```

is usually too clean.

Real switching can produce:

- charge injection;
- finite switching time;
- brief control feedthrough;
- residual DC movement;
- clicks at audio nodes.

A fast approximation is a shaped transition:

```cpp
float smoothSwitch(float state, bool target,
                   float dt, float tau)
{
    const float desired = target ? 1.0f : 0.0f;
    const float a = 1.0f - std::exp(-dt / tau);
    return state + a * (desired - state);
}
```

For authentic behavior, model the switch topology and capacitor charge injection.

---

# 13. Oscillator Modeling

## 13.1 Core architecture

A typical vintage VCO contains:

```text
1 V/oct CV
   ↓
exponential converter
   ↓
constant-current integrator
   ↓
saw ramp
   ↓
wave shaper(s)
   ↓
output buffer
```

For a resettable saw integrator:

```text
C * dV/dt = I
```

and at the reset threshold:

```text
V → V_reset
```

The finite reset time produces a small waveform imperfection which is normally absent from an ideal mathematical saw.

## 13.2 Exponential converter

An ideal pitch converter is:

```text
I = I0 * exp(Vcv / VT_oct)
```

where the practical circuit uses transistor ratios and resistors so that one volt changes frequency by one octave.

A numerically safe form:

```cpp
float vcoCurrent(float vOct, float i0)
{
    constexpr float ln2 = 0.69314718056f;
    return i0 * std::exp(vOct * ln2);
}
```

For a transistor-level model, include:

- BJT mismatch;
- V_BE temperature drift;
- resistor ratio error;
- reference current drift;
- finite beta;
- supply dependence.

## 13.3 Free-running phase

Unless the original architecture explicitly resets the oscillator on a gate, keep oscillator phase continuous.

```cpp
phase += frequency * dt;
phase -= std::floor(phase);
```

This is especially important for polysynths, where oscillator phases in multiple voices create evolving beating.

---

# 14. Band-Limited Oscillator Approximation

For real-time operation, use PolyBLEP, BLAMP, minBLEP or a band-limited analytic oscillator rather than a naive discontinuous waveform.

Example saw:

```cpp
float naiveSaw(float phase)
{
    return 2.0f * phase - 1.0f;
}

float polyBlep(float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }

    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }

    return 0.0f;
}

float polyBlepSaw(float phase, float dt)
{
    return naiveSaw(phase) - polyBlep(phase, dt);
}
```

For high-accuracy analogue modeling, oversampled continuous-time integration is often preferable because the goal is not simply “band-limited digital synthesis”; the internal nonlinear circuit can generate harmonics far above Nyquist and must then be anti-aliased by the oversampled simulation and final decimation.

---

# 15. VCO Imperfections

Important oscillator imperfections:

## 15.1 Pitch drift

Use slow correlated variation, not white noise.

## 15.2 Saw reset asymmetry

Model a finite reset interval if the original circuit has one.

## 15.3 Wave-shaper offsets

A waveshaper with a small DC offset can generate even harmonics after nonlinear processing.

## 15.4 Pulse-width mismatch

Pulse width is often not exactly the nominal switch point.

## 15.5 Control feedthrough

Fast CV changes can momentarily perturb amplitude or waveform shape.

## 15.6 Temperature

VCO exponential converters are particularly temperature-sensitive without compensation.

---

# 16. Pulse / Square Wave Modeling

An ideal square wave:

```cpp
float square(float phase, float duty)
{
    return phase < duty ? 1.0f : -1.0f;
}
```

is not sufficient for many analogue oscillators.

Real pulse outputs may have:

- finite rise/fall time;
- duty-cycle errors;
- amplitude dependence on frequency;
- waveform-dependent DC offset;
- switch feedthrough;
- unequal positive/negative slew.

For an approximation, use asymmetric edge smoothing.

For high accuracy, derive the waveform from the actual comparator/transistor/switch topology.

---

# 17. Envelope Generators

## 17.1 Exponential RC stages

A capacitor charging toward a supply follows:

```text
V(t) = V_target + (V_initial - V_target) * exp(-t/RC)
```

Therefore a digital envelope should not normally use a linear ramp unless the real circuit uses one.

```cpp
float onePoleTowards(float y, float target,
                     float tau, float dt)
{
    const float a = 1.0f - std::exp(-dt / tau);
    return y + a * (target - y);
}
```

## 17.2 Timing error

Vintage analogue envelope generators can have large timing errors compared with modern digitally calculated times because:

- capacitors have tolerance;
- switch resistance changes effective R;
- transistor thresholds vary;
- control current may be nonlinear;
- temperature changes leakage/current.

Preserve per-voice envelope time offsets in a polyphonic synth.

## 17.3 Gate and trigger are not always the same

Many analogue envelope generators distinguish:

- gate;
- trigger;
- retrigger;
- note-off/discharge.

The model must preserve these distinctions whenever the hardware does.

---

# 18. VCA Modeling

A generic linear multiplier:

```cpp
out = audio * gain;
```

is only the first approximation.

A vintage VCA can exhibit:

- finite control range;
- control feedthrough;
- residual gain at zero control;
- asymmetrical saturation;
- gain compression;
- DC offsets;
- logarithmic/exponential control scaling.

Useful approximation:

```cpp
float vcaApprox(float audio, float control,
                float drive, float asymmetry)
{
    const float g = std::max(0.0f, control);
    const float x = audio * g;
    return std::tanh((x + asymmetry) * drive) / drive;
}
```

For a transistor-pair or OTA VCA, model the control current first and derive gain from the device equations.

---

# 19. Filter Families

A vintage-synth modeling library should not contain one “analog filter”. At minimum implement separate reference models for:

1. Moog-style transistor ladder;
2. diode/transistor ladder;
3. OTA/CEM3320-style four-pole filter;
4. IR3109 / Roland OTA filter;
5. Yamaha IG00156 two-pole state-variable filter;
6. SSM2040 filter;
7. Sallen-Key / Korg-style filters;
8. generic continuous state-variable filter.

These topologies can share infrastructure but should not be reduced to one common transfer function.

---

# 20. Zero-Delay Feedback and TPT

Analog feedback is instantaneous within the continuous-time model. A naive DSP implementation introduces a one-sample delay.

```text
analog:
    y(t) = f(x(t), y(t))

digital naive:
    y[n] = f(x[n], y[n-1])
```

This creates phase error in the feedback path.

TPT/ZDF methods discretize integrators using trapezoidal integration and preserve topology more faithfully.

For a one-pole TPT integrator:

```cpp
struct TPTOnePole
{
    float z = 0.0f;
    float g = 0.0f;

    float process(float x)
    {
        const float v = (g * x + z) / (1.0f + g);
        z = 2.0f * v - z;
        return v;
    }
};
```

with:

```text
g = tan(pi * fc / fs)
```

for prewarped trapezoidal integration.

For nonlinear feedback:

```text
F(y) = 0
```

must be solved implicitly.

Newton iteration:

```cpp
template <typename F, typename DF>
float newton(float initial, F&& f, DF&& df)
{
    float y = initial;

    for (int i = 0; i < 5; ++i)
    {
        const float fy = f(y);
        const float d  = df(y);

        if (std::abs(d) < 1.0e-9f)
            break;

        const float step = fy / d;
        y -= step;

        if (std::abs(step) < 1.0e-6f)
            break;
    }

    return y;
}
```

Use the previous sample as the initial guess for good convergence.

**Biquad vs. pole count vs. ZDF/WDF:** "biquad" names an *implementation structure* (a 2-pole/2-zero direct-form difference equation), not a synonym for 2-pole filtering — biquads are cascaded to reach higher order. Biquads are a poor fit for resonant analog-modeled filters because they have no natural way to represent a saturating feedback path; that is why this section uses ZDF/TPT (trapezoidal-integrated one-pole stages, unconditionally stable) or direct ODE integration (e.g. RK2) instead of a cascaded-biquad design.

---

# 21. Newton vs Fixed-Point Iteration

## Newton-Raphson

Advantages:

- rapid convergence near the solution;
- appropriate for nonlinear ZDF filters;
- usually 2–5 iterations/sample when well conditioned.

Disadvantages:

- derivative required;
- can become unstable for poor initial guesses;
- vectorized multi-stage systems require care.

## Fixed point

```cpp
for (int i = 0; i < iterations; ++i)
    y = mix(y, evaluate(y), relaxation);
```

Advantages:

- simple;
- robust for smooth saturation;
- derivative-free.

Disadvantages:

- more iterations;
- can fail at high resonance or strong nonlinearity.

For a polyphonic synth, a practical implementation can use fixed iteration counts at normal settings and increase iterations only when resonance/drive becomes large.

---

# 22. Nonlinearity and Oversampling

A nonlinear function creates harmonics:

```text
x(t) → f(x) → harmonics above Nyquist → aliasing
```

Therefore nonlinear filters, VCAs, wave shapers and transistor models should be oversampled.

Recommended starting points:

| Use | Oversampling |
|---|---:|
| mild waveshaping | 2× |
| nonlinear OTA/VCF | 4× |
| resonant nonlinear ladder | 4–8× |
| hard sync + heavy saturation | 8–16× |
| offline reference rendering | 8–32× |

The original analogue circuit itself does not alias. Your digital approximation does.

---

# 23. Anti-Aliased Nonlinearity

A static waveshaper can be made less problematic with oversampling, but truly high-accuracy modeling may use:

- antiderivative antialiasing;
- ADAA;
- wave digital structures;
- oversampled state-space solving;
- band-limited impulse response methods.

For most analogue synth emulations, 8× oversampling plus good decimation is a strong practical baseline.

Do not use a very aggressive linear-phase decimator inside every per-voice processing path unless latency and CPU cost are acceptable. Minimum-phase/IIR half-band stages can be suitable for a real-time plugin; offline reference rendering can use steeper FIR decimation.

---

# 24. Moog Transistor Ladder Reference

## 24.1 Architecture

A classic Minimoog-style VCF consists of a nonlinear differential input stage feeding four coupled low-pass stages made from transistor junctions and capacitors, followed by a gain-recovery stage and resonance feedback.

The Minimoog Model D service documentation describes the ladder explicitly as a four-pole low-pass filter using transistor base-emitter junctions and capacitors; cutoff is controlled by ladder standing current, and the filter output is taken differentially across the final capacitor before gain recovery. The service manual specifies a nominal cutoff range of approximately 40 Hz to 20 kHz and a 24 dB/octave high-frequency slope. [Moog Minimoog Technical Service Manual]

## 24.2 Nonlinearity

Each differential pair can be approximated with `tanh`, while the ladder stages are coupled through nonlinear transistor junction behavior.

A simplified model:

```cpp
struct LadderStage
{
    float state = 0.0f;

    float process(float input, float alpha,
                  float drive)
    {
        const float nonlinear = std::tanh(input * drive);
        state += alpha * (nonlinear - state);
        return state;
    }
};
```

A high-accuracy model should not use four independent low-pass filters. The actual ladder has bidirectional loading and nonlinear currents between stages.

## 24.3 Resonance

Resonance is feedback from the filter output to the differential input stage.

High resonance changes the signal level entering the nonlinear ladder. Consequently resonance itself becomes nonlinear.

This naturally produces:

- passband gain changes;
- resonance compression;
- harmonic generation;
- bass loss;
- waveform-dependent resonance drive.

Do not add an arbitrary “warmth waveshaper” after a clean ladder to reproduce this behavior.

## 24.4 Fast approximation

A TPT 4-pole ladder with nonlinear stage saturation and a calibrated resonance feedback coefficient is a useful approximation.

## 24.5 High-accuracy

Use a coupled nonlinear state-space formulation and solve the feedback loop implicitly. Include:

- differential input pair;
- four nonlinear stages;
- gain recovery;
- resonance feedback path;
- actual RC values;
- output loading;
- transistor temperature state.

---

# 25. Yamaha CS-80 Reference

## 25.1 Why the CS-80 deserves its own model

The CS-80 is not simply a two-oscillator subtractive synth.

A CS-80 voice channel includes:

- a dedicated Yamaha VCO;
- waveform converter;
- resonant high-pass filter;
- resonant low-pass filter;
- unusual five-parameter filter envelope;
- separate VCA envelope;
- bypass sine path;
- extensive touch response;
- ring modulation and modulation facilities;
- very large numbers of per-voice analog control elements.

Documentation and hardware photography identify custom Yamaha devices including:

```text
IG00153  VCO core
IG00158  waveform converter
IG00156  VCF
M51621L  VCA-related circuitry in CS-family documentation
```

and the CS-80 uses separate high-pass and low-pass filtering per channel. The CS-80 voice cards contain dedicated VCO, waveform conversion, two VCF ICs, envelope generators and numerous VCAs. [Yamaha CS-80 references]

## 25.2 VCO

The IG00153 is a saw-core VCO and is described as **not internally temperature compensated**.

That is important: do not model the CS-80 VCO as a modern precision CEM3340-like oscillator.

A practical model should include:

```text
1 V/oct CV
    ↓
exponential/current converter
    ↓
saw core
    ↓
waveform converter
```

## 25.3 Waveform converter

The waveform converter converts the saw into:

- triangle;
- pulse;
- sine-like output;
- reverse-saw-related shapes depending on implementation.

The generated waveforms are not necessarily mathematically independent oscillators.

This is a critical modeling shortcut: derive the available waves from one oscillator state, then apply waveform-specific nonlinear shaping.

## 25.4 Filter topology

The CS-series IG00156 is a two-pole 12 dB/octave state-variable filter.

In the CS-80, one filter is used as a high-pass and another as a low-pass, with the two filters cascaded.

This produces:

```text
input
  ↓
12 dB resonant HPF
  ↓
12 dB resonant LPF
  ↓
output
```

The resulting overall response is therefore not just a single four-pole low-pass.

The IG00156 filter has a noteworthy **frequency-dependent Q** and its resonance is implemented by damping rather than a simple positive-feedback self-oscillation mechanism; this contributes to a smooth resonance that does not behave like a freely self-oscillating textbook filter. [Yamaha IG00156 analyses]

## 25.5 High-accuracy IG00156 approximation

A two-integrator TPT SVF is the appropriate structural approximation:

```text
highpass = input - lowpass - k*bandpass
bandpass += g*highpass
lowpass  += g*bandpass
```

but to emulate the CS-80 accurately, add the actual extra damping/control components of the IG00156 rather than using a generic SVF.

```cpp
struct CS80SVF
{
    float ic1eq = 0.0f;
    float ic2eq = 0.0f;
    float g = 0.0f;
    float k = 0.5f;

    void set(float fc, float q, float fs)
    {
        g = std::tan(float(M_PI) * fc / fs);
        k = 1.0f / std::max(q, 0.001f);
    }

    float processLP(float x)
    {
        const float hp = (x - k * ic1eq - ic2eq) / (1.0f + g * (g + k));
        const float bp = ic1eq + g * hp;
        const float lp = ic2eq + g * bp;
        ic1eq = 2.0f * bp - ic1eq;
        ic2eq = 2.0f * lp - ic2eq;
        return lp;
    }
};
```

For a high-accuracy model, replace `k` with the IG00156 damping/control equations and model OTA/gm nonlinearities internally.

## 25.6 CS-80 filter envelope

The filter envelope is unusual and should be explicitly represented as:

```text
IL  Initial Level
AL  Attack Level
A   Attack Time
D   Decay Time
R   Release Time
```

The envelope is offset around the static filter level rather than behaving like a conventional positive-only ADSR.

This allows the filter to move both above and below the static cutoff.

A generic implementation:

```cpp
float cs80Envelope(float t,
                   float initialLevel,
                   float attackLevel,
                   float sustainReference,
                   float attackTau,
                   float decayTau)
{
    if (t < 0.0f)
        return initialLevel;

    if (t < attackTau)
    {
        const float a = 1.0f - std::exp(-t / attackTau);
        return initialLevel + a * (attackLevel - initialLevel);
    }

    const float d = 1.0f - std::exp(-(t - attackTau) / decayTau);
    return attackLevel + d * (sustainReference - attackLevel);
}
```

The exact CS-80 circuit is more specific than this approximation.

## 25.7 Touch response

Initial Touch changes:

- VCA level;
- brilliance/filter behavior.

After Touch changes:

- level;
- brilliance;
- potentially modulation depending on routing.

For MIDI/MPE emulation, preserve this **per note/voice**. A global aftertouch signal loses the main expressive quality of the original architecture.

## 25.8 Sine path bypass

A CS-80 channel provides a sine-wave path that can bypass the VCF and mix separately into the VCA.

Do not filter the sine path just because the main oscillator path passes through the VCF.

## 25.9 CS-80 character summary

The strongest defining modeling targets are:

- voice-to-voice VCO differences;
- fast but nonlinear oscillator shaping;
- HPF + LPF cascade;
- smooth non-self-oscillating IG00156 resonance;
- unusual five-stage filter envelope;
- per-key touch response;
- analog ring modulation;
- separate sine level;
- extensive calibration/mismatch.

---

# 26. Prophet-5 Reference

## 26.1 Treat Prophet-5 revisions separately

The first three Prophet-5 revisions do not share the same filter technology.

### Rev1/Rev2

- SSM2030 VCOs;
- SSM2040 VCFs;
- SSM2050 envelopes;
- SSM2020 VCAs in documented revision architecture.

### Rev3

- CEM3340/CEM3345 oscillators;
- CEM3320 VCF;
- CEM3310 envelope generators;
- CA3280/related VCA implementation.

Sequential's modern Prophet-5 documentation explicitly identifies the Rev1/2 filter as the Dave Rossum-designed 2040 architecture and Rev3 as the Doug Curtis CEM3320 architecture. The modern instrument's “Vintage” control was added specifically to reproduce voice-to-voice variations in oscillators, filters and envelopes. [Sequential Prophet-5 documentation]

## 26.2 Prophet-5 oscillator behavior

The Prophet-5 uses two oscillators per voice.

The Rev3 architecture is based on CEM3340 VCOs, whose outputs include ramp, pulse and triangle functions and which support hard sync and voltage control. The original Prophet-5 service documentation describes 1 V/oct scaling and high-resolution per-oscillator sample-and-hold CVs used for tuning. [Prophet-5 technical manual]

A high-fidelity model should therefore include per-oscillator:

```text
pitch offset
pitch scale error
drift
phase
waveform amplitude
pulse width error
sync state
```

## 26.3 Poly Mod

Prophet-5 Poly Mod is a particularly important interaction:

- oscillator B can modulate oscillator A frequency;
- oscillator B can modulate oscillator A pulse width;
- filter envelope can modulate oscillator A frequency;
- filter envelope can participate in filter modulation depending on panel routing.

Do not implement Poly Mod as a clean post-oscillator pitch LFO. It operates in the oscillator/control path.

```cpp
float polyModPitch(float baseCV,
                   float oscB,
                   float filterEnv,
                   float oscBAmount,
                   float envAmount)
{
    return baseCV
         + oscB * oscBAmount
         + filterEnv * envAmount;
}
```

For high accuracy, route the modulating signals at their actual circuit nodes and preserve DC offsets and saturation.

## 26.4 SSM2040

The SSM2040 is a four-section voltage-controlled filter with exponential control over a very large frequency range. Its circuit differs substantially from later CEM3320 implementations.

The SSM2040 should therefore get its own model rather than be approximated by CEM3320 coefficients.

Useful targets:

- nonlinear transistor/current-cell behavior;
- 4-section filter topology;
- resonance feedback;
- filter input/output loading;
- device-to-device cutoff variation;
- temperature dependence.

## 26.5 CEM3320

The CEM3320 is a four-pole OTA-based VCF with internal variable gain cells and buffer stages.

Published device data indicate:

```text
pole control scale ≈ 60 mV/decade (typ.)
pole-frequency control tempco ≈ 3300 ppm/°C (typ.)
max gain of gain cell ≈ 3 (typ.)
```

These numbers are extremely useful when constructing a device-inspired model.

A first approximation is:

```cpp
float poleCurrent(float vControl,
                  float tempC,
                  float reference)
{
    constexpr float mVPerDecade = 60e-3f;
    const float tempCorrection =
        1.0f + 0.0033f * (tempC - 25.0f);

    return reference
         * std::pow(10.0f,
                    vControl / mVPerDecade)
         * tempCorrection;
}
```

This is a *teaching approximation*, not a complete CEM3320 equation.

## 26.6 Envelope voice spread

A Prophet-5 style model benefits enormously from static per-voice variation:

```text
Voice 1: slightly faster filter envelope
Voice 2: slightly slower oscillator B
Voice 3: slightly different VCF cutoff
...
```

Preserve identity across notes.

---

# 27. Minimoog Model D Reference

## 27.1 Architecture

The Model D has:

- three VCOs;
- mixer;
- noise generator;
- external input;
- four-pole Moog ladder VCF;
- two VCAs;
- two contour generators;
- modulation mixer;
- glide circuitry.

The service manual describes the VCF as a four-pole 24 dB/octave resonant low-pass with a nominal 40 Hz–20 kHz range and documents transistor ladder stages, VCA circuitry, noise generation and supply architecture. [Moog Minimoog Technical Service Manual]

## 27.2 Oscillators

The oscillator bank provides multiple waveshapes, including:

- triangle;
- triangle-saw hybrid;
- saw;
- square;
- pulse;
- narrow pulse.

These should not be implemented as six unrelated perfect mathematical waves. They result from analogue waveshaping and level selection.

The hybrid triangle/saw waveform is particularly important for brass-like patches.

## 27.3 Oscillator 3 as modulation source

Oscillator 3 can be switched into low-frequency operation and used as modulation.

This creates a subtle architecture difference: its control path and waveform behavior in low-frequency operation need not be identical to an independent modern LFO.

## 27.4 Mixer overload

The Minimoog mixer is a major nonlinear stage.

Multiple oscillators can be pushed into the mixer before the filter. Therefore:

```text
OSC1 + OSC2 + OSC3 + NOISE
             |
             v
       nonlinear mixer
             |
             v
            VCF
```

Do not normalize the summed oscillators immediately after mixing.

An analogue-style approximation:

```cpp
float miniMixer(float osc1, float osc2,
                float osc3, float noise,
                float headroom)
{
    const float x = osc1 + osc2 + osc3 + noise;
    return std::tanh(x / headroom) * headroom;
}
```

This stage should be calibrated to the desired mixer headroom rather than hard-coded to 0 dBFS.

## 27.5 Noise

The original machine uses a reverse-biased transistor noise source followed by filtering for white/pink/red-ish outputs.

For a high-accuracy model:

- generate a transistor-like broadband noise source;
- apply the documented filtering;
- preserve output level and loading.

A modern pseudo-random white-noise source followed by a pinking filter is a good approximation.

## 27.6 Contour generators

The Minimoog has analogue contour generators with approximately 10 ms to 10 s attack/decay ranges and a continuously variable sustain level according to the service documentation.

Use exponential RC-like shapes rather than linear ramps.

## 27.7 Glide

Model glide as a first-order lag in pitch voltage, not as a post-oscillator pitch interpolation.

---

# 28. Jupiter-8 Reference

## 28.1 Architecture

The Jupiter-8 combines:

- two VCOs per voice;
- waveform mixing;
- dedicated high-pass filtering;
- IR3109 four-stage OTA VCF;
- multi-mode 12/24 dB low-pass operation;
- envelope generators;
- BA662-style VCA circuitry;
- extensive cross-modulation/sync facilities.

The Roland IR3109 is a particularly important filter building block used throughout Roland's analogue era. Analysis of Roland filter implementations shows the IR3109 as four OTA stages, with the Jupiter-8 obtaining 12 dB or 24 dB low-pass output by selecting after the second or fourth OTA stage. [Roland filter topology reference]

## 28.2 IR3109 filter

The IR3109 provides four OTA-like filter sections but **does not include on-chip resonance control**. Resonance is therefore implemented externally.

This has an important modeling implication:

```text
IR3109 core
    + external feedback network
```

should be modeled, rather than pretending resonance is an internal “Q” control.

## 28.3 12 dB and 24 dB outputs

The Jupiter-8 can select:

```text
12 dB LPF = after stage 2
24 dB LPF = after stage 4
```

This is a topology distinction, not simply a choice between two independent filter algorithms.

## 28.4 HPF

The Jupiter-8 also has a separate high-pass filter path. Its interaction with the main VCF is important for thin brass, string and bass patches.

## 28.5 OTA nonlinear model

A practical IR3109 approximation is:

```cpp
struct OTAStage
{
    float z = 0.0f;

    float process(float x, float g,
                  float drive, float alpha)
    {
        const float i = std::tanh(x * drive);
        z += alpha * (g * i - z);
        return z;
    }
};
```

A higher-accuracy model uses the actual current-cell relationships and capacitor currents.

## 28.6 BA662 VCA

Roland frequently paired the IR3109 filter with BA662-based VCA/resonance/control circuitry. The Juno-60 analysis, for example, identifies the IR3109 filter plus BA662 resonance-control and voice-VCA stages; the Juno-106's 80017A packages the equivalent functional blocks into a hybrid assembly. [Roland 80017A / Juno-60 analysis]

For Jupiter-8 style VCA modeling, preserve:

- control-current law;
- residual gain;
- feedthrough;
- saturation;
- finite bandwidth.

## 28.7 Voice mismatch

Jupiter-style polyphony benefits from per-voice:

```text
VCO1 offset
VCO2 offset
VCO1 drift
VCO2 drift
filter scale error
VCA gain error
envelope timing error
```

Do not randomize these at every note.

---

# 29. Juno-60 Reference

## 29.1 DCO architecture

The Juno-60 uses a digitally controlled oscillator rather than a free-running VCO architecture.

This means the oscillator pitch is much more stable than a Prophet-5/Minimoog/CS-80 VCO, while the waveform generation and analogue output stages still provide analogue character.

A useful model is:

```text
master digital timing
       |
       v
DCO reset / divider
       |
       v
analogue ramp
       |
       +--> saw
       +--> pulse
       +--> sub oscillator
```

The Juno-60 therefore should **not** be given the same amount of free-running pitch drift as a vintage VCO.

Small analogue amplitude/waveshape/supply variations can remain.

## 29.2 Voice path

The Juno-60 uses an IR3109-based VCF and BA662 VCA circuitry. The voice filter/VCA topology is closely related to the later Roland 80017A implementation, but is discrete in the Juno-60 rather than encapsulated in the hybrid. [Juno-60 VCF/VCA analyses]

## 29.3 VCF

The filter is a four-pole resonant low-pass with external resonance behavior derived from the IR3109/BA662 circuitry.

There is also a dedicated high-pass filter before/around the main voice architecture.

## 29.4 Envelope

The Juno-60 uses a fast envelope architecture with panel ranges approximately:

```text
Attack: 1 ms – 3 s
Decay:  2 ms – 12 s
Release: 2 ms – 12 s
Sustain: 0–100%
```

These are panel specification ranges, not necessarily the exact capacitor RC time constants.

## 29.5 Chorus

The Juno-60 chorus is a major part of its sound and belongs in the reference model.

The chorus uses analogue BBD delay lines driven by a slow triangle LFO. The two stereo delay lines are modulated in opposite phase.

Published analyses of Juno-60 chorus behavior give useful approximate settings:

| Mode | Approx. modulation rate | Delay range |
|---|---:|---:|
| Chorus I | ≈0.5 Hz | ≈1.66–5.35 ms |
| Chorus II | ≈0.86 Hz | ≈1.66–5.35 ms |
| I+II / special mode | ≈9–10 Hz | ≈3.3–3.7 ms |

Service-note documentation is known to contain slightly different/possibly erroneous rate labels; treat the values above as modeling targets rather than immutable factory constants. [Juno-60 chorus analysis]

## 29.6 BBD modeling

A BBD is fundamentally a sampled analogue delay:

```text
input
  ↓
anti-alias LPF
  ↓
sample/hold cells
  ↓
clocked charge transfer
  ↓
output reconstruction LPF
```

Add:

- clock feedthrough;
- high-frequency attenuation;
- compander behavior if the original circuit has one;
- noise floor;
- nonlinear level dependence;
- clock-related modulation artifacts.

A high-accuracy BBD model can explicitly emulate the discrete delay cells at an internal clock rate.

A practical approximation is a modulated delay line plus steep filtering and calibrated noise.

```cpp
float modulatedDelayTime(float lfo,
                         float minDelay,
                         float maxDelay)
{
    const float u = 0.5f + 0.5f * lfo;
    return minDelay + u * (maxDelay - minDelay);
}
```

Do not use a perfectly transparent modern chorus algorithm if the goal is a Juno-60 identity.

---

# 30. TB-303 Reference

The TB-303 deserves its own section because it demonstrates how sequencer timing, oscillator waveform generation, filter nonlinearity, envelope state and accent memory can interact.

## 30.1 Core architecture

```text
sequencer
   |
6-bit-ish pitch/CV system
   |
slide / gate logic
   |
VCO
   |
waveform selector
   |
VCF input
   |
4-stage nonlinear diode/transistor ladder
   |
BA662-like VCA
   |
output
```

The TB-303 service documentation gives important calibration targets including 1 V/oct CV, a nominal A frequency of 110 Hz, and a VCO width/tuning procedure. [Roland TB-303 service manual]

## 30.2 Square waveform

Do not assume the square is a fixed 50% mathematical square. The waveform is derived from the oscillator/waveshaping circuitry and can have pitch-dependent duty-cycle behavior.

## 30.3 Filter

The TB-303 VCF is a four-stage diode/transistor ladder whose poles and loading differ materially from a buffered four-pole Moog ladder.

Do not substitute:

```text
ideal 24 dB low-pass + resonance
```

for the original topology.

The TB-303 is physically a 4-pole/24dB ladder, not a genuine 3-pole design — but one of its four stage capacitors is deliberately a different value from the other three (roughly half), which pulls the measured transfer function away from a clean 24dB/oct slope toward something that behaves closer to an 18dB filter. This is confirmed by Tim Stinchcombe's published transfer-function analysis of the TB-303 VCF, not merely an inference from the schematic. The practical modeling consequence: a relabeled *symmetric*, matched-capacitor 4-pole BJT ladder (e.g. a Minimoog-style ladder with a "TB-303" name on it) will not reproduce the "broken 24dB" character regardless of how its resonance/drive constants are tuned — the mismatch itself, not the pole count, is the character. A TB-303 model should expose per-stage capacitor scale as an explicit, named calibration parameter (see Section 42) rather than hard-coding one fixed mismatch ratio shared with other ladder-based synths.

## 30.4 Accent

Accent changes multiple control paths simultaneously and is not simply velocity gain.

The Accent Sweep circuit includes an RC memory path whose capacitor voltage persists between events.

This is a key reason consecutive accents can become progressively more pronounced.

## 30.5 Slide

Slide should be modeled as:

```text
continuous gate
+
first-order pitch CV glide
+
no new envelope trigger
```

rather than as ordinary MIDI portamento.

## 30.6 Implementation

Use:

```cpp
struct TB303State
{
    float phase = 0.0f;
    float pitchCv = 0.0f;
    float targetPitchCv = 0.0f;
    float meg = 0.0f;
    float veg = 0.0f;
    float accentCap = 0.0f;
};
```

and keep the state alive across note boundaries.

---

# 31. Additional Synth Family: Korg MS-20

The MS-20 is worth including because its two filter generations are unusually nonlinear and because it demonstrates why “Korg filter” is not one topology.

## 31.1 K35-era filter

The early MS-20 used a discrete filter topology commonly associated with the K35 circuit.

It is strongly nonlinear and has a characteristic aggressive resonance.

## 31.2 Later filter

Later MS-20 revisions changed filter implementation.

Therefore a family model should expose a revision switch rather than assuming one fixed response.

## 31.3 Modeling

A suitable high-accuracy model is a nonlinear Sallen-Key-like state-space network with the actual transistor/diode feedback path.

A generic fast model:

```cpp
float ms20Drive(float x, float drive)
{
    const float y = x * drive;
    return y / (1.0f + std::abs(y));
}
```

Then place the nonlinearity in the **feedback path**, not only after the final output.

The purpose is to reproduce the aggressive resonance growth and distortion associated with the topology.

---

# 32. Additional Synth Family: Oberheim SEM / OB Polyphonic Family

The Oberheim SEM is useful because it represents an analogue state-variable filter with multiple outputs and a very different resonance philosophy from the Moog ladder.

## 32.1 State-variable topology

The analog architecture is based on:

```text
high-pass summer
    ↓
integrator
    ↓
band-pass
    ↓
integrator
    ↓
low-pass
```

Feedback sets damping/Q.

The filter can provide:

- low-pass;
- band-pass;
- high-pass;
- notch.

## 32.2 Fast approximation

Use a Cytomic/Simper TPT SVF.

The two-integrator zero-delay formulation is well suited because it directly represents the analog topology and resolves the feedback analytically. [Cytomic technical papers]

## 32.3 Nonlinear approximation

Insert saturation at the OTA input/summing stages, not only at the final output.

```cpp
float sat(float x, float drive)
{
    return std::tanh(x * drive) / drive;
}
```

Then:

```cpp
hp = sat(input - k * bp - lp, drive);
bp = bp + g * hp;
lp = lp + g * bp;
```

A real circuit model should place nonlinear transfer functions at the actual OTA/buffer nodes.

---

# 33. Additional Synth Family: Roland Jupiter-6

The Jupiter-6 is valuable because it uses an IR3109 arranged as **two 12 dB state-variable-like sections** and can produce:

- 24 dB low-pass;
- 24 dB high-pass;
- 12 dB band-pass configurations.

This is a useful reminder that a single IC designation does not uniquely define a final filter topology; the surrounding routing matters.

For an agent architecture, define:

```text
Device model: IR3109
Topology configuration:
    JP8 LP12
    JP8 LP24
    JP6 LP24
    JP6 HP24
    JP6 BP12
```

rather than one monolithic “IR3109 filter” class.

---

# 34. Additional Synth Family: ARP Odyssey

The ARP Odyssey is useful because it combines:

- VCOs with analogue waveshaping;
- sample-and-hold modulation;
- multiple revisions;
- differing filter implementations between revisions;
- nonlinear mixer/filter behavior.

A revision-aware model is more appropriate than a single Odyssey filter curve.

The same framework used for Prophet-5 revision modeling should be used here:

```text
instrument family
      ↓
revision
      ↓
voice circuit
      ↓
component population
```

---

# 35. Additional Synth Family: Yamaha CS-60 / CS-50

These are useful companion references to the CS-80 because they share the Yamaha CS-series IG00153 / IG00156 / IG00158 family concepts while differing in polyphony, control and calibration.

They are especially useful when building reusable Yamaha component models.

Model the **device family** once, then instantiate the surrounding circuit differently.

---

# 36. Famous Synth Comparison Matrix

| Synth | Oscillator technology | Main filter topology | Main distinctive nonlinearity / imperfection |
|---|---|---|---|
| TB-303 | analogue VCO | nonlinear diode/transistor ladder | accent memory, unusual ladder loading, sequencer slide/gate |
| Minimoog Model D | 3 analogue VCOs | Moog transistor ladder | mixer drive, ladder saturation, oscillator drift |
| Prophet-5 Rev1/2 | SSM2030 | SSM2040 | voice mismatch, SSM filter character |
| Prophet-5 Rev3 | CEM3340 | CEM3320 | OTA filter nonlinearity, calibrated VCOs |
| Yamaha CS-80 | Yamaha custom VCO + waveform converter | IG00156 HPF + LPF | touch response, unusual envelope, smooth Q, voice-card variation |
| Jupiter-8 | analogue VCOs | IR3109 OTA stages | external resonance network, 12/24 dB topology |
| Juno-60 | DCO | IR3109 + BA662 | DCO stability, BBD chorus, shared voice architecture |
| Korg MS-20 | analogue VCOs | K35/variant Sallen-Key-like | aggressive nonlinear resonance |
| Oberheim SEM | analogue VCOs | state variable | smooth multimode response, nonlinear OTA stages |
| Jupiter-6 | CEM3340 / IR3109 family depending block | configurable IR3109 topology | multimode state-variable arrangements |
| ARP Odyssey | analogue VCOs | revision-dependent filters | strong revision-dependent filter character |
| ARP 2600 | analogue VCOs, patchable | 4012 (Moog-derived ladder) or 4072 (ARP redesign) | revision-dependent ladder; 4072 has a materially lower practical cutoff ceiling than 4012 |

---

# 37. Component Model Library

An implementation project should maintain reusable primitives.

Recommended modules:

```text
TransistorPair
BJT
Diode
OTA
VCA
ExponentialConverter
Integrator
TPTIntegrator
NonlinearIntegrator
RCNetwork
MemoryCapacitor
Potentiometer
AnalogSwitch
Comparator
SampleAndHold
VCOCore
WaveShaper
NoiseSource
SupplyRail
ThermalState
BBDDelay
AnalogMixer
```

These primitives can be connected into synth-specific models.

---

# 38. Recommended C++ Interfaces

```cpp
class AnalogBlock
{
public:
    virtual ~AnalogBlock() = default;
    virtual void reset() = 0;
    virtual void setSampleRate(float fs) = 0;
    virtual void setTemperature(float kelvin) = 0;
    virtual float process(float input) = 0;
};
```

For multi-node circuits:

```cpp
class NonlinearCircuit
{
public:
    virtual ~NonlinearCircuit() = default;

    virtual void reset() = 0;
    virtual void setSampleRate(float fs) = 0;
    virtual void updateControls() = 0;

    virtual void solve(float input,
                       float& output) = 0;
};
```

Prefer explicit state ownership so an agent can inspect/filter/calibrate every internal state.

---

# 39. High-Accuracy vs Fast Approximation Decision Rules

| Situation | Preferred approach |
|---|---|
| VCO waveform | oversampled analytic oscillator / circuit-integrated core |
| Exponential pitch converter | transistor-pair approximation or calibrated exp curve |
| Simple VCA | nonlinear multiplier + calibration |
| BA662-like VCA | current-controlled nonlinear model |
| CEM3320 | device-inspired OTA/state-space |
| IR3109 | four nonlinear OTA stages + external feedback |
| IG00156 | nonlinear 2-pole SVF/state-space |
| SSM2040 | dedicated topology model |
| Moog ladder | nonlinear coupled ladder ZDF |
| TB-303 VCF | nonlinear diode/transistor ladder + control-current model |
| Envelope | RC state variables |
| Juno chorus | modulated delay approximation; BBD model for high accuracy |
| Noise | calibrated colored source |
| Drift | slow correlated state |
| voice variation | fixed per-voice calibrated random state |
| power interaction | shared supply state |

---

# 40. What NOT to Do

Do not build a “vintage” synth by applying the following globally:

```text
random pitch LFO
+ white noise
+ tanh after filter
+ EQ bass bump
+ random detune each note
```

That creates an effect, not an analogue model.

Do not:

- reset oscillator phase unless the original circuit does;
- normalize every mixer stage;
- force all filters to 24 dB/octave;
- model resonance as Q alone;
- use one `tanh()` at the final output to represent all transistor distortion;
- randomize voice parameters on every note;
- add arbitrary DC offsets everywhere;
- give DCO instruments the same pitch drift as VCO instruments;
- treat aftertouch as a global mono modulation source when the original is per key;
- collapse different revisions of a synthesizer into one model;
- use a mathematically perfect chorus for a BBD-based instrument when the chorus is part of the instrument identity.

---

# 41. Calibration Without Reference Hardware

This project assumes no access to original hardware, so the model should be built around **published constraints plus internal consistency tests**.

For every synth/component, maintain three levels of confidence:

```text
A = published schematic/datasheet/service measurement
B = published technical analysis / independent circuit reconstruction
C = engineering approximation / inferred parameter
```

Example:

```text
CEM3320 pole-frequency scale: A
IR3109 four-OTA topology: B/A depending source
exact transistor leakage of one 1981 unit: C
```

Do not hide C-level assumptions inside “magic constants”. Put them in named calibration parameters.

---

# 42. Reference Parameter Sets

Every synth model should have a reference parameter block such as:

```cpp
struct VintageParameters
{
    // oscillator
    float oscScale = 1.0f;
    float oscOffset = 0.0f;
    float oscDriftCents = 0.05f;

    // filter
    float filterScale = 1.0f;
    float filterOffset = 0.0f;
    float filterDrive = 1.0f;
    float resonanceScale = 1.0f;

    // envelopes
    float attackScale = 1.0f;
    float decayScale = 1.0f;
    float releaseScale = 1.0f;

    // nonlinearities
    float transistorDrive = 1.0f;
    float vcaDrive = 1.0f;

    // environment
    float thermalSensitivity = 1.0f;
    float supplySensitivity = 1.0f;
};
```

This allows the same circuit model to be instantiated as:

```text
ideal
nominal vintage
hot vintage
cold vintage
individual device A
individual device B
```

without rewriting DSP.

---

# 43. Deterministic Vintage Identity

A very important requirement for polyphonic instruments is **repeatable imperfection**.

Use a deterministic seed for each voice/device:

```cpp
uint32_t makeVoiceSeed(uint32_t instrumentSeed,
                       uint32_t voiceIndex)
{
    uint32_t x = instrumentSeed ^ (voiceIndex * 0x9E3779B9u);
    x ^= x >> 16;
    x *= 0x85EBCA6Bu;
    x ^= x >> 13;
    x *= 0xC2B2AE35u;
    x ^= x >> 16;
    return x;
}
```

Generate voice tolerances once.

This produces a believable identity where voice 4 remains “that slightly sharp voice” rather than becoming a different instrument every note.

---

# 44. Warm-Up Model

Some vintage instruments change noticeably during warm-up.

A useful model is:

```text
power on
   ↓
thermal state starts below equilibrium
   ↓
transistor currents change
   ↓
VCO/filter operating points move
   ↓
slow convergence toward thermal equilibrium
```

For a synth plugin, model this as optional behavior because starting a DAW project should not always result in several minutes of pitch drift.

```cpp
float warmupAmount(float elapsed,
                   float timeConstant)
{
    return 1.0f - std::exp(-elapsed / timeConstant);
}
```

Apply warm-up only to parameters for which the circuit topology supports it.

---

# 45. Noise Modeling

Noise should be categorized:

## Thermal / Johnson noise

Broadband and dependent on resistance/temperature.

## Shot noise

Associated with semiconductor currents.

## Flicker noise

Strongest toward low frequencies.

## Supply noise

Correlated across modules.

## Clock noise

Important in BBD/DCO designs.

A high-quality plugin can combine:

```text
independent device noise
+
correlated supply noise
+
clock/feedthrough noise
```

rather than using one master white-noise generator.

---

# 46. Audio-Level Calibration Strategy

Choose an internal analogue reference.

For example:

```text
1.0 Vpeak internal analogue level
```

or:

```text
0 dBu reference
```

and preserve it through the modeled signal chain.

Do not equate analogue headroom directly with digital 0 dBFS.

A final calibration block can convert analogue-equivalent volts to plugin full scale:

```cpp
float voltsToFS(float volts)
{
    constexpr float reference = 1.0f;
    return volts / reference;
}
```

The internal reference must remain consistent across:

- oscillator outputs;
- mixer inputs;
- VCF drive;
- VCA input;
- output stage.

---

# 47. Testing Philosophy

Every module should have at least four test classes.

## Test A: Small signal linearity

Use tiny sine input.

Measure:

- gain;
- cutoff;
- pole placement;
- phase;
- resonance.

## Test B: Large signal nonlinearity

Sweep amplitude and measure:

- harmonic generation;
- compression;
- asymmetry;
- DC shift.

## Test C: Control interaction

Move two parameters simultaneously.

Examples:

```text
Cutoff + Resonance
Env Mod + Decay
Accent + Resonance
Pitch + Temperature
```

## Test D: Dynamic state

Use repeated notes and fast transitions.

Measure:

- capacitor memory;
- envelope retriggering;
- slide continuity;
- thermal behavior.

---

# 48. Spectral and Time-Domain Metrics

For each reference patch, save:

```text
waveform capture
FFT magnitude
FFT phase
RMS
crest factor
zero crossings
envelope trajectory
filter resonance frequency
filter decay trajectory
```

Useful objective measures include:

```text
spectral centroid
spectral tilt
THD
IMD
harmonic ratios
attack time
decay time
release time
pitch error in cents
voice-to-voice spread
```

Even without hardware, these tests let you ensure that a later parameter change did not destroy the intended circuit behavior.

---

# 49. Building an Agent-Ready Reference System

Each module description should contain these fields:

```text
MODULE
-------
Name
Instrument(s)
Revision(s)
Circuit topology
Primary components
Signal-domain role
Control-domain role
Known nonlinearities
Known imperfections
Nominal constants
High-accuracy equations
Fast approximation
State variables
Calibration parameters
Validation tests
Confidence level
Sources
```

Example:

```text
MODULE: IR3109 VCF
INSTRUMENTS: Jupiter-8, Juno-60, others
TOPOLOGY: four OTA stages
NONLINEARITY: OTA differential input / current cell
RESONANCE: external feedback network
STATE: four capacitor nodes + feedback state
FAST MODEL: four-pole TPT OTA ladder
HIGH MODEL: nonlinear current-cell state-space
CALIBRATION: pole scale, resonance gain, input gain
CONFIDENCE: A/B
```

This is much easier for a software agent to consume than prose alone.

---

# 50. Recommended Agent Task Boundaries

Do not ask an agent to “make a vintage filter”.

Instead give it:

```text
Implement CEM3320 nonlinear four-pole VCF.

Requirements:
- 48 kHz host rate
- 8x oversampling
- TPT/ZDF
- four internal states
- OTA/current-cell nonlinearity
- per-voice scale mismatch
- temperature input
- resonance feedback
- small-signal response must remain stable
- no final-stage magic saturation
- provide unit tests
```

Likewise:

```text
Implement CS-80 IG00156-style 2-pole SVF.

Requirements:
- independent HP/LP instances
- frequency-dependent Q
- non-self-oscillating resonance
- per-voice cutoff mismatch
- nonlinear OTA approximation
- five-stage filter envelope input
- TPT/ZDF structure
```

This produces much more consistent agent output.

---

# 51. Recommended Folder/Code Structure

```text
AnalogModel/
    Core/
        ThermalState.h
        SupplyRail.h
        AnalogSwitch.h
        NonlinearMath.h
        Oversampler.h
        Solver.h

    Devices/
        BJT.h
        Diode.h
        OTA.h
        VCA.h
        CEM3340.h
        CEM3320.h
        SSM2030.h
        SSM2040.h
        SSM2050.h
        IR3109.h
        BA662.h
        IG00153.h
        IG00156.h
        IG00158.h

    Filters/
        MoogLadder.h
        DiodeLadder.h
        CEM3320Filter.h
        IR3109Filter.h
        IG00156Filter.h
        SSM2040Filter.h
        SallenKeyFilter.h
        TPTSVF.h

    Effects/
        BBD.h
        JunoChorus.h

    Synths/
        TB303/
        Minimoog/
        Prophet5/
        CS80/
        Jupiter8/
        Juno60/
        MS20/
        SEM/
```

---

# 52. A Reusable Nonlinear One-Pole Primitive

A useful building block for many analogue filters is a one-pole state with nonlinear input current:

```cpp
class NonlinearOnePole
{
public:
    void setTimeConstant(float tau)
    {
        tau_ = std::max(tau, 1.0e-6f);
    }

    float process(float input, float dt,
                  float drive)
    {
        // Trapezoidal-like smooth state update.
        const float g = dt / (2.0f * tau_);
        const float n = std::tanh(input * drive);
        const float a = (1.0f - g) / (1.0f + g);
        const float b = g / (1.0f + g);

        state_ = a * state_ + b * (n + previousInput_);
        previousInput_ = n;
        return state_;
    }

    void reset(float value = 0.0f)
    {
        state_ = value;
        previousInput_ = value;
    }

private:
    float tau_ = 1.0e-3f;
    float state_ = 0.0f;
    float previousInput_ = 0.0f;
};
```

This is not an exact transistor stage, but it is preferable to a linear one-pole followed by global clipping because the nonlinearity is attached to the integrator's current flow.

---

# 53. Practical Component Models

## 53.1 Diode

Approximate:

```text
I = Is * (exp(V/(n*VT)) - 1)
```

Use a numerically safe implementation:

```cpp
float diodeCurrent(float v, float Is,
                   float n, float VT)
{
    const float x = std::clamp(v / (n * VT), -20.0f, 20.0f);
    return Is * (std::exp(x) - 1.0f);
}
```

## 53.2 Soft limiter

For approximate analogue gain-stage saturation:

```cpp
float softClip(float x, float drive)
{
    return std::tanh(x * drive) / drive;
}
```

## 53.3 Polynomial saturation

Use only where CPU requires it and after validating its harmonic spectrum:

```cpp
float fastSat(float x)
{
    x = std::clamp(x, -1.5f, 1.5f);
    return x * (1.0f - x*x / 6.75f);
}
```

Do not assume a polynomial has the same sound as `tanh` at high drive.

---

# 54. High-Frequency Behavior

Vintage synthesizer circuitry was not designed around today's 96/192 kHz digital assumptions.

Internal analogue stages have finite bandwidth.

Important mechanisms include:

- transistor junction capacitance;
- op-amp gain-bandwidth;
- OTA output capacitance;
- PCB capacitance;
- switch capacitance;
- RC compensation.

A high-accuracy model can add a small high-frequency pole to gain stages.

Example:

```cpp
float hfPole(float x, float& state,
             float fc, float fs)
{
    const float a = 1.0f -
        std::exp(-2.0f * float(M_PI) * fc / fs);

    state += a * (x - state);
    return state;
}
```

Do not add the same arbitrary 14 kHz low-pass to every oscillator. The high-frequency behavior is topology-specific.

---

# 55. Low-Frequency Coupling

Many analogue audio paths are AC-coupled.

A simple coupling capacitor and resistor give:

```text
fc = 1 / (2*pi*R*C)
```

Example:

```cpp
float couplingCutoff(float R, float C)
{
    return 1.0f / (2.0f * float(M_PI) * R * C);
}
```

This can create:

- bass rolloff;
- transient tilt;
- phase shift;
- resonance bass loss;
- DC rejection.

If the original circuit has a coupling capacitor before or inside the feedback path, model it there. Moving it to the final output does not produce the same behavior.

---

# 56. Resonance Modeling Rules

For any resonant filter ask:

1. Where is the feedback taken from?
2. What node receives it?
3. Is the feedback voltage- or current-controlled?
4. Is there a coupling capacitor in the feedback loop?
5. Is the feedback path nonlinear?
6. Does resonance feed back before or after gain recovery?
7. Does the resonance control itself have multiple gangs or stages?
8. Does high resonance change passband gain?
9. Can the original circuit self-oscillate?
10. Is the resonance peak frequency exactly the nominal cutoff?

These questions are more important than whether the filter is labeled “18 dB”, “24 dB”, or “12 dB”.

---

# 57. Polyphonic Voice Allocation and Analog Character

The synth-level voice manager should preserve analogue identity.

Recommended per-voice lifetime:

```text
voice object created
    ↓
component mismatch generated once
    ↓
thermal state initialized
    ↓
oscillator/filter/envelope state retained
    ↓
notes retrigger state only where hardware retriggers
```

When voice stealing occurs, do not necessarily reset every analogue state. Depending on the modeled instrument, the hardware may reuse a voice circuit with residual capacitor/oscillator state.

For the closest digital reproduction, make this selectable:

```text
Voice reset policy:
    clean restart
    analog continuation
```

---

# 58. Unison Character

Unison should expose existing voice mismatches rather than replacing them with a global detune formula.

If each voice has:

```text
+2.1 cents
-1.3 cents
+0.4 cents
-2.7 cents
```

and slightly different cutoff/gain/phase, unison will naturally sound wider and less static.

A separate tiny slow drift can be added on top.

---

# 59. Stereo Imaging

Vintage monophonic voice circuits are usually fundamentally mono.

For polyphonic instruments, stereo width can naturally arise from:

- voice panning;
- chorus;
- separate output paths;
- component variation.

Do not put a modern stereo widening algorithm after every vintage synth model when the hardware itself was not stereo.

For the Juno-60 chorus, however, stereo is part of the instrument's expected behavior.

---

# 60. Sample-and-Hold Modeling

Vintage sample-and-hold circuits use:

- noise source;
- switch;
- holding capacitor;
- leakage path;
- clock/gate timing.

A realistic model includes finite droop:

```cpp
float updateHold(float hold, float sampled,
                 float dt, float droopTau,
                 bool sample)
{
    if (sample)
        hold = sampled;

    return hold * std::exp(-dt / droopTau);
}
```

In reality, sample and hold can be affected by clock feedthrough and charge injection.

These effects are important in instruments where S/H directly modulates pitch or filter cutoff.

---

# 61. Ring Modulation

A balanced analogue ring modulator can be modeled as multiplication:

```cpp
float ringMod(float a, float b)
{
    return a * b;
}
```

but a real ring modulator has:

- carrier feedthrough;
- incomplete switching;
- diode nonlinearity;
- DC offsets;
- limited bandwidth.

For a fast approximation:

```cpp
float ringModImperfect(float a, float b,
                       float feedthrough)
{
    return a * b + feedthrough * a;
}
```

Use a nonlinear switching model for a high-accuracy instrument reproduction.

The CS-80 is a particularly interesting case because the ring modulator is part of a larger expressive signal architecture rather than a generic plugin insert.

---

# 62. BBD Delay High-Accuracy Model

A BBD chain with `N` stages and clock `fclk` has an approximate delay:

```text
T_delay ≈ N / (2*fclk)
```

because each stage transfers charge on alternating clock phases.

A real delay line includes clock feedthrough and bandwidth limitations.

A component-informed model can use two-phase transfer:

```cpp
struct BBD
{
    std::vector<float> cells;
    int index = 0;

    void processClock(float input)
    {
        float v = input;

        for (float& cell : cells)
        {
            const float next = cell;
            cell = v;
            v = next;
        }
    }
};
```

At audio-plugin sample rates, a complete cell-by-cell BBD may be expensive, but an 8× or 16× internal clock-domain model can be used in an offline/high-quality mode.

---

# 63. Juno-60 Chorus Fast Approximation

The minimum credible Juno-style chorus approximation is:

```text
mono signal
   |
   +---- modulated delay ----> L
   |
   +---- inverted-LFO delay -> R
```

with BBD-like filtering and noise.

```cpp
struct StereoChorus
{
    float lfoPhase = 0.0f;

    void process(float input,
                 float& left,
                 float& right,
                 float fs)
    {
        const float rate = 0.513f;
        lfoPhase += rate / fs;
        lfoPhase -= std::floor(lfoPhase);

        const float lfoL = std::sin(2.0f * float(M_PI) * lfoPhase);
        const float lfoR = -lfoL;

        // Replace with real interpolated delay lines.
        left = input;
        right = input;
        (void) lfoL;
        (void) lfoR;
    }
};
```

The code shows only control structure. A real implementation needs interpolated delay buffers and reconstruction filtering.

---

# 64. Reference Constants Cheat Sheet

```text
Thermal voltage @25°C:                  ≈25.7 mV
Typical BJT dVBE/dT:                    ≈-2 mV/°C
24 ppqn sequencer clock:                24 pulses/quarter
16th note at 24 ppqn:                   6 clock pulses
1 V/octave system:                       83.333 mV/semitone
Moog ladder asymptotic slope:            24 dB/oct
SSM/CEM four-pole class:                 nominally 24 dB/oct
Two-pole SVF:                            12 dB/oct
RC cutoff:                               1/(2πRC)
BBD delay:                               ≈N/(2*fclock)
TPT prewarp:                             g=tan(π*fc/fs)
BJT thermal voltage:                     kT/q
Diff pair:                               I*tanh(Vdiff/(2VT))
```

These constants are reference equations, not universal synth-specific calibrations.

---

# 65. Vintage Character Presets

A useful product design is to expose internal “character modes” that scale the physical imperfections.

## Clean analytical

```text
component mismatch: 0
thermal drift: 0
supply coupling: 0
noise: minimal
nonlinearity: circuit only
```

## Nominal vintage

```text
component mismatch: realistic
thermal drift: low
supply coupling: low
noise: calibrated
nonlinearity: realistic
```

## Hot vintage

```text
component mismatch: realistic
thermal drift: medium
supply coupling: medium
filter drive: slightly higher
nonlinearities: more obvious
```

## Worn/aged

```text
capacitor tolerance: larger
leakage: larger
voice mismatch: larger
switching artifacts: larger
noise: larger
```

Avoid making “hot vintage” mean “more random”. It should mean stronger versions of *specific physical effects*.

---

# 66. Model Validation When No Hardware Is Available

Without a physical reference, validation becomes a consistency problem.

Use:

1. service-manual calibration points;
2. device datasheets;
3. independent circuit analyses;
4. SPICE where available;
5. published scope/spectrum measurements;
6. known historical schematics;
7. cross-instrument component comparisons.

The model should first reproduce the documented circuit in small signal, then add nonlinearities, then add imperfections.

Do not tune the system by ear alone.

---

# 67. SPICE as an Offline Oracle

Even if the final plugin does not run SPICE, a transistor-level circuit can be simulated offline.

Recommended workflow:

```text
schematic
   ↓
SPICE model
   ↓
impulse/sine/step tests
   ↓
export reference curves
   ↓
fit lower-cost DSP model
   ↓
compare DSP vs SPICE
```

This is especially valuable for:

- transistor filters;
- CEM/SSM approximations;
- nonlinear VCAs;
- CS-80 filter stages;
- TB-303 filter stages.

An agent can use exported SPICE curves as a calibration oracle even though the final plugin never runs SPICE.

---

# 68. Curve Fitting Strategy

When converting a high-accuracy model to a fast approximation, fit:

```text
control → parameter
input level → nonlinear gain
resonance → feedback gain
temperature → drift coefficient
```

using splines or monotonic piecewise polynomials.

Example:

```cpp
class CalibrationCurve
{
public:
    float process(float x) const
    {
        if (x <= 0.0f) return y0;
        if (x >= 1.0f) return y1;

        const float p = x * float(values.size() - 1);
        const int i = int(p);
        const float f = p - float(i);

        return values[i] * (1.0f - f)
             + values[i + 1] * f;
    }

    float y0 = 0.0f;
    float y1 = 1.0f;
    std::vector<float> values;
};
```

This is preferable to burying arbitrary exponential constants in code.

---

# 69. Numerical Stability

Circuit-inspired filters can become unstable near self-oscillation.

Required safeguards:

- finite internal state bounds;
- Newton iteration limit;
- fallback iteration method;
- smooth parameter interpolation;
- NaN/Inf detection in debug builds;
- safe exponential evaluation;
- denormal handling;
- oversampled processing before nonlinear feedback.

Example:

```cpp
inline float safeTanh(float x)
{
    if (x > 10.0f)  return 1.0f;
    if (x < -10.0f) return -1.0f;
    return std::tanh(x);
}
```

Do not silently clamp an unstable filter into a generic saturated output. Log the condition in development builds.

---

# 70. Parameter Smoothing

Parameter smoothing must respect the physical topology.

For a cutoff control entering a transistor exponential converter, smoothing frequency in Hz is not necessarily equivalent to smoothing control voltage.

Prefer:

```text
UI cutoff
    ↓
physical control voltage smoothing
    ↓
exponential current converter
```

rather than:

```text
UI cutoff
    ↓
convert to Hz
    ↓
linear one-pole smoothing
```

The same principle applies to resonance, VCA control and envelope depth.

---

# 71. Parameter Modulation Sample Accuracy

For fast modulation sources:

- update analogue-equivalent CV at audio rate;
- evaluate nonlinear control conversion at the internal oversampled rate when required;
- avoid block-rate parameter changes.

This is particularly important for:

- audio-rate oscillator cross-modulation;
- FM;
- PWM;
- high-rate filter modulation;
- ring modulation.

---

# 72. Modulation Matrix Modeling

In a vintage instrument, modulation often occurs at a specific physical node.

For example:

```text
LFO → oscillator pitch converter
```

is not necessarily equivalent to:

```text
LFO → oscillator output phase
```

Likewise:

```text
filter envelope → filter control current
```

is not necessarily equivalent to:

```text
envelope → cutoff Hz
```

Always define the **destination domain**:

```text
pitch voltage
frequency current
filter bias
VCA control current
pulse width control
audio input
```

This is one of the most important rules for agent-generated synth modules.

---

# 73. Synth-Specific Priority Matrix

## TB-303

Highest priority:

```text
VCF topology
Accent Sweep
slide/gate timing
MEG/VEG interaction
oscillator waveshape
resonance feedback
```

## CS-80

Highest priority:

```text
IG00156 filter behavior
5-stage filter envelope
touch response
voice mismatch
waveform converter
sine bypass path
```

## Prophet-5

Highest priority:

```text
revision-specific filter
CEM/SSM oscillator behavior
Poly Mod
voice-to-voice variation
envelope timing spread
```

## Minimoog

Highest priority:

```text
mixer drive
Moog ladder
oscillator waveshapes
oscillator interaction
contour generator
```

## Jupiter-8

Highest priority:

```text
IR3109 topology
external resonance
12/24 dB selection
VCO interactions
HPF
voice mismatch
```

## Juno-60

Highest priority:

```text
DCO timing
IR3109 + BA662 VCF/VCA
HPF
chorus/BBD
voice-level mismatch
```

## ARP 2600

Highest priority:

```text
4012 vs 4072 VCF identity (selectable, not a shared curve)
ladder tanh nonlinearity
4072 cutoff-ceiling compression
patch-matrix flexibility (VCF/VCA/envelopes independently patchable)
monophonic per-voice accuracy budget
```

---

# 74. Final Design Rules

1. **Topology beats labels.** A “24 dB filter” tells you less than its actual circuit.
2. **State beats magic constants.** Capacitors, oscillator phase, thermal state and voice mismatch should persist.
3. **Current-domain modulation beats arbitrary Hz-domain modulation** when the real circuit uses exponential current conversion.
4. **Nonlinearity belongs where the device is nonlinear.** Do not move all distortion to the output.
5. **Revision is part of the instrument identity.** Prophet-5 Rev1/2 and Rev3 are materially different analog machines.
6. **DCO and VCO are different error models.** Do not give them identical drift.
7. **A polyphonic synth should have a population of voices, not one perfect voice copied eight times.**
8. **Analogue memory matters.** Envelope capacitors and accent/hold circuits can influence future notes.
9. **Feedback needs ZDF/TPT or an equivalent implicit treatment.** One-sample feedback delay changes high-frequency resonance.
10. **Oversampling is mandatory around nonlinear feedback when fidelity matters.**
11. **Use published service/datasheet data as anchors.** Put inferred values in explicit calibration tables.
12. **Validate small signal before large signal.** First match the linear circuit; then add nonlinearities; then add ageing and drift.
13. **Use deterministic imperfections.** Hardware identities should persist.
14. **Do not confuse imperfection with randomness.** Real analogue errors are correlated, stateful and topology-dependent.
15. **Give every module a confidence level and source trail.**

---

# 75. Primary References and Further Reading

## Hardware / service references

- Roland TB-303 Bass Line Service Manual / Service Notes — VCO, VCF, VCA, CV, calibration and sequencer details.
- Moog Music, *Minimoog Model D Technical Service Manual* — oscillator, mixer, VCF ladder, VCA, contour, power supply and calibration information.
- Sequential Circuits, *Prophet-5 Technical Manual / Service Manual* — oscillator, filter, envelope, control matrix, DAC/S&H and revision-specific circuitry.
- Sequential, *Prophet-5 User Guide* — historical distinction between Rev1/2 SSM2040 and Rev3 CEM3320 filters and documented “Vintage” voice variation concept.
- Yamaha, *CS-80 Service Manual / Servicing Guide* — custom ICs, voice-card architecture and calibration.
- Yamaha CS-80 owner documentation — filter envelope, touch response and voice architecture.
- Roland, *Juno-60 Service Notes* — DCO, VCF/VCA, chorus and service/calibration information.
- Tim Stinchcombe, published transfer-function analysis of the Roland TB-303 VCF — source for the 4-pole-with-mismatched-capacitor characterization in Section 30.3.
- ARP 2600 technical/circuit histories covering the 4012 (Moog-derived) and 4072 (ARP redesign, post patent-dispute) VCF boards — source for Section 77.

## Device references

- CEM3340 datasheet — VCO architecture and operating characteristics.
- CEM3320 datasheet — four-pole VCF, exponential pole control and gain-cell specifications.
- SSM2030 datasheet — VCO device.
- SSM2040 datasheet — four-section voltage-controlled filter.
- SSM2050 datasheet — voltage-controlled transient/envelope generator.
- Roland IR3109 analyses and reverse-engineered filter schematics.
- Roland BA662/BA6110 family information.
- Yamaha IG00153 / IG00156 / IG00158 analyses and reconstructed schematics.

## DSP / analog modeling references

- Andrew Simper / Cytomic technical papers on trapezoidal integration, TPT state-variable filters, nonlinear analog modeling and solving continuous-time analog structures.
- Vadim Zavalishin, *The Art of VA Filter Design*.
- Antti Huovilainen, nonlinear digital ladder filter research.
- Circuit-level SPICE analyses of vintage synthesizer filters where available.

---

# 76. Source / Confidence Notes for the Major Featured Synths

| Topic | Confidence | Reason |
|---|---|---|
| TB-303 service/calibration architecture | A | Roland service documentation |
| Minimoog oscillator/filter/service behavior | A | Moog service documentation |
| Prophet-5 revision distinction | A | Sequential documentation + service manual |
| CEM3320 published electrical behavior | A | device datasheet |
| CS-80 IG00156 two-pole architecture | A/B | service information + independent circuit reconstruction |
| CS-80 custom IC functional allocation | A/B | service/hardware documentation |
| Jupiter-8 IR3109 12/24 dB architecture | A/B | service/topology analysis |
| Juno-60 IR3109/BA662 relationship | B | schematic/teardown analysis |
| Juno-60 chorus exact rate labels | B/C | service notes and independent reverse engineering can disagree |
| TB-303 mismatched-capacitor ladder (4-pole, not 3-pole) | A/B | confirmed by Stinchcombe's independent transfer-function analysis |
| ARP 2600 4012 vs 4072 VCF distinction and patent-dispute origin | B | well-documented technical/historical circuit analysis, not a directly sourced ARP service manual in this document |
| ARP 2600 4072 practical cutoff ceiling (~7-12 kHz) | B/C | repeatedly cited in independent comparisons; treat as a calibration target pending SPICE/service-manual cross-check |
| Exact component drift of an arbitrary vintage unit | C | requires unit-specific measurement |
| Exact dielectric absorption of every original capacitor | C | requires component-specific data |

When implementing an unmeasured characteristic, expose it as a calibration parameter rather than treating the guessed value as historical fact.

---

# 77. ARP 2600 Reference

## 77.1 Why the ARP 2600 needs its own model

The ARP 2600 is semi-modular: three VCOs, a VCF, a VCA, ADSR/AR envelopes, a ring modulator, sample-and-hold, a spring reverb and a patch-panel, normalized into a default signal path but fully repatchable. It is also strictly monophonic, so — unlike Jupiter-8, Juno-60 or CS-80 — the per-voice CPU budget is not a polyphony-driven constraint, and the highest-accuracy solver tier is affordable in real time.

## 77.2 Filter revisions: 4012 vs 4072

Two materially different VCF boards were used across ARP 2600 production:

- **"4012" board:** an early design, essentially a licensed/derived version of the Moog transistor-ladder topology, predating ARP's own patent workaround. 4-pole/24dB. Widely regarded as the more musical of the two, and shares the same tanh-derived differential-pair character as the Minimoog ladder.
- **"4072" board:** ARP's own redesign, forced by Moog's ladder-filter patent dispute with ARP. Still nominally 4-pole/24dB, but built around a different circuit rather than a licensed Moog ladder, with a materially lower practical cutoff ceiling — usable resonant sweeps top out around **~7–12 kHz** rather than opening up to a bright ~18–20 kHz the way the 4012 or a Minimoog ladder does. It sounds duller/darker at the same nominal cutoff-knob position.

This is the same pattern as Prophet-5 Rev1/2 vs Rev3: "ARP 2600 filter" is not one circuit, and a model must expose 4012 vs 4072 as a selectable instrument identity rather than one shared curve with a brightness trim.

## 77.3 Modeling implications

- Both revisions are BJT differential-pair ladders, so `tanh` is the physically correct nonlinearity for both (same reasoning as Minimoog/Jupiter-8) — the audible 4012-vs-4072 difference is in bias/gain-staging and pole placement, not in swapping the saturator family.
- The 4072's lower ceiling should come from a genuine topology-driven frequency-range limitation (lower max control current, different pole scaling), not from a static "duller" low-pass trim bolted onto a shared 4012 model — the real darkening/compression changes with resonance and drive, not only with the cutoff knob.
- Because the instrument is monophonic, `TanhSaturator` + a higher-iteration Newton solver (or a WDF ladder) is affordable even for a real-time voice — there is no polyphony tax the way there is for Jupiter-8/CS-80.

## 77.4 Signal path

The voice-relevant signal path, using the default/normalized (pre-patched) routing:

```text
VCO1/2/3 --> Mixer --> VCF (4012 or 4072 ladder) --> VCA --> Output
                 ^                 ^
      Ring Mod, S&H     ADSR / AR envelopes, LFO
```

Unlike a fixed-architecture instrument (Minimoog, Jupiter-8), the 2600's patch matrix means this is a default normalization, not a hardwired circuit. An emulation should keep the VCF/VCA/envelope blocks independently patchable rather than assuming the default routing is the only one.

## 77.5 Confidence

- 4-pole/24dB spec for both 4012 and 4072: **A** (widely published spec).
- 4012 as a Moog-derived ladder, and the 4072 redesign forced by the patent dispute: **B** (well-documented technical/historical circuit analysis, not a directly quoted ARP service manual in this document).
- 4072's ~7–12 kHz practical ceiling: **B/C** — repeatedly cited in independent technical comparisons; treat the exact numeric range as a calibration target, not a hardware-verified constant, until cross-checked against a service manual or SPICE reconstruction.

---

# 78. Summary: The Reference Model Philosophy

The objective is **not** to produce a synthesizer that has random imperfections.

The objective is to produce a simulated analogue instrument in which imperfections have causes.

The ideal architecture is:

```text
                 ┌────────────────────────┐
                 │   Physical Parameters   │
                 │ R C transistor OTA VBE │
                 └────────────┬───────────┘
                              │
                              v
                 ┌────────────────────────┐
                 │   Circuit Topology     │
                 │ integrators feedback   │
                 │ switches current paths │
                 └────────────┬───────────┘
                              │
                              v
                 ┌────────────────────────┐
                 │ Continuous State Model │
                 │ caps currents phase   │
                 │ thermal supply memory │
                 └────────────┬───────────┘
                              │
                    TPT / ZDF / solver
                              │
                              v
                 ┌────────────────────────┐
                 │      Nonlinearity      │
                 │ BJT diode OTA VCA     │
                 └────────────┬───────────┘
                              │
                          oversample
                              │
                              v
                 ┌────────────────────────┐
                 │      Audio output      │
                 └────────────────────────┘
```

For fast real-time models, preserve the same architecture conceptually and replace expensive circuit blocks with calibrated equivalents.

The result should remain physically coherent when controls interact.

That is the central requirement for a convincing vintage analogue emulation:

> **A good vintage model does not merely sound imperfect. It behaves imperfectly in the same places, for the same reasons, and with the same memory and interaction as the original circuit family.**

