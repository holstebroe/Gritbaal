#include "Filter.hpp"
#include <cmath>
#include <algorithm>

namespace gritbaal {

static const float FIR_COEFFS[16] = {
    -0.0031f, 0.0f, 0.0156f, 0.0f, -0.0528f, 0.0f, 0.3134f, 0.5f,
     0.3134f, 0.0f, -0.0528f, 0.0f, 0.0156f, 0.0f, -0.0031f, 0.0f
};

Filter::Filter() {
    // Minimoog / ARP 2600 (4012): matched stage capacitors, clean 24dB
    // ladder. ARP 2600 currently shares Minimoog's tuning -- see
    // docs/filter-architecture-plan.md section 4 for the calibration TODO.
    minimoogCore_.setCapScale(1.0f, 1.0f, 1.0f, 1.0f);
    arp2600Core_.setCapScale(1.0f, 1.0f, 1.0f, 1.0f);

    // TB-303: deliberately mismatched stage capacitors -- confirmed by
    // Stinchcombe's transfer-function analysis (compendium section 30.3).
    // This mismatch, not pole count, is the TB-303's "broken 24dB" character.
    tb303Core_.setCapScale(1.0000f, 0.6667f, 0.3030f, 1.0000f);

    setSampleRate(44100.0);
}

void Filter::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
    oversampledRate_ = sampleRate_ * 4.0;

    minimoogCore_.setSampleRate(oversampledRate_);
    arp2600Core_.setSampleRate(oversampledRate_);
    tb303Core_.setSampleRate(oversampledRate_);
    ms20Core_.setSampleRate(oversampledRate_);

    reset();
}

void Filter::setVintageModel(VintageFilterModel model) {
    if (model == model_) {
        return;
    }
    model_ = model;

    // Start the newly selected core from a clean state so switching models
    // mid-performance doesn't resurrect stale capacitor/feedback state.
    switch (model_) {
        case VintageFilterModel::Minimoog: minimoogCore_.reset(); break;
        case VintageFilterModel::Arp2600:  arp2600Core_.reset();  break;
        case VintageFilterModel::TB303:    tb303Core_.reset();    break;
        case VintageFilterModel::MS20:     ms20Core_.reset();     break;
    }
}

void Filter::reset() {
    minimoogCore_.reset();
    arp2600Core_.reset();
    tb303Core_.reset();
    ms20Core_.reset();

    upBuffer1_.fill(0.0f);
    downBuffer1_.fill(0.0f);
    upIdx1_ = downIdx1_ = 0;
}

float Filter::processOversampledSample(float input, float cutoffHz, float resonance) {
    switch (model_) {
        case VintageFilterModel::Minimoog: return minimoogCore_.process(input, cutoffHz, resonance);
        case VintageFilterModel::Arp2600:  return arp2600Core_.process(input, cutoffHz, resonance);
        case VintageFilterModel::TB303:    return tb303Core_.process(input, cutoffHz, resonance);
        case VintageFilterModel::MS20:     return ms20Core_.process(input, cutoffHz, resonance);
    }
    return 0.0f;
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

        // Process single oversampled sample through the active VCF core
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

} // namespace gritbaal
