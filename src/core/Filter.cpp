#include "Filter.hpp"
#include <cmath>
#include <algorithm>

namespace gritbaal {

static const float FIR_COEFFS[16] = {
    -0.0031f, 0.0f, 0.0156f, 0.0f, -0.0528f, 0.0f, 0.3134f, 0.5f,
     0.3134f, 0.0f, -0.0528f, 0.0f, 0.0156f, 0.0f, -0.0031f, 0.0f
};

Filter::Filter() {
    setSampleRate(44100.0);
}

void Filter::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
    oversampledRate_ = sampleRate_ * 4.0;
    hpfFeedback_.setSampleRate(oversampledRate_);
    reset();
}

void Filter::reset() {
    stage1_.reset();
    stage2_.reset();
    stage3_.reset();
    stage4_.reset();
    hpfFeedback_.reset();
    upBuffer1_.fill(0.0f);
    upBuffer2_.fill(0.0f);
    downBuffer1_.fill(0.0f);
    downBuffer2_.fill(0.0f);
    upIdx1_ = upIdx2_ = downIdx1_ = downIdx2_ = 0;

    skS1_ = 0.0f;
    skS2_ = 0.0f;
}

float Filter::processSallenKeySample(float input, float cutoffHz, float resonance) {
    // 2-pole Sallen-Key Diode Filter (MS-20 style screaming self-oscillating VCF)
    // Run at oversampled rate
    float nyquist = static_cast<float>(oversampledRate_ * 0.5);
    float maxCutoff = std::min(18000.0f, 0.49f * nyquist);
    float totalCutoffHz = std::clamp(cutoffHz, 20.0f, maxCutoff);
    float resNorm = std::clamp(resonance, 0.0f, 1.0f);

    float wc = kTWO_PI_F * totalCutoffHz;
    float g = std::tan(wc / (2.0f * static_cast<float>(oversampledRate_)));
    float k = resNorm * 2.2f; // Sallen-Key resonance scaling up to self-oscillation boundary

    float A = g / (1.0f + g);
    float f0 = A * A * input + A * skS1_ / (1.0f + g) + skS2_ / (1.0f + g);

    // Solve y2 using Newton-Raphson: F(y2) = y2 + A^2 * tanh(k * y2) - f0 = 0
    float y2 = skS2_;
    for (int iter = 0; iter < 5; ++iter) {
        float th = std::tanh(k * y2);
        float fVal = y2 + A * A * th - f0;
        float fDev = 1.0f + A * A * k * (1.0f - th * th);
        float step = fVal / fDev;
        y2 -= step;
        if (std::abs(step) < 1.0e-6f) {
            break;
        }
    }

    // State updates using converged solution
    float satFb = std::tanh(k * y2);
    float u = input - satFb;
    float v1 = (g * u + skS1_) / (1.0f + g);
    skS1_ = 2.0f * v1 - skS1_;
    float v2 = (g * v1 + skS2_) / (1.0f + g);
    skS2_ = 2.0f * v2 - skS2_;

    return v2;
}

float Filter::processOversampledSample(float input, float cutoffHz, float resonance) {
    if (filterType_ == FilterType::SallenKey) {
        return processSallenKeySample(input, cutoffHz, resonance);
    }

    // 4-pole ZDF Transistor Ladder with capacitor mismatch scaling and Newton-Raphson feedback solve
    float nyquist = static_cast<float>(oversampledRate_ * 0.5);
    float maxCutoff = std::min(18000.0f, 0.49f * nyquist);
    float totalCutoffHz = std::clamp(cutoffHz, 20.0f, maxCutoff);

    float resNorm = std::clamp(resonance, 0.0f, 1.0f);
    float hpfCutoff = 150.0f + 100.0f * resNorm;
    hpfFeedback_.setCutoff(hpfCutoff);

    float resGain = resNorm * 16.5f;

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

    // Solve hpFb using Newton-Raphson: hpFb + G_hp * tanh(resGain * hpFb) - f0 = 0
    float f0 = G_hp * input + S_hp;
    float hpFb = hpfState.y1;

    for (int iter = 0; iter < 5; ++iter) {
        float th = std::tanh(resGain * hpFb);
        float fVal = hpFb + G_hp * th - f0;
        float fDev = 1.0f + G_hp * resGain * (1.0f - th * th);
        float step = fVal / fDev;
        hpFb -= step;
        if (std::abs(step) < 1.0e-6f) {
            break;
        }
    }

    float satFb = std::tanh(resGain * hpFb);
    float x1 = input - satFb;

    float y1 = stage1_.process(x1, g1);
    float y2 = stage2_.process(y1, g2);
    float y3 = stage3_.process(y2, g3);
    float y4 = stage4_.process(y3, g4);

    hpfFeedback_.process(y4);

    return y4;
}

float Filter::processSample(float input, float cutoffHz, float resonance) {
    // Pre-filter drive
    float drivenInput = std::tanh(input * preDrive_);

    // 4x oversampling with 16-tap polyphase FIR filtering
    float finalOut = 0.0f;

    for (int os = 0; os < 4; ++os) {
        // Upsampling step: insert input at os == 0, zero otherwise
        float upVal = (os == 0) ? drivenInput : 0.0f;
        upIdx1_ = (upIdx1_ + 1) % FIR_TAPS;
        upBuffer1_[upIdx1_] = upVal;

        float upSample = 0.0f;
        for (int tap = 0; tap < FIR_TAPS; ++tap) {
            int bufIdx = (upIdx1_ - tap + FIR_TAPS) % FIR_TAPS;
            upSample += upBuffer1_[bufIdx] * FIR_COEFFS[tap];
        }
        upSample *= 4.0f; // Gain restoration for 4x zero-stuffing

        // Process single oversampled sample through VCF core
        float osOut = processOversampledSample(upSample, cutoffHz, resonance);

        // Downsampling step: push to FIR downsampling buffer
        downIdx1_ = (downIdx1_ + 1) % FIR_TAPS;
        downBuffer1_[downIdx1_] = osOut;

        if (os == 3) {
            float downSample = 0.0f;
            for (int tap = 0; tap < FIR_TAPS; ++tap) {
                int bufIdx = (downIdx1_ - tap + FIR_TAPS) % FIR_TAPS;
                downSample += downBuffer1_[bufIdx] * FIR_COEFFS[tap];
            }
            finalOut = downSample;
        }
    }

    return finalOut;
}

float Filter::processCoupledLadderSample(float input, float cutoffHz, float resonance) {
    return processSample(input, cutoffHz, resonance);
}

} // namespace gritbaal
