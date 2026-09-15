#include "SynthEngine.hpp"
#include <algorithm>
#include <cmath>

namespace gritbaal {

SynthEngine::SynthEngine() {
    setSampleRate(44100.0);
}

void SynthEngine::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
    osc_.setSampleRate(sampleRate_);
    env_.setSampleRate(sampleRate_);
    filter_.setSampleRate(sampleRate_);
}

void SynthEngine::reset() {
    filter_.reset();
    osc_.resetFilterStates();
    currentNote_ = -1;
    isNoteActive_ = false;
    accentLevel_ = 0.0f;
    railVoltage_ = 1.0f;
    powerSagLpf_ = 0.0f;
}

void SynthEngine::noteOn(int noteNumber, float velocity) {
    bool isSlide = isNoteActive_;
    bool isAccent = (velocity >= 0.8f);
    accentLevel_ = isAccent ? 1.0f : 0.0f;

    currentNote_ = noteNumber;
    isNoteActive_ = true;

    osc_.setVco1Waveform(params_.waveform);
    osc_.setVco2Waveform(params_.vco2Waveform);
    osc_.setVco1PulseWidth(params_.vco1PulseWidth);
    osc_.setVco2PulseWidth(params_.vco2PulseWidth);
    osc_.setVco2DetuneSemitones(params_.vco2Detune);
    osc_.setFmAmount(params_.fmAmount);
    osc_.setHardSync(params_.hardSync);
    osc_.setVco1Level(params_.vco1Level);
    osc_.setVco2Level(params_.vco2Level);
    osc_.setSubLevel(params_.subLevel);
    osc_.setNoiseLevel(params_.noiseLevel);
    osc_.setNoiseType(params_.noiseType);
    osc_.setThermalDriftAmount(params_.thermalDrift);

    osc_.noteOn(noteNumber, isSlide);
    env_.setDecay(params_.decay);
    env_.noteOn(isAccent, isSlide, params_.accent);
}

void SynthEngine::noteOff(int noteNumber) {
    if (noteNumber == currentNote_ || noteNumber < 0) {
        isNoteActive_ = false;
        osc_.noteOff();
        env_.noteOff();
    }
}

void SynthEngine::processAudio(float* outLeft, float* outRight, int numFrames) {
    osc_.setVco1Waveform(params_.waveform);
    osc_.setVco2Waveform(params_.vco2Waveform);
    osc_.setVco1PulseWidth(params_.vco1PulseWidth);
    osc_.setVco2PulseWidth(params_.vco2PulseWidth);
    osc_.setVco2DetuneSemitones(params_.vco2Detune);
    osc_.setFmAmount(params_.fmAmount);
    osc_.setHardSync(params_.hardSync);
    osc_.setVco1Level(params_.vco1Level);
    osc_.setVco2Level(params_.vco2Level);
    osc_.setSubLevel(params_.subLevel);
    osc_.setNoiseLevel(params_.noiseLevel);
    osc_.setNoiseType(params_.noiseType);
    osc_.setThermalDriftAmount(params_.thermalDrift);

    filter_.setFilterType(params_.filterType);
    filter_.setPreDrive(params_.preFilterDrive);
    env_.setDecay(params_.decay);

    for (int i = 0; i < numFrames; ++i) {
        if (!env_.isActive() && !isNoteActive_) {
            if (outLeft) outLeft[i] = 0.0f;
            if (outRight) outRight[i] = 0.0f;
            continue;
        }

        // 1. Generate oscillator signal
        float rawOsc = osc_.processNextSample();

        // 2. Process envelope sample
        env_.processNextSample();
        float vcfEnvVal = env_.getVcfEnv();
        float vcaEnvVal = env_.getVcaEnv();
        float accentCapVal = env_.getAccentCap();
        float accentVcaVal = env_.getAccentVca();
        bool noteAccent = env_.isAccent();

        float cNorm = std::min(std::max(params_.cutoff, 0.0f), 1.0f);
        float resNorm = std::min(std::max(params_.resonance, 0.0f), 1.0f);
        float envModNorm = std::min(std::max(params_.envMod, 0.0f), 1.0f);
        float accentNorm = std::min(std::max(params_.accent, 0.0f), 1.0f);

        // Power Supply Rail Sag: dynamic voltage drop under heavy low-frequency load
        float load = std::abs(rawOsc) * vcaEnvVal;
        powerSagLpf_ += 0.005f * (load - powerSagLpf_);
        railVoltage_ = 1.0f - params_.powerSagAmount * 0.25f * powerSagLpf_;
        railVoltage_ = std::clamp(railVoltage_, 0.65f, 1.0f);

        float filterOut = 0.0f;
        float vcaSignal = 0.0f;

        if (params_.mode == EmulationMode::Accurate) {
            // --- ACCURATE MODE: Current-Domain Control Summing & Filter Processing ---
            float cTaper = cNorm * cNorm;
            float envModTaper = envModNorm * envModNorm;

            float cv_base = 3.64385f * cTaper;
            float cv_offset = envModTaper * 0.80735f;

            float effectiveEnvMod = noteAccent ? (envModNorm + (1.0f - envModNorm) * accentNorm) : envModNorm;
            float effectiveEnvModTaper = effectiveEnvMod * effectiveEnvMod;
            float cv_envmod = effectiveEnvModTaper * vcfEnvVal * 3.5f;

            float directAccentPortion = (1.0f - resNorm * 0.7f) * vcfEnvVal;
            float sweepCapPortion = (resNorm * 0.7f) * accentCapVal;
            float accentSweepSignal = directAccentPortion + sweepCapPortion;

            float cv_accent = noteAccent ? (accentNorm * accentSweepSignal * 1.5f) : (accentNorm * sweepCapPortion * 0.75f);

            float cv_total = cv_base + cv_offset + cv_envmod + cv_accent;

            float effectiveCutoff = 200.0f * std::pow(2.0f, cv_total) * (1.0f - (0.15f * resNorm));
            // Power sag slightly lowers filter cutoff ceiling
            effectiveCutoff *= railVoltage_;
            float totalCutoff = std::min(std::max(effectiveCutoff, 20.0f), 15000.0f);

            filterOut = filter_.processAccurateSample(rawOsc, totalCutoff, resNorm);

            float vcaGain = vcaEnvVal;
            if (noteAccent) {
                vcaGain += accentVcaVal * accentNorm * 0.8f;
            }

            float xVal = filterOut * vcaGain;
            if (xVal > 0.0f) {
                vcaSignal = std::tanh(xVal * 1.1f);
            } else {
                vcaSignal = std::tanh(xVal * 0.9f);
            }
        } else {
            // --- SIMPLIFIED MODE ---
            float cTaper = cNorm * cNorm;
            float envModTaper = envModNorm * envModNorm;

            float cv_base = 3.64385f * cTaper;
            float cv_offset = envModTaper * 0.80735f;

            float effectiveEnvMod = noteAccent ? (envModNorm + (1.0f - envModNorm) * accentNorm) : envModNorm;
            float effectiveEnvModTaper = effectiveEnvMod * effectiveEnvMod;
            float cv_envmod = effectiveEnvModTaper * vcfEnvVal * 3.5f;

            float directAccentPortion = (1.0f - resNorm * 0.7f) * vcfEnvVal;
            float sweepCapPortion = (resNorm * 0.7f) * accentCapVal;
            float accentSweepSignal = directAccentPortion + sweepCapPortion;

            float cv_accent = noteAccent ? (accentNorm * accentSweepSignal * 1.5f) : (accentNorm * sweepCapPortion * 0.75f);
            float cv_total = cv_base + cv_offset + cv_envmod + cv_accent;

            float effectiveCutoff = 200.0f * std::pow(2.0f, cv_total) * (1.0f - (0.15f * resNorm));
            effectiveCutoff *= railVoltage_;
            float totalCutoff = std::min(std::max(effectiveCutoff, 20.0f), 14000.0f);

            filterOut = filter_.processSample(rawOsc, totalCutoff, resNorm);

            float vcaGain = vcaEnvVal;
            if (noteAccent && accentVcaVal > 0.0001f) {
                vcaGain += accentVcaVal * accentNorm * 0.8f;
            }

            vcaSignal = filterOut * vcaGain;
            if (noteAccent && accentNorm > 0.01f) {
                float satDrive = 1.0f + accentNorm * 0.25f;
                if (vcaSignal > 0.0f) {
                    vcaSignal = std::tanh(vcaSignal * satDrive);
                } else {
                    vcaSignal = std::tanh(vcaSignal * (satDrive * 0.85f));
                }
            }
        }

        // Post-Filter Tube / Diode Overdrive Waveshaper (Section 5.3)
        // y = tanh(x + 0.15 * x^2)
        if (params_.overdriveAmount > 0.001f) {
            float driveScale = 1.0f + params_.overdriveAmount * 3.0f;
            float xDriven = vcaSignal * driveScale;
            float asymmetricVal = xDriven + 0.15f * xDriven * xDriven;
            vcaSignal = std::tanh(asymmetricVal);
        }

        float finalSample = vcaSignal * params_.masterVolume * railVoltage_;

        if (outLeft) outLeft[i] = finalSample;
        if (outRight) outRight[i] = finalSample;
    }
}

} // namespace gritbaal
