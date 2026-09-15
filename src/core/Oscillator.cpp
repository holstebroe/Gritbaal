#include "Oscillator.hpp"
#include <cmath>
#include <algorithm>

namespace gritbaal {

// PolyBLEP residual function for band-limited step discontinuities
static inline double polyBlepResidual(double t, double dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

Oscillator::Oscillator() {
    setSampleRate(44100.0);
}

void Oscillator::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;

    // Nominal analogue slide lag time constant ~60 ms
    double slideTimeSec = 0.060;
    slideCoeff_ = std::exp(-1.0 / (sampleRate_ * slideTimeSec));

    // 1-pole LPF at 14 kHz for sawtooth peak rounding
    double fcLpf = 14000.0;
    lpfSawCoeff_ = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * fcLpf / sampleRate_);

    // 1-pole HPF at 150 Hz for pulse wave phase shift / tilt
    double fcHpf = 150.0;
    hpfSqCoeff_ = std::exp(-2.0 * 3.14159265358979323846 * fcHpf / sampleRate_);

    resetFilterStates();
}

void Oscillator::resetFilterStates() {
    lpfSawState_ = 0.0;
    hpfSqX1_ = 0.0;
    hpfSqY1_ = 0.0;
    pinkB0_ = pinkB1_ = pinkB2_ = pinkB3_ = pinkB4_ = pinkB5_ = pinkB6_ = 0.0;
    triState1_ = 0.0;
    triState2_ = 0.0;
}

void Oscillator::noteOn(int noteNumber, bool slide) {
    double freq = noteToFreq(noteNumber);
    targetFreq_ = freq;

    if (!slide) {
        currentFreq_ = targetFreq_;
        isSliding_ = false;
    } else {
        isSliding_ = true;
    }
}

void Oscillator::noteOff() {
    // Note off release handled by envelope/voice
}

double Oscillator::generatePolyBlepWave(double phase, double phaseInc, Waveform wave, double pw, double& triState) {
    pw = std::clamp(pw, 0.02, 0.98);

    if (wave == Waveform::Saw) {
        // Trivial sawtooth: 1 - 2*phase
        double naive = 1.0 - 2.0 * phase;
        double blep = polyBlepResidual(phase, phaseInc);
        return naive - blep;
    } else if (wave == Waveform::Pulse || wave == Waveform::Square) {
        // Trivial pulse with variable width pw
        double naive = (phase < pw) ? 1.0 : -1.0;
        double blep1 = polyBlepResidual(phase, phaseInc);
        double blep2 = polyBlepResidual(std::fmod(phase + (1.0 - pw), 1.0), phaseInc);
        return naive - blep1 + blep2;
    } else if (wave == Waveform::Triangle) {
        // PolyBLEP integrated square wave for high quality anti-aliased triangle
        double sq = (phase < 0.5) ? 1.0 : -1.0;
        sq -= polyBlepResidual(phase, phaseInc);
        sq += polyBlepResidual(std::fmod(phase + 0.5, 1.0), phaseInc);

        // Integrate square wave: triState += 4 * sq * phaseInc
        triState += 4.0 * phaseInc * sq;
        // Leaky integration to prevent DC offset drift
        triState *= 0.9995;
        return triState;
    }

    return 0.0;
}

double Oscillator::generateNoiseSample() {
    double white = gaussianDist_(rng_) * 0.3;
    if (noiseType_ == NoiseType::White) {
        return white;
    } else {
        // Paul Kellett's refined 3 dB/octave pinking filter
        pinkB0_ = 0.99886 * pinkB0_ + white * 0.0555179;
        pinkB1_ = 0.99332 * pinkB1_ + white * 0.0750759;
        pinkB2_ = 0.96900 * pinkB2_ + white * 0.1538520;
        pinkB3_ = 0.86650 * pinkB3_ + white * 0.3104856;
        pinkB4_ = 0.55000 * pinkB4_ + white * 0.5329522;
        pinkB5_ = -0.7616 * pinkB5_ - white * 0.0168980;
        double pink = pinkB0_ + pinkB1_ + pinkB2_ + pinkB3_ + pinkB4_ + pinkB5_ + pinkB6_ + white * 0.5362;
        pinkB6_ = white * 0.115926;
        return pink * 0.12; // Normalize pink noise gain
    }
}

float Oscillator::processNextSample() {
    // 1. Portamento / Pitch Glide
    if (isSliding_) {
        currentFreq_ = targetFreq_ + (currentFreq_ - targetFreq_) * slideCoeff_;
        if (std::abs(currentFreq_ - targetFreq_) < 0.001) {
            currentFreq_ = targetFreq_;
            isSliding_ = false;
        }
    } else {
        currentFreq_ = targetFreq_;
    }

    // 2. Thermal pitch walk (Ornstein-Uhlenbeck continuous random walk)
    double tau = 5.0; // 5-second thermal correlation time
    double alpha = std::exp(-1.0 / (sampleRate_ * tau));
    thermalWalk1_ = alpha * thermalWalk1_ + std::sqrt(1.0 - alpha * alpha) * gaussianDist_(rng_) * 0.05;
    thermalWalk2_ = alpha * thermalWalk2_ + std::sqrt(1.0 - alpha * alpha) * gaussianDist_(rng_) * 0.05;

    double cents1 = mismatchCents_ + thermalDrift_ * thermalWalk1_;
    double freq1 = currentFreq_ * std::pow(2.0, cents1 / 1200.0);

    // 3. Generate VCO1 Phase
    double phaseInc1 = freq1 / sampleRate_;
    phase1_ += phaseInc1;
    if (phase1_ >= 1.0) {
        phase1_ -= 1.0;
    }

    // 4. Exponential FM & VCO2 Frequency calculation
    double vco2FreqBase = freq1 * std::pow(2.0, vco2Detune_ / 12.0);
    double cents2 = -mismatchCents_ + thermalDrift_ * thermalWalk2_;
    double freq2 = vco2FreqBase * std::pow(2.0, cents2 / 1200.0);

    // Generate VCO1 audio first to modulate VCO2 via FM if requested
    double vco1Out = generatePolyBlepWave(phase1_, phaseInc1, vco1Wave_, vco1PulseWidth_, triState1_);

    if (fmAmount_ > 0.0) {
        // Exponential FM from VCO1 to VCO2
        freq2 *= std::pow(2.0, vco1Out * fmAmount_ * 2.0);
    }

    // 5. Generate VCO2 Phase & Hard Sync
    double phaseInc2 = freq2 / sampleRate_;
    phase2_ += phaseInc2;

    if (hardSync_ && phase1_ < phaseInc1) {
        // Reset VCO2 phase on VCO1 cycle reset
        phase2_ = phase1_ * (phaseInc2 / phaseInc1);
    } else if (phase2_ >= 1.0) {
        phase2_ -= 1.0;
    }

    double vco2Out = generatePolyBlepWave(phase2_, phaseInc2, vco2Wave_, vco2PulseWidth_, triState2_);

    // 6. Sub-Oscillator (Square wave 1 octave below VCO1)
    subPhase_ += (phaseInc1 * 0.5);
    if (subPhase_ >= 1.0) {
        subPhase_ -= 1.0;
    }
    double subOut = (subPhase_ < 0.5) ? 0.75 : -0.75;

    // 7. Noise Generation
    double noiseOut = generateNoiseSample();

    // 8. Mixer Summation
    double mixOut = vco1Level_ * vco1Out +
                    vco2Level_ * vco2Out +
                    subLevel_ * subOut +
                    noiseLevel_ * noiseOut;

    return static_cast<float>(mixOut);
}

} // namespace gritbaal
