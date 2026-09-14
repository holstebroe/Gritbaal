#ifndef SYREBAS_FILTER_HPP
#define SYREBAS_FILTER_HPP

#include <cmath>
#include <array>

namespace syrebas {

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
        float w0 = 2.0f * 3.14159265358979323846f * cutoffHz;
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

class Filter {
public:
    Filter();
    ~Filter() = default;

    void setSampleRate(double sampleRate);
    void reset();

    float processSample(float input, float cutoffHz, float resonance);

    // Accurate coupled diode-ladder solver with inter-stage loading
    float processAccurateSample(float input, float cutoffHz, float resonance);

private:
    double sampleRate_{44100.0};
    double oversampledRate_{176400.0};

    TPTOnePole stage1_;
    TPTOnePole stage2_;
    TPTOnePole stage3_;
    TPTOnePole stage4_;

    // Coupled ladder node voltage states for accurate mode (v1, v2, v3, v4)
    float ladderV1_{0.0f};
    float ladderV2_{0.0f};
    float ladderV3_{0.0f};
    float ladderV4_{0.0f};
    float hpFbStateX1_{0.0f};
    float hpFbStateY1_{0.0f};
    float prevAccurateInput_{0.0f};

    HPFFeedback hpfFeedback_;

    // Diode ladder capacitor values / pole spreading for ~18dB/oct slope
    // C1 = 10nF, C2 = 15nF, C3 = 33nF, C4 = 10nF -> conductance scale = 1/C
    const float capScale1_{1.0000f};
    const float capScale2_{0.6667f};
    const float capScale3_{0.3030f};
    const float capScale4_{1.0000f};

    static constexpr int FIR_TAPS = 16;
    std::array<float, FIR_TAPS> upBuffer1_{};
    std::array<float, FIR_TAPS> upBuffer2_{};
    std::array<float, FIR_TAPS> downBuffer1_{};
    std::array<float, FIR_TAPS> downBuffer2_{};
    int upIdx1_{0};
    int upIdx2_{0};
    int downIdx1_{0};
    int downIdx2_{0};

    float processOversampledSample(float input, float cutoffHz, float resonance);
};

} // namespace syrebas

#endif // SYREBAS_FILTER_HPP
