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

    ladderV1_ = 0.0f;
    ladderV2_ = 0.0f;
    ladderV3_ = 0.0f;
    ladderV4_ = 0.0f;
    hpFbStateX1_ = 0.0f;
    hpFbStateY1_ = 0.0f;
    prevAccurateInput_ = 0.0f;

    skS1_ = 0.0f;
    skS2_ = 0.0f;
}

float Filter::processAccurateSample(float input, float cutoffHz, float resonance) {
    // Apply Pre-Filter Drive Stage
    float drivenInput = std::tanh(input * preDrive_);

    if (filterType_ == FilterType::SallenKey) {
        return processSallenKeySample(drivenInput, cutoffHz, resonance);
    }

    // 4x oversampling step for accurate coupled diode ladder
    float dt = 1.0f / static_cast<float>(oversampledRate_);
    float totalCutoffHz = std::min(std::max(cutoffHz, 20.0f), 18000.0f);

    float wc = 2.0f * 3.14159265358979323846f * totalCutoffHz;

    float resNorm = std::min(std::max(resonance, 0.0f), 1.0f);
    float hpfCutoff = 150.0f + 100.0f * resNorm;
    float hpfAlpha = 1.0f / (1.0f + 2.0f * 3.14159265358979323846f * hpfCutoff * dt);

    float kFb = resNorm * 33.0f;

    const float Vt = 0.052f;
    const float Vt_inv = 19.23f;

    float accOut = 0.0f;

    float prevIn = prevAccurateInput_;
    prevAccurateInput_ = drivenInput;

    for (int os = 0; os < 4; ++os) {
        float alphaOS = static_cast<float>(os + 1) / 4.0f;
        float currIn = prevIn + alphaOS * (drivenInput - prevIn);

        float inSample = currIn * 0.05f;

        float hpOut = hpfAlpha * (hpFbStateY1_ + ladderV4_ - hpFbStateX1_);
        hpFbStateX1_ = ladderV4_;
        hpFbStateY1_ = hpOut;

        float u = inSample - hpOut * kFb;

        float h = dt;
        float v1 = ladderV1_;
        float v2 = ladderV2_;
        float v3 = ladderV3_;
        float v4 = ladderV4_;

        // K1
        float dv1_1 = wc * Vt * (std::tanh((u - v1) * Vt_inv) - std::tanh((v1 - v2) * Vt_inv));
        float dv2_1 = wc * Vt * (std::tanh((v1 - v2) * Vt_inv) - std::tanh((v2 - v3) * Vt_inv));
        float dv3_1 = wc * Vt * (std::tanh((v2 - v3) * Vt_inv) - std::tanh((v3 - v4) * Vt_inv));
        float dv4_1 = 2.0f * wc * Vt * std::tanh((v3 - v4) * Vt_inv);

        // K2
        float v1_mid = v1 + 0.5f * h * dv1_1;
        float v2_mid = v2 + 0.5f * h * dv2_1;
        float v3_mid = v3 + 0.5f * h * dv3_1;
        float v4_mid = v4 + 0.5f * h * dv4_1;

        float hpOut_mid = hpfAlpha * (hpFbStateY1_ + v4_mid - hpFbStateX1_);
        float u_mid = inSample - hpOut_mid * kFb;

        float dv1_2 = wc * Vt * (std::tanh((u_mid - v1_mid) * Vt_inv) - std::tanh((v1_mid - v2_mid) * Vt_inv));
        float dv2_2 = wc * Vt * (std::tanh((v1_mid - v2_mid) * Vt_inv) - std::tanh((v2_mid - v3_mid) * Vt_inv));
        float dv3_2 = wc * Vt * (std::tanh((v2_mid - v3_mid) * Vt_inv) - std::tanh((v3_mid - v4_mid) * Vt_inv));
        float dv4_2 = 2.0f * wc * Vt * std::tanh((v3_mid - v4_mid) * Vt_inv);

        ladderV1_ += h * dv1_2;
        ladderV2_ += h * dv2_2;
        ladderV3_ += h * dv3_2;
        ladderV4_ += h * dv4_2;

        accOut += (ladderV4_ / 0.05f) * 0.25f;
    }

    return accOut;
}

float Filter::processSallenKeySample(float input, float cutoffHz, float resonance) {
    // 2-pole Sallen-Key Diode Filter (MS-20 style screaming self-oscillating VCF)
    float totalCutoffHz = std::min(std::max(cutoffHz, 20.0f), 18000.0f);
    float resNorm = std::min(std::max(resonance, 0.0f), 1.0f);

    float wc = 2.0f * 3.14159265358979323846f * totalCutoffHz;
    float g = std::tan(wc / (2.0f * static_cast<float>(sampleRate_)));
    float k = resNorm * 2.2f; // Sallen-Key resonance scaling up to screaming self-oscillation boundary

    // Diode feedback clipping non-linearity in Sallen-Key loop: y_fb = tanh(k * y2)
    float y2 = skS2_;
    for (int iter = 0; iter < 3; ++iter) {
        float satFb = std::tanh(k * y2);
        float u = input - satFb;
        float v1 = (g * u + skS1_) / (1.0f + g);
        float v2 = (g * v1 + skS2_) / (1.0f + g);
        y2 = v2;
    }

    // Final state update pass
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

    float totalCutoffHz = std::min(std::max(cutoffHz, 20.0f), 18000.0f);

    float resNorm = std::min(std::max(resonance, 0.0f), 1.0f);
    float hpfCutoff = 150.0f + 100.0f * resNorm;
    hpfFeedback_.setCutoff(hpfCutoff);

    float resGain = resNorm * 16.5f;

    float wc = 2.0f * 3.14159265358979323846f * totalCutoffHz;
    float gBase = std::tan(wc / (2.0f * static_cast<float>(oversampledRate_)));

    float g1 = gBase * capScale1_;
    float g2 = gBase * capScale2_;
    float g3 = gBase * capScale3_;
    float g4 = gBase * capScale4_;

    float savedS1 = stage1_.getState();
    float savedS2 = stage2_.getState();
    float savedS3 = stage3_.getState();
    float savedS4 = stage4_.getState();
    HPFFeedback::State savedHpfState = hpfFeedback_.getState();

    float hpFb = hpfFeedback_.process(0.0f);
    float satFb = std::tanh(hpFb * resGain);
    float x1 = input - satFb;

    for (int iter = 0; iter < 3; ++iter) {
        stage1_.setState(savedS1);
        stage2_.setState(savedS2);
        stage3_.setState(savedS3);
        stage4_.setState(savedS4);
        hpfFeedback_.setState(savedHpfState);

        float y1 = stage1_.process(x1, g1);
        float y2 = stage2_.process(y1, g2);
        float y3 = stage3_.process(y2, g3);
        float y4 = stage4_.process(y3, g4);

        hpFb = hpfFeedback_.process(y4);
        satFb = std::tanh(hpFb * resGain);
        x1 = input - satFb;
    }

    stage1_.setState(savedS1);
    stage2_.setState(savedS2);
    stage3_.setState(savedS3);
    stage4_.setState(savedS4);
    hpfFeedback_.setState(savedHpfState);

    float y1 = stage1_.process(x1, g1);
    float y2 = stage2_.process(y1, g2);
    float y3 = stage3_.process(y2, g3);
    float y4 = stage4_.process(y3, g4);

    hpfFeedback_.process(y4);

    return y4;
}

float Filter::processSample(float input, float cutoffHz, float resonance) {
    float drivenInput = std::tanh(input * preDrive_);

    if (filterType_ == FilterType::SallenKey) {
        return processSallenKeySample(drivenInput, cutoffHz, resonance);
    }

    float oversampledSamples[4];

    for (int i = 0; i < 2; ++i) {
        float inVal = (i == 0) ? drivenInput * 2.0f : 0.0f;
        upBuffer1_[upIdx1_] = inVal;

        float stage1Out = 0.0f;
        for (int tap = 0; tap < FIR_TAPS; ++tap) {
            int idx = (upIdx1_ - tap + FIR_TAPS) % FIR_TAPS;
            stage1Out += upBuffer1_[idx] * FIR_COEFFS[tap];
        }
        upIdx1_ = (upIdx1_ + 1) % FIR_TAPS;

        for (int j = 0; j < 2; ++j) {
            float inVal2 = (j == 0) ? stage1Out * 2.0f : 0.0f;
            upBuffer2_[upIdx2_] = inVal2;

            float stage2Out = 0.0f;
            for (int tap = 0; tap < FIR_TAPS; ++tap) {
                int idx = (upIdx2_ - tap + FIR_TAPS) % FIR_TAPS;
                stage2Out += upBuffer2_[idx] * FIR_COEFFS[tap];
            }
            upIdx2_ = (upIdx2_ + 1) % FIR_TAPS;

            oversampledSamples[i * 2 + j] = stage2Out;
        }
    }

    float filterOut[4];
    for (int k = 0; k < 4; ++k) {
        filterOut[k] = processOversampledSample(oversampledSamples[k], cutoffHz, resonance);
    }

    float downStage1[2];
    for (int k = 0; k < 2; ++k) {
        downBuffer1_[downIdx1_] = filterOut[k * 2];
        downIdx1_ = (downIdx1_ + 1) % FIR_TAPS;
        downBuffer1_[downIdx1_] = filterOut[k * 2 + 1];
        downIdx1_ = (downIdx1_ + 1) % FIR_TAPS;

        float outVal = 0.0f;
        for (int tap = 0; tap < FIR_TAPS; ++tap) {
            int idx = (downIdx1_ - 1 - tap + FIR_TAPS) % FIR_TAPS;
            outVal += downBuffer1_[idx] * FIR_COEFFS[tap];
        }
        downStage1[k] = outVal;
    }

    downBuffer2_[downIdx2_] = downStage1[0];
    downIdx2_ = (downIdx2_ + 1) % FIR_TAPS;
    downBuffer2_[downIdx2_] = downStage1[1];
    downIdx2_ = (downIdx2_ + 1) % FIR_TAPS;

    float finalOut = 0.0f;
    for (int tap = 0; tap < FIR_TAPS; ++tap) {
        int idx = (downIdx2_ - 1 - tap + FIR_TAPS) % FIR_TAPS;
        finalOut += downBuffer2_[idx] * FIR_COEFFS[tap];
    }

    return finalOut;
}

} // namespace gritbaal
