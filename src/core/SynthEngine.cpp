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
    for (int i = 0; i < static_cast<int>(ModTarget::Count); ++i) {
        effectiveTargetNorm_[i] = 0.5f;
    }
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
    lfo1_.setDepth(1.0f);
    lfo2_.setRate(params_.lfo2Rate);
    lfo2_.setDepth(1.0f);

    for (int i = 0; i < numFrames; ++i) {
        if (!env2_.isActive() && !isNoteActive_) {
            if (outLeft) outLeft[i] = 0.0f;
            if (outRight) outRight[i] = 0.0f;
            continue;
        }

        // 1. Process LFOs
        float rawLfo1 = lfo1_.processNextSample(); // -1.0 to +1.0
        float rawLfo2 = lfo2_.processNextSample(); // -1.0 to +1.0

        float lfo1Val = rawLfo1 * ((params_.lfo1Depth - 0.5f) * 2.0f);
        float lfo2Val = rawLfo2 * ((params_.lfo2Depth - 0.5f) * 2.0f);

        // 2. Process Envelopes
        env1_.processNextSample();
        env2_.processNextSample();

        float env1Val = env1_.getValue();
        float env2Val = env2_.getValue();

        float env1ModVal = env1Val * ((params_.env1Amount - 0.5f) * 2.0f);

        // 3. Aggregate Modulations
        float modAcc[19] = {0.0f};
        auto addMod = [&](ModTarget t, float val) {
            int idx = static_cast<int>(t);
            if (idx >= 0 && idx < 19) modAcc[idx] += val;
        };

        addMod(params_.lfo1Target, lfo1Val);
        addMod(params_.lfo2Target, lfo2Val);
        addMod(params_.env1Target, env1ModVal);

        float modCutoff   = modAcc[0];
        float modReson    = modAcc[1];
        float modPitch    = modAcc[2];
        float modPw1      = modAcc[3];
        float modPw2      = modAcc[4];
        float modDetune   = modAcc[5];
        float modFm       = modAcc[6];
        float modV1Vol    = modAcc[7];
        float modV2Vol    = modAcc[8];
        float modSubVol   = modAcc[9];
        float modRingMod  = modAcc[10];
        float modNoiseVol = modAcc[11];
        float modPreDrive = modAcc[12];
        float modTubeDrive= modAcc[13];
        float modAmp      = modAcc[14];
        float modLfo1R    = modAcc[15];
        float modLfo1A    = modAcc[16];
        float modLfo2R    = modAcc[17];
        float modLfo2A    = modAcc[18];

        lfo1_.setRate(std::clamp(params_.lfo1Rate + modLfo1R * 10.0f, 0.05f, 30.0f));
        lfo2_.setRate(std::clamp(params_.lfo2Rate + modLfo2R * 10.0f, 0.05f, 30.0f));

        float modulatedPw1 = std::clamp(params_.vco1PulseWidth + modPw1 * 0.4f, 0.05f, 0.95f);
        float modulatedPw2 = std::clamp(params_.vco2PulseWidth + modPw2 * 0.4f, 0.05f, 0.95f);
        osc_.setVco1PulseWidth(modulatedPw1);
        osc_.setVco2PulseWidth(modulatedPw2);
        osc_.setPitchModulationSemitones(modPitch * 12.0f);
        osc_.setVco2DetuneSemitones(std::clamp(params_.vco2Detune + modDetune * 12.0f, -24.0f, 24.0f));
        osc_.setFmAmount(std::clamp(params_.fmAmount + modFm, 0.0f, 1.0f));

        osc_.setVco1Level(std::clamp(params_.vco1Level + modV1Vol, 0.0f, 1.0f));
        osc_.setVco2Level(std::clamp(params_.vco2Level + modV2Vol, 0.0f, 1.0f));
        osc_.setSubLevel(std::clamp(params_.subLevel + modSubVol, 0.0f, 1.0f));
        osc_.setNoiseLevel(std::clamp(params_.noiseLevel + modNoiseVol, 0.0f, 1.0f));

        // Store effective normalized values for UI double-arc rendering across all targets
        effectiveTargetNorm_[static_cast<int>(ModTarget::Cutoff)]   = std::clamp(params_.cutoff + modCutoff, 0.0f, 1.0f);
        effectiveTargetNorm_[static_cast<int>(ModTarget::Resonance)] = std::clamp(params_.resonance + modReson, 0.0f, 1.0f);
        effectiveTargetNorm_[static_cast<int>(ModTarget::Pitch)]     = std::clamp(0.5f + modPitch * 0.5f, 0.0f, 1.0f);
        effectiveTargetNorm_[static_cast<int>(ModTarget::Pw1)]       = (modulatedPw1 - 0.05f) / 0.90f;
        effectiveTargetNorm_[static_cast<int>(ModTarget::Pw2)]       = (modulatedPw2 - 0.05f) / 0.90f;
        effectiveTargetNorm_[static_cast<int>(ModTarget::Detune)]    = std::clamp(0.5f + (params_.vco2Detune / 48.0f) + modDetune * 0.25f, 0.0f, 1.0f);
        effectiveTargetNorm_[static_cast<int>(ModTarget::FmAmount)]  = std::clamp(params_.fmAmount + modFm, 0.0f, 1.0f);
        effectiveTargetNorm_[static_cast<int>(ModTarget::Vco1Vol)]   = std::clamp(params_.vco1Level + modV1Vol, 0.0f, 1.0f);
        effectiveTargetNorm_[static_cast<int>(ModTarget::Vco2Vol)]   = std::clamp(params_.vco2Level + modV2Vol, 0.0f, 1.0f);
        effectiveTargetNorm_[static_cast<int>(ModTarget::SubVol)]    = std::clamp(params_.subLevel + modSubVol, 0.0f, 1.0f);
        effectiveTargetNorm_[static_cast<int>(ModTarget::RingMod)]   = std::clamp(modRingMod, 0.0f, 1.0f);
        effectiveTargetNorm_[static_cast<int>(ModTarget::NoiseVol)]  = std::clamp(params_.noiseLevel + modNoiseVol, 0.0f, 1.0f);
        effectiveTargetNorm_[static_cast<int>(ModTarget::PreDrive)]  = std::clamp((params_.preFilterDrive - 1.0f) / 4.0f + modPreDrive * 0.5f, 0.0f, 1.0f);
        effectiveTargetNorm_[static_cast<int>(ModTarget::TubeDrive)] = std::clamp(params_.overdriveAmount + modTubeDrive, 0.0f, 1.0f);
        effectiveTargetNorm_[static_cast<int>(ModTarget::Amp)]       = std::clamp(params_.masterVolume + modAmp, 0.0f, 1.0f);
        effectiveTargetNorm_[static_cast<int>(ModTarget::Lfo1Rate)]  = std::clamp(params_.lfo1Rate / 30.0f + modLfo1R * 0.33f, 0.0f, 1.0f);
        effectiveTargetNorm_[static_cast<int>(ModTarget::Lfo1Amount)]= std::clamp(params_.lfo1Depth + modLfo1A * 0.5f, 0.0f, 1.0f);
        effectiveTargetNorm_[static_cast<int>(ModTarget::Lfo2Rate)]  = std::clamp(params_.lfo2Rate / 30.0f + modLfo2R * 0.33f, 0.0f, 1.0f);
        effectiveTargetNorm_[static_cast<int>(ModTarget::Lfo2Amount)]= std::clamp(params_.lfo2Depth + modLfo2A * 0.5f, 0.0f, 1.0f);

        float rawOsc = osc_.processNextSample();

        float baseCutoff = std::clamp(params_.cutoff, 0.0f, 1.0f);
        float effectiveCutoffNorm = effectiveTargetNorm_[static_cast<int>(ModTarget::Cutoff)];

        float resNorm = effectiveTargetNorm_[static_cast<int>(ModTarget::Resonance)];

        float effectivePreDrive = std::clamp(params_.preFilterDrive + modPreDrive * 2.0f, 1.0f, 5.0f);
        filter_.setFilterType(params_.filterType);
        filter_.setPreDrive(effectivePreDrive);

        float ampScale = std::clamp(1.0f + modAmp, 0.0f, 2.0f);
        float vcaEnvVal = env2Val * ampScale;

        float load = std::abs(rawOsc) * vcaEnvVal;
        float sagRate = (load > powerSagLpf_) ? 0.02f : 0.002f; // Fast sag under load, slow PSU recovery
        powerSagLpf_ += sagRate * (load - powerSagLpf_);
        railVoltage_ = 1.0f - params_.powerSagAmount * 0.25f * powerSagLpf_;
        railVoltage_ = std::clamp(railVoltage_, 0.65f, 1.0f);

        float cTaper = effectiveCutoffNorm * effectiveCutoffNorm;
        float cv_total = 6.90689f * cTaper; // Log2(18000 / 150) = 6.90689 octaves
        float effectiveCutoff = 150.0f * std::pow(2.0f, cv_total);
        effectiveCutoff *= railVoltage_;
        float totalCutoff = std::clamp(effectiveCutoff, 20.0f, 18000.0f);

        float filterOut = filter_.processSample(rawOsc, totalCutoff, resNorm);

        float vcaSignal = filterOut * vcaEnvVal;

        if (params_.warmthAmount > 0.001f) {
            float w = params_.warmthAmount;
            float sq = vcaSignal * vcaSignal;
            float cube = sq * vcaSignal;
            vcaSignal = std::tanh(vcaSignal + w * (0.85f * sq + 0.35f * cube));
        }

        float effOverdrive = std::clamp(params_.overdriveAmount + modTubeDrive, 0.0f, 1.0f);
        if (effOverdrive > 0.001f) {
            float driveScale = 1.0f + effOverdrive * 3.0f;
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
