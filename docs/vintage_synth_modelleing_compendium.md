# Vintage Synth Modeling Compendium: Analog Imperfections & Non-Linearities

This compendium serves as an implementation guide for Virtual Analog (VA) digital signal processing. It details the physical mechanisms, component tolerances, and circuit non-linearities that define the "vintage warmth" of analog synthesizers, alongside strategies for modeling them—ranging from fast phenomenological approximations to high-accuracy Zero-Delay Feedback (ZDF) circuit simulations.

---

## 1. Physical Foundations of Analog Warmth

Analog warmth is not a single process; it is a composite effect of multiple physical and electrical interactions occurring simultaneously across the signal path.

### 1.1 Differential Transistor Saturation ($\tanh$)
Differential transistor pairs (used in ladder filters, gain stages, and VCAs) exhibit a non-linear voltage-to-current transfer characteristic governed by the hyperbolic tangent function:

$$I_{out} = I_{tail} \cdot \tanh\left(\frac{V_{in}}{2 V_T}\right)$$

Where $V_T$ is the thermal voltage ($\approx 26\text{ mV}$ at room temperature).

* **Acoustic Character:** Introduces smooth, odd-harmonic saturation (3rd, 5th, 7th harmonics). Softly rounds peak amplitudes, acting as a natural soft limiter that prevents harsh digital clipping.
* **Dynamic Response:** As signal amplitude increases, the effective gain drops, broadening the resonant peak and compressing the dynamic range of the filter.

### 1.2 Thermal Drift and Voltage Instability
Analog components (resistors, capacitors, matched transistor arrays) are sensitive to ambient temperature and local heat dissipation.

* **Oscillator Drift:** Thermal fluctuations cause tiny continuous shifts in control voltages ($V/\text{Oct}$), resulting in frequency drift ($\Delta f \approx 0.1\text{ Hz} \text{ to } 2\text{ Hz}$).
* **Phase Detuning:** Multiple free-running analog oscillators never maintain static phase alignment. Their phase relationships continuously drift, causing dynamic cancellation and reinforcement across the harmonic spectrum (natural phase chorus/beating).

### 1.3 Capacitor Imperfections
Ideal capacitors store energy linearly ($Q = CV$). Physical capacitors introduce multiple non-ideal behaviors:

* **Voltage Coefficient of Capacitance ($C(V)$):** In ceramic (X7R) and semiconductor junctions, effective capacitance varies with instantaneous voltage across the terminals:
  $$C(V) = C_0 \cdot (1 - \alpha V^2)$$
  This causes frequency modulation at signal speed, producing subtle even and odd harmonics.
* **Dielectric Absorption ("Capacitor Memory"):** Dipoles in the dielectric material fail to discharge instantly. When discharged, residual voltage slowly leaks back onto the nodes, causing subtle phase smearing and slew rounding on fast transients.
* **Equivalent Series Resistance (ESR) & Loss Tangnet ($\tan \delta$):** Parasitic resistance inside the capacitor attenuates high-frequency energy, dampening sharp resonances and rounding high-frequency edge transitions.

### 1.4 Component Tolerances and Asymmetry
Factory component tolerances ($\pm 1\% \text{ to } \pm 10\%$) prevent identical stages within ladder networks or polyphonic voices from matching perfectly.

* **Asymmetrical Pole Placement:** A theoretical 4-pole filter assumes identical cutoff frequencies for all four $RC$ stages. Hardware variance skews each stage's cutoff frequency ($f_1 \neq f_2 \neq f_3 \neq f_4$), warping the phase response and softening the ideal 24 dB/octave slope into an asymmetric transition band.
* **DC Offsets:** Microscopic transistor imbalances inject small internal DC offsets into signal paths, causing asymmetric $\tanh$ saturation (generating even harmonics alongside odd harmonics).

### 1.5 Power Supply Sag and Inter-Block Crosstalk
Analog synths share a single power bus across all modules.

* **Supply Sag:** Heavy low-frequency draw (e.g., loud sub-bass notes or fast envelope bursts) temporarily pulls down the rail voltage, causing transient pitch drops, VCA gain dips, and envelope speed changes.
* **Control Voltage Bleed / Crosstalk:** High-frequency oscillator signals or sharp gate pulses leak into neighboring traces via capacitive coupling, adding subtle tonal texture and sub-audible artifacts.

---

## 2. Zero-Delay Feedback (ZDF) & Topology-Preserving Transforms (TPT)

### 2.1 The Digital Feedback Problem (1-Sample Delay)
In continuous-time analog circuits, feedback acts instantaneously. Direct discretization using standard delay lines ($z^{-1}$) introduces a one-sample delay into the feedback loop:

$$y[n] = f(x[n], y[n-1])$$

This 1-sample delay introduces an uncontrolled frequency-dependent phase shift:

$$\Delta \phi = -\omega T_s$$

* **Consequences:** At high cutoff frequencies, the added phase shift destroys the intended pole locations, causing resonance frequency errors, amplitude damping, and severe high-frequency instability.

### 2.2 Bilinear Transform and TPT Integrators
To eliminate the 1-sample delay, continuous-time integrators $s^{-1}$ are mapped to discrete time using the **Bilinear Transform** (Trapezoidal Integration), yielding a **Topology-Preserving Transform (TPT)** structure:

$$H_{TPT}(z) = \frac{g}{1 + g} \cdot \frac{1 + z^{-1}}{1 - \left(\frac{1 - g}{1 + g}\right)z^{-1}}$$

Where the digital tuning factor $g$ is pre-warped to match the analog cutoff frequency $f_c$:

$$g = \tan\left(\frac{\pi f_c}{f_s}\right)$$

An individual TPT 1-pole low-pass integrator node is computed as:

$$v[n] = \frac{g \cdot x[n] + s[n-1]}{1 + g}$$
$$s[n] = 2 v[n] - s[n-1]$$

Where $s[n-1]$ is the state memory of the integrator capacitor.

### 2.3 Resolving Zero-Delay Feedback Loops

#### Linear Case (Direct Matrix Substitution)
When all loop components are linear, the implicit feedback equation $y[n] = f(x[n], y[n])$ is solved algebraically by expressing the instantaneous output as a linear function of current input $x[n]$ and historical state memory $\mathbf{s}[n-1]$.

For a generic 1st-order loop:

$$y[n] = G \cdot x[n] + S$$

Where $G$ represents the instantaneous gain through the open-loop path and $S$ represents the combined historical state response. The implicit loop can then be inverted explicitly without iteration.

#### Non-Linear Case (Newton-Raphson & Fixed-Point Iteration)
When non-linearities ($\tanh$ saturation) exist within the feedback loop, the system forms an implicit non-linear equation:

$$f(y) = y - g \cdot \tanh(x - K \cdot y) - s = 0$$

* **Newton-Raphson Solver:** Solves $f(y) = 0$ iteratively until convergence:
  $$y_{k+1} = y_k - \frac{f(y_k)}{f'(y_k)}$$
* **Table-Based / Polynomial Solvers:** High-speed implementations approximate the inverse non-linear feedback curve using pre-computed 2D lookup tables or 3rd/5th-order polynomial inversions to avoid per-sample iterative loops.

---

## 3. High-Accuracy vs. Fast-Approximation Implementation Strategies

| Feature / Aspect | High-Accuracy Model (Circuit-Level) | Fast Approximation (Real-Time / Polyphonic) |
| :--- | :--- | :--- |
| **Integrator Topology** | ZDF / TPT discrete state-space representation. | Direct Form II Transposed Biquads or basic SVF. |
| **Feedback Loop** | Implicit non-linear solver (Newton-Raphson or vector solver per sample). | Explicit feedback with 1-sample delay ($z^{-1}$) + phase equalizer compensation. |
| **Saturation Engine** | Exact $\tanh(x)$ or transistor Ebers-Moll equation applied at every internal node. | Fast polynomial approximation: $f(x) \approx x - \frac{x^3}{3}$ or static lookup table (LUT). |
| **Oversampling** | $4\times$ to $16\times$ oversampling with linear-phase polyphase FIR decimation. | $1\times$ (native rate) or $2\times$ oversampling with low-order IIR half-band filters. |
| **Component Variation** | Dynamic node-level variable tolerances, heat modeling, and supply-voltage tracking. | Per-voice static randomized offsets (e.g., $\pm 2\%$ cutoff/resonance spread per voice). |
| **CPU Target** | Monophonic synths, master bus processors, offline rendering. | High-polyphony synths (32+ voices), mobile DSP, real-time live performance. |

---

## 4. Core Components: Detailed Specifications & Modeling Notes

### 4.1 Filters

#### 4.1.1 4-Pole Transistor Ladder Filter (e.g., Minimoog Style)
* **Topology:** Four cascaded 1-pole low-pass sections with a single global feedback path.
* **Non-Linear Points:** Differential input pair ($\tanh$), 4 ladder stage transistor pairs ($\tanh$), feedback gain stage.
* **Key Imperfection:** As resonance increases, passband gain drops by up to 12 dB due to feedback-induced saturation clamping the lower frequencies (the classic "Moog bass loss").
* **Accurate Modeling:** 4-stage ZDF structure with $\tanh$ clipping embedded at every stage output and resolved simultaneously in the loop.
* **Fast Approximation:** SVF or dual-biquad cascade with a manual bass-boost compensation curve linked to the resonance control parameter:
  $$\text{Gain}_{\text{comp}} = 1.0 + \alpha \cdot K_{\text{res}}$$

#### 4.1.2 Diode Ladder Filter (e.g., TB-303 Style)
* **Topology:** Four cascaded $RC$ sections using dynamic diode impedances, featuring asymmetric capacitor values ($C_1 = 33\text{nF}$, $C_2 = 22\text{nF}$, $C_3 = 10\text{nF}$, $C_4 = 1\mu\text{F}$).
* **Non-Linear Points:** Diode dynamic resistance changes with total current; high-pass feedback filter network ($C_{13}/R_{23}$) strips sub-bass prior to feedback injection.
* **Key Imperfection:** Passband behaves as 18 dB/octave; high-pass feedback creates a characteristic squelchy, liquid resonance peak with a pronounced bass dip.
* **Accurate Modeling:** Matrix-based ZDF TPT solver accounting for asymmetric stage capacitors and the embedded high-pass feedback filter pole.
* **Fast Approximation:** 3-pole SVF variant with a dedicated high-pass filter embedded in the feedback path.

#### 4.1.3 State-Variable Filter (e.g., Oberheim SEM / Jupiter-8 Style)
* **Topology:** Simultaneous Low-Pass, High-Pass, Band-Pass, and Notch outputs derived from two continuous integrators.
* **Non-Linear Points:** Integrator input buffers and feedback damping amplifiers.
* **Key Imperfection:** Smooth, highly stable resonance behavior; soft overdrive character when pushed without extreme bass loss.
* **Accurate Modeling:** Andrew Simper / Cytomic TPT SVF topology with non-linear saturation on the band-pass feedback loop.

#### 4.1.4 Korg MS-20 Sallen-Key Filter *(Placeholder)*
* **Topology:** Second-order Sallen-Key architecture (K35 chip in early models, OTA in later models).
* **Non-Linear Points:** Heavy diode-clipping feedback path.
* **Key Imperfection:** Aggressive, screamy, self-oscillating distortion.

#### 4.1.5 OTA-Based Ladder Filter (e.g., Prophet-5 / IR3109 Style) *(Placeholder)*
* **Topology:** Four operational transconductance amplifier (OTA) stages.
* **Non-Linear Points:** Transconductance non-linearity ($I_{out} = g_m \cdot V_{in}$).

---

### 4.2 Oscillators

#### 4.2.1 Voltage-Controlled Oscillator (VCO)
* **Core Mechanics:** Integrator charges a capacitor to generate a raw ramp wave; a high-speed comparator flushes the capacitor to reset the wave.
* **Non-Linear Points:** Integrator discharge time is non-zero (creates a tiny parasitic falling-edge slope on saw waves); comparator threshold voltage drifts thermally.
* **Accurate Modeling:** Band-limited impulse train (BLIT) or PolyBLEP anti-aliasing with dynamic period modulation, sub-sample reset timing, and low-frequency noise drift added to pitch control inputs.
* **Fast Approximation:** Standard PolyBLEP saw/square oscillators driven by a shared per-voice drift LFO:
  $$f_{\text{inst}} = f_0 \cdot \left(1 + \text{Noise}_{\text{pink}}[n] \cdot 0.001\right)$$

#### 4.2.2 Digitally-Controlled Analog Oscillator (DCO)
* **Core Mechanics:** Digital clock resets an analog integrator charging ramp.
* **Key Imperfection:** Pitch is crystal-locked (zero pitch drift), but the amplitude and waveshape are defined by physical analog charging curves.
* **Accurate Modeling:** Perfectly stable digital timing combined with analog waveshaper non-linearities and supply voltage ripple.

#### 4.2.3 Master-Slave Hard Sync & PWM Artifacts *(Placeholder)*
* **Core Mechanics:** Resetting slave phase on master cycle completion; pulse-width modulation limits.

---

### 4.3 VCAs, Envelopes & Utility Modules

#### 4.3.1 Analog VCAs (Differential Pair & OTA-based)
* **Non-Linear Points:** Control voltage port saturation; exponential current converters.
* **Key Imperfection:** Asymmetric control voltage bleed (envelope signal leaks directly into the audio output as a low-frequency click/thup on fast attack times).

#### 4.3.2 Analog Envelope Generators (RC Charge/Discharge)
* **Core Mechanics:** Analog envelopes (like the Minimoog or System-100m) use physical $RC$ charging networks rather than linear digital ramps.
* **Curve Characteristics:** Natural exponential attack, decay, and release curves ($1 - e^{-t/RC}$ and $e^{-t/RC}$).
* **Accurate Modeling:** Discrete 1-pole recursive filters for envelope stages rather than linear accumulators.

#### 4.3.3 BBD Delay & Chorus Lines *(Placeholder)*
* **Core Mechanics:** Bucket-brigade device analog shift registers.
* **Imperfections:** Clock noise, high-frequency attenuation, compander non-linearities.

---

## 5. Anti-Aliasing Strategies for Non-Linear DSP

Any non-linear function $f(x)$ (such as $\tanh(x)$ or polynomial clipping) expands the signal spectrum. Harmonics that exceed the Nyquist frequency ($f_s / 2$) fold back into the audible spectrum as unharmonic **aliasing distortion**.

```
Input Spectrum ---> [ Non-Linearity f(x) ] ---> Expanded Spectrum ---> [ Oversampling Decimation Filter ] ---> Clean Output
```

### 5.1 Oversampling Frameworks
To accommodate expanded harmonic bandwidth, the non-linear processing block is run at an elevated sampling rate ($M \cdot f_s$):

1. **Upsampling:** Insert $M-1$ zeros between input samples and process through a low-pass interpolation filter.
2. **Non-Linear Processing:** Execute the non-linear filter/oscillator code at $M \cdot f_s$.
3. **Downsampling:** Filter the output with a steep low-pass decimation filter (cutoff at $f_s / 2$) and drop $M-1$ out of $M$ samples.

* **Recommended Oversampling Factors:**
  * Mild Saturation / Gentle Warmth: $2\times$
  * Heavy Filter Resonance Saturation: $4\times$ to $8\times$
  * Aggressive Wavefolding / Hard Sync: $8\times$ to $16\times$

### 5.2 Antialiased Saturation Functions

#### Exact Hyperbolic Tangent ($\tanh$)
```csharp
// High-accuracy tanh saturation with drive control
public static float SatuateTanh(float input, float drive)
{
    float x = input * drive;
    return MathF.Tanh(x);
}
```

Fast 3rd-Order Polynomial Approximation
Avoids expensive transcendent calls (MathF.Tanh) for high-polyphony contexts:
```
// Fast 3rd-order polynomial soft clipper (Input clamped to [-1.5, 1.5])
public static float FastSoftClip(float x)
{
    if (x <= -1.5f) return -1.0f;
    if (x >= 1.5f) return 1.0f;
    return x * (1.0f - (x * x) / 6.75f);
}
```

Pade Approximation of TanhProvides closer adherence to $\tanh(x)$ than low-order polynomials with minimal CPU overhead:
```
// Pade (3,2) approximation of tanh(x)
public static float PadeTanh(float x)
{
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
```