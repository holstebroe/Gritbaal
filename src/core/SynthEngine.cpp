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
    env1_.setSampleRate(sampleRate_);
    env2_.setSampleRate(sampleRate_);
    filter_.setSampleRate(sampleRate_);
    lfo1_.setSampleRate(sampleRate_);
    lfo2_.setSampleRate(sampleRate_);
}

void SynthEngine::reset() {
    filter_.reset();
    osc_.resetFilterStates();
    lfo1_.reset();
    lfo2_.reset();
    currentNote_ = -1;
    isNoteActive_ = false;
    railVoltage_ = 1.0f;
    powerSagLpf_ = 0.0f;
    effectiveCutoffNorm_ = 0.5f;
    effectivePw1Norm_ = 0.5f;
    effectivePw2Norm_ = 0.5f;
}

void SynthEngine::noteOn(int noteNumber, float velocity) {
    bool isSlide = isNoteActive_;
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

    env1_.setAttack(params_.env1Attack);
    env1_.setDecay(params_.env1Decay);
    env1_.setSustain(params_.env1Sustain);
    env1_.setRelease(params_.env1Release);

    env2_.setAttack(params_.env2Attack);
    env2_.setDecay(params_.env2Decay);
    env2_.setSustain(params_.env2Sustain);
    env2_.setRelease(params_.env2Release);

    env1_.noteOn();
    env2_.noteOn();
}

void SynthEngine::noteOff(int noteNumber) {
    if (noteNumber == currentNote_ || noteNumber < 0) {
        isNoteActive_ = false;
        osc_.noteOff();
        env1_.noteOff();
        env2_.noteOff();
    }
}

void SynthEngine::processAudio(float* outLeft, float* outRight, int numFrames) {
    osc_.setVco1Waveform(params_.waveform);
    osc_.setVco2Waveform(params_.vco2Waveform);
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

    env1_.setAttack(params_.env1Attack);
    env1_.setDecay(params_.env1Decay);
    env1_.setSustain(params_.env1Sustain);
    env1_.setRelease(params_.env1Release);

    env2_.setAttack(params_.env2Attack);
    env2_.setDecay(params_.env2Decay);
    env2_.setSustain(params_.env2Sustain);
    env2_.setRelease(params_.env2Release);

    lfo1_.setRate(params_.lfo1Rate);
    lfo1_.setDepth(params_.lfo1Depth);
    lfo2_.setRate(params_.lfo2Rate);
    lfo2_.setDepth(params_.lfo2Depth);

    for (int i = 0; i < numFrames; ++i) {
        if (!env2_.isActive() && !isNoteActive_) {
            if (outLeft) outLeft[i] = 0.0f;
            if (outRight) outRight[i] = 0.0f;
            continue;
        }

        // 1. Process LFOs
        float lfo1Val = lfo1_.processNextSample(); // -lfo1Depth .. +lfo1Depth (Modulates Cutoff)
        float lfo2Val = lfo2_.processNextSample(); // -lfo2Depth .. +lfo2Depth (Modulates Pulse Width)

        // Apply LFO2 pulse width modulation
        float modulatedPw1 = std::clamp(params_.vco1PulseWidth + lfo2Val * 0.4f, 0.05f, 0.95f);
        float modulatedPw2 = std::clamp(params_.vco2PulseWidth + lfo2Val * 0.4f, 0.05f, 0.95f);
        osc_.setVco1PulseWidth(modulatedPw1);
        osc_.setVco2PulseWidth(modulatedPw2);

        effectivePw1Norm_ = (modulatedPw1 - 0.05f) / 0.90f;
        effectivePw2Norm_ = (modulatedPw2 - 0.05f) / 0.90f;

        // 2. Generate oscillator signal
        float rawOsc = osc_.processNextSample();

        // 3. Process envelopes
        env1_.processNextSample();
        env2_.processNextSample();

        float vcfEnvVal = env1_.getValue();
        float vcaEnvVal = env2_.getValue();

        // Calculate effective normalized cutoff considering base knob, LFO1, and ENV1 (VCF Env)
        float baseCutoff = std::clamp(params_.cutoff, 0.0f, 1.0f);
        float envModNorm = std::clamp(params_.envMod, 0.0f, 1.0f);
        float modCutoff = baseCutoff + lfo1Val + envModNorm * vcfEnvVal;
        effectiveCutoffNorm_ = std::clamp(modCutoff, 0.0f, 1.0f);

        float resNorm = std::clamp(params_.resonance, 0.0f, 1.0f);

        // Power Supply Rail Sag: dynamic voltage drop under heavy low-frequency load
        float load = std::abs(rawOsc) * vcaEnvVal;
        powerSagLpf_ += 0.005f * (load - powerSagLpf_);
        railVoltage_ = 1.0f - params_.powerSagAmount * 0.25f * powerSagLpf_;
        railVoltage_ = std::clamp(railVoltage_, 0.65f, 1.0f);

        float filterOut = 0.0f;
        float vcaSignal = 0.0f;

        // Exponential mapping for cutoff frequency
        float cNorm = std::clamp(baseCutoff + lfo1Val, 0.0f, 1.0f);
        float cTaper = cNorm * cNorm;
        float cv_base = 3.64385f * cTaper;
        float cv_envmod = envModNorm * vcfEnvVal * 4.0f;
        float cv_total = cv_base + cv_envmod;

        float effectiveCutoff = 150.0f * std::pow(2.0f, cv_total) * (1.0f - (0.15f * resNorm));
        effectiveCutoff *= railVoltage_;
        float totalCutoff = std::clamp(effectiveCutoff, 20.0f, 16000.0f);

        if (params_.mode == EmulationMode::Accurate) {
            filterOut = filter_.processAccurateSample(rawOsc, totalCutoff, resNorm);
        } else {
            filterOut = filter_.processSample(rawOsc, totalCutoff, resNorm);
        }

        vcaSignal = filterOut * vcaEnvVal;

        // Exaggerated Warmth Saturation (rich second-harmonic analog warmth + soft asymmetric clipping):
        // y = x + warmth * (0.8 * x^2 + 0.3 * x^3)
        if (params_.warmthAmount > 0.001f) {
            float w = params_.warmthAmount;
            float sq = vcaSignal * vcaSignal;
            float cube = sq * vcaSignal;
            vcaSignal = std::tanh(vcaSignal + w * (0.85f * sq + 0.35f * cube));
        }

        // Post-Filter Tube / Diode Overdrive Waveshaper: y = tanh(x + 0.15 * x^2)
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
