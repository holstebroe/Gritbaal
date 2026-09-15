#include "Oscillator.hpp"
#include <cmath>
#include <algorithm>

namespace gritbaal {

Oscillator::Oscillator() {
    setSampleRate(44100.0);
}

void Oscillator::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;

    // Nominal analogue slide lag time constant ~60 ms (Section 37)
    double slideTimeSec = 0.060;
    slideCoeff_ = std::exp(-1.0 / (sampleRate_ * slideTimeSec));

    // 1-pole LPF at 14 kHz for sawtooth peak rounding
    double fcLpf = 14000.0;
    lpfSawCoeff_ = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * fcLpf / sampleRate_);

    // 1-pole HPF at 150 Hz for square wave phase shift / tilt
    double fcHpf = 150.0;
    hpfSqCoeff_ = std::exp(-2.0 * 3.14159265358979323846 * fcHpf / sampleRate_);

    resetFilterStates();
}

void Oscillator::resetFilterStates() {
    lpfSawState_ = 0.0;
    hpfSqX1_ = 0.0;
    hpfSqY1_ = 0.0;
}

void Oscillator::noteOn(int noteNumber, bool slide) {
    double freq = noteToFreq(noteNumber);
    targetFreq_ = freq;

    if (!slide) {
        // Hard jump to new frequency without phase reset (continuous analogue VCO)
        currentFreq_ = targetFreq_;
        isSliding_ = false;
    } else {
        // Slide / portamento: glide smoothly from currentFreq_ to targetFreq_
        isSliding_ = true;
    }
}

void Oscillator::noteOff() {
    // Note off does not change pitch, release handled by envelope/voice
}

float Oscillator::processNextSample() {
    // Pitch Glide via 1-pole lag filter when sliding
    if (isSliding_) {
        currentFreq_ = targetFreq_ + (currentFreq_ - targetFreq_) * slideCoeff_;
        if (std::abs(currentFreq_ - targetFreq_) < 0.001) {
            currentFreq_ = targetFreq_;
            isSliding_ = false;
        }
    } else {
        currentFreq_ = targetFreq_;
    }

    double phaseInc = currentFreq_ / sampleRate_;
    phase_ += phaseInc;
    if (phase_ >= 1.0) {
        phase_ -= 1.0;
    }

    float out = 0.0f;
    if (waveform_ == Waveform::Saw) {
        // Negative-going sawtooth: 1.0 - 2.0 * phase_
        double rawSaw = 1.0 - 2.0 * phase_;

        // 1-pole LPF at 14 kHz to round sharp peaks
        lpfSawState_ += lpfSawCoeff_ * (rawSaw - lpfSawState_);

        // Mild quadratic distortion: f(x) = x - 0.05 * x^2
        double x = lpfSawState_;
        double curvedSaw = x - 0.05 * x * x;

        out = static_cast<float>(curvedSaw);
    } else {
        // Pitch-dependent duty cycle: approaches ~45% at high pitches, up to ~70% at very low frequencies
        double duty = 0.45 + 0.25 * std::exp(-currentFreq_ / 180.0);
        duty = std::min(0.70, std::max(0.45, duty));

        double rawSq = (phase_ < duty) ? 0.75 : -0.75;

        // 1-pole HPF fixed at 150 Hz to tilt top and bottom flats and cut sub-bass
        double hpfOut = hpfSqCoeff_ * (hpfSqY1_ + rawSq - hpfSqX1_);
        hpfSqX1_ = rawSq;
        hpfSqY1_ = hpfOut;

        out = static_cast<float>(hpfOut);
    }

    return out;
}

} // namespace gritbaal
