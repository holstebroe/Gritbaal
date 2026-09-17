#ifndef GRITBAAL_FILTER_HPP
#define GRITBAAL_FILTER_HPP

#include "MathConstants.hpp"
#include <cmath>
#include <algorithm>
#include <array>

namespace gritbaal {

// ---------------------------------------------------------------------------
// Saturator policies (the nonlinearity axis).
//
// Only two families are needed: a symmetric tanh curve, the correct closed
// form for a matched BJT differential pair (Minimoog, ARP 2600 ladders), and
// an asymmetric curve approximating diode-clipper asymmetry (TB-303, MS-20).
// See docs/vintage_synth_modelleing_compendium.md section 3 and the "Key
// pattern" note in docs/filter-architecture-plan.md section 3.
// ---------------------------------------------------------------------------

struct TanhSaturator {
    static inline float value(float x) { return std::tanh(x); }
    static inline float deriv(float x) {
        float t = std::tanh(x);
        return 1.0f - t * t;
    }
};

// Cheap fix for diode-clipper asymmetry: positive half uses unity tanh,
// negative half uses a shallower/scaled tanh. Not a true Shockley-diode
// solve (see docs/filter-architecture-plan.md section 3), but it captures
// the qualitative asymmetry a symmetric tanh cannot.
struct AsymmetricSaturator {
    static constexpr float kNegSlope = 0.7f;
    static constexpr float kNegScale = 1.3f;

    static inline float value(float x) {
        if (x >= 0.0f) {
            return std::tanh(x);
        }
        return kNegScale * std::tanh(kNegSlope * x);
    }

    static inline float deriv(float x) {
        if (x >= 0.0f) {
            float t = std::tanh(x);
            return 1.0f - t * t;
        }
        float t = std::tanh(kNegSlope * x);
        return kNegScale * kNegSlope * (1.0f - t * t);
    }
};

// ---------------------------------------------------------------------------
// Solver policies (the iteration-quality axis). Both solve the same implicit
// feedback equation:  y + G * Saturator::value(k * y) - f0 == 0
//
// FixedPointSolver is the performance-tier building block: cheap, no
// derivative, no convergence check. Not wired into any shipped preset today
// -- every current target is monophonic, so the accuracy tier is affordable
// (see docs/filter-architecture-plan.md section 3). Kept ready for future
// CPU-constrained (polyphonic) presets.
//
// NewtonSolver is the accuracy-tier building block and is what every
// shipped preset uses.
// ---------------------------------------------------------------------------

template <typename Saturator, int Iterations>
struct FixedPointSolver {
    static inline float solve(float f0, float G, float k, float initialGuess) {
        float y = initialGuess;
        for (int i = 0; i < Iterations; ++i) {
            y = f0 - G * Saturator::value(k * y);
        }
        return y;
    }
};

template <typename Saturator, int Iterations>
struct NewtonSolver {
    static inline float solve(float f0, float G, float k, float initialGuess) {
        float y = initialGuess;
        for (int i = 0; i < Iterations; ++i) {
            float sat = Saturator::value(k * y);
            float fVal = y + G * sat - f0;
            float fDev = 1.0f + G * k * Saturator::deriv(k * y);
            float step = fVal / fDev;
            y -= step;
            if (std::abs(step) < 1.0e-6f) {
                break;
            }
        }
        return y;
    }
};

// Single TPT 1-pole Low-Pass Stage with capacitor memory state s[n]
class TPTOnePole {
public:
    TPTOnePole() = default;

    void reset() { s_ = 0.0f; }

    inline float process(float x, float g) {
        float v = (g * x + s_) / (1.0f + g);
        s_ = 2.0f * v - s_;
        return v;
    }

    float getState() const { return s_; }
    void setState(float s) { s_ = s; }

private:
    float s_{0.0f};
};

// High-Pass Filter for Feedback Loop
class HPFFeedback {
public:
    HPFFeedback() = default;

    void setSampleRate(double sampleRate) {
        sampleRate_ = sampleRate;
        setCutoff(150.0f);
        reset();
    }

    void setCutoff(float cutoffHz) {
        float w0 = kTWO_PI_F * cutoffHz;
        alpha_ = 1.0f / (1.0f + w0 / (2.0f * static_cast<float>(sampleRate_)));
    }

    void reset() {
        x1_ = 0.0f;
        y1_ = 0.0f;
    }

    inline float process(float x) {
        float y = alpha_ * (y1_ + x - x1_);
        x1_ = x;
        y1_ = y;
        return y;
    }

    struct State {
        float x1;
        float y1;
    };

    State getState() const { return {x1_, y1_}; }
    void setState(const State& state) {
        x1_ = state.x1;
        y1_ = state.y1;
    }

private:
    double sampleRate_{44100.0};
    float alpha_{0.98f};
    float x1_{0.0f};
    float y1_{0.0f};
};

// ---------------------------------------------------------------------------
// Topology: 4-pole transistor/diode ladder with a high-pass-filtered
// resonance feedback path (DC-blocked feedback, matching the Moog-style
// ladder). Saturator + Solver are injected; per-stage capacitor scale and
// resonance gain are runtime configuration rather than template parameters,
// so a single instantiation can serve multiple synths that share a
// saturator family (e.g. Minimoog and ARP 2600 "4012" both use
// LadderFilter<TanhSaturator, ...>, distinguished only by their capacitor/
// gain configuration -- see docs/filter-architecture-plan.md section 3-4).
// ---------------------------------------------------------------------------

template <typename Saturator, typename Solver>
class LadderFilter {
public:
    void reset() {
        stage1_.reset();
        stage2_.reset();
        stage3_.reset();
        stage4_.reset();
        hpfFeedback_.reset();
    }

    void setSampleRate(double oversampledRate) {
        oversampledRate_ = oversampledRate;
        hpfFeedback_.setSampleRate(oversampledRate_);
    }

    // {1,1,1,1} for a matched ladder (Minimoog, ARP 2600); a mismatched
    // pattern for TB-303 -- the mismatch, not the pole count, is what gives
    // the TB-303 its "broken 24dB" character (compendium section 30.3).
    void setCapScale(float c1, float c2, float c3, float c4) {
        capScale1_ = c1;
        capScale2_ = c2;
        capScale3_ = c3;
        capScale4_ = c4;
    }

    void setResonanceGainScale(float scale) { resGainScale_ = scale; }

    float process(float input, float cutoffHz, float resonance) {
        float nyquist = static_cast<float>(oversampledRate_ * 0.5);
        float maxCutoff = std::min(18000.0f, 0.49f * nyquist);
        float totalCutoffHz = std::clamp(cutoffHz, 20.0f, maxCutoff);
        float resNorm = std::clamp(resonance, 0.0f, 1.0f);

        float hpfCutoff = 150.0f + 100.0f * resNorm;
        hpfFeedback_.setCutoff(hpfCutoff);
        float resGain = resNorm * resGainScale_;

        float wc = kTWO_PI_F * totalCutoffHz;
        float gBase = std::tan(wc / (2.0f * static_cast<float>(oversampledRate_)));

        float g1 = gBase * capScale1_;
        float g2 = gBase * capScale2_;
        float g3 = gBase * capScale3_;
        float g4 = gBase * capScale4_;

        float A1 = g1 / (1.0f + g1);
        float A2 = g2 / (1.0f + g2);
        float A3 = g3 / (1.0f + g3);
        float A4 = g4 / (1.0f + g4);

        float B1 = stage1_.getState() / (1.0f + g1);
        float B2 = stage2_.getState() / (1.0f + g2);
        float B3 = stage3_.getState() / (1.0f + g3);
        float B4 = stage4_.getState() / (1.0f + g4);

        float G_ladder = A4 * A3 * A2 * A1;
        float S_ladder = A4 * A3 * A2 * B1 + A4 * A3 * B2 + A4 * B3 + B4;

        // HPF feedback linear relationship: hpFb = alpha * (y1_prev + v4 - x1_prev)
        HPFFeedback::State hpfState = hpfFeedback_.getState();
        float hpfAlpha = 1.0f / (1.0f + (kTWO_PI_F * hpfCutoff) / (2.0f * static_cast<float>(oversampledRate_)));
        float hpLinear = hpfAlpha * (hpfState.y1 - hpfState.x1);

        float G_hp = hpfAlpha * G_ladder;
        float S_hp = hpfAlpha * S_ladder + hpLinear;

        float f0 = G_hp * input + S_hp;
        float hpFb = Solver::solve(f0, G_hp, resGain, hpfState.y1);

        float satFb = Saturator::value(resGain * hpFb);
        float x1 = input - satFb;

        float y1 = stage1_.process(x1, g1);
        float y2 = stage2_.process(y1, g2);
        float y3 = stage3_.process(y2, g3);
        float y4 = stage4_.process(y3, g4);

        hpfFeedback_.process(y4);

        return y4;
    }

private:
    double oversampledRate_{176400.0};

    TPTOnePole stage1_;
    TPTOnePole stage2_;
    TPTOnePole stage3_;
    TPTOnePole stage4_;
    HPFFeedback hpfFeedback_;

    float capScale1_{1.0f};
    float capScale2_{1.0f};
    float capScale3_{1.0f};
    float capScale4_{1.0f};
    float resGainScale_{16.5f};
};

// ---------------------------------------------------------------------------
// Topology: 2-pole Sallen-Key diode-feedback filter (Korg MS-20 style).
// ---------------------------------------------------------------------------

template <typename Saturator, typename Solver>
class SallenKeyFilter {
public:
    void reset() {
        s1_ = 0.0f;
        s2_ = 0.0f;
    }

    void setSampleRate(double oversampledRate) { oversampledRate_ = oversampledRate; }
    void setResonanceGainScale(float scale) { resGainScale_ = scale; }

    float process(float input, float cutoffHz, float resonance) {
        float nyquist = static_cast<float>(oversampledRate_ * 0.5);
        float maxCutoff = std::min(18000.0f, 0.49f * nyquist);
        float totalCutoffHz = std::clamp(cutoffHz, 20.0f, maxCutoff);
        float resNorm = std::clamp(resonance, 0.0f, 1.0f);

        float wc = kTWO_PI_F * totalCutoffHz;
        float g = std::tan(wc / (2.0f * static_cast<float>(oversampledRate_)));
        float k = resNorm * resGainScale_;

        float A = g / (1.0f + g);
        float f0 = A * A * input + A * s1_ / (1.0f + g) + s2_ / (1.0f + g);

        float y2 = Solver::solve(f0, A * A, k, s2_);

        float satFb = Saturator::value(k * y2);
        float u = input - satFb;
        float v1 = (g * u + s1_) / (1.0f + g);
        s1_ = 2.0f * v1 - s1_;
        float v2 = (g * v1 + s2_) / (1.0f + g);
        s2_ = 2.0f * v2 - s2_;

        return v2;
    }

private:
    double oversampledRate_{176400.0};
    float s1_{0.0f};
    float s2_{0.0f};
    float resGainScale_{2.2f};
};

// ---------------------------------------------------------------------------
// Vintage filter model selection. Mono targets only for now: Jupiter-8 and
// CS-80 are polyphonic and deferred (see docs/filter-architecture-plan.md
// section on synth priority). ARP 2600 here means the "4012" board, the
// Moog-derived ladder generally regarded as the better-sounding of its two
// production VCF revisions (see compendium section 77).
// ---------------------------------------------------------------------------

enum class VintageFilterModel {
    Minimoog = 0, // Moog transistor ladder, matched stage capacitors
    Arp2600 = 1,  // ARP 2600 "4012" board -- Moog-derived ladder, same saturator family as Minimoog
    TB303 = 2,    // Diode/transistor ladder, deliberately mismatched capacitors
    MS20 = 3      // Korg MS-20 Sallen-Key diode filter
};

constexpr int kNumVintageFilterModels = 4;

class Filter {
public:
    Filter();
    ~Filter() = default;

    void setSampleRate(double sampleRate);
    void setVintageModel(VintageFilterModel model);
    VintageFilterModel getVintageModel() const { return model_; }
    void setPreDrive(float drive) { preDrive_ = std::max(1.0f, drive); }

    void reset();

    float processSample(float input, float cutoffHz, float resonance);

private:
    double sampleRate_{44100.0};
    double oversampledRate_{176400.0};

    VintageFilterModel model_{VintageFilterModel::Minimoog};
    float preDrive_{1.0f};

    // Accuracy-tier cores (NewtonSolver) for each mono target. Minimoog and
    // ARP 2600 share the same LadderFilter<TanhSaturator, ...> instantiation
    // (per the synth pairing table) but get independent instances so their
    // capacitor/state don't collide and so they can be calibrated apart
    // later without another refactor.
    LadderFilter<TanhSaturator, NewtonSolver<TanhSaturator, 5>> minimoogCore_;
    LadderFilter<TanhSaturator, NewtonSolver<TanhSaturator, 5>> arp2600Core_;
    LadderFilter<AsymmetricSaturator, NewtonSolver<AsymmetricSaturator, 5>> tb303Core_;
    SallenKeyFilter<AsymmetricSaturator, NewtonSolver<AsymmetricSaturator, 5>> ms20Core_;

    static constexpr int FIR_TAPS = 16;
    std::array<float, FIR_TAPS> upBuffer1_{};
    std::array<float, FIR_TAPS> downBuffer1_{};
    int upIdx1_{0};
    int downIdx1_{0};

    float processOversampledSample(float input, float cutoffHz, float resonance);
};

} // namespace gritbaal

#endif // GRITBAAL_FILTER_HPP
