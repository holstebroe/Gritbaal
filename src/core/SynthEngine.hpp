#ifndef GRITBAAL_SYNTH_ENGINE_HPP
#define GRITBAAL_SYNTH_ENGINE_HPP

#include "Oscillator.hpp"
#include "Envelope.hpp"
#include "Filter.hpp"
#include "Lfo.hpp"

namespace gritbaal {

enum class EmulationMode {
    Accurate = 0,
    Simplified = 1
};

struct SynthParameters {
    // Filter & General
    float cutoff{0.5f};        // Knob range 0.0 to 1.0
    float resonance{0.5f};     // Knob range 0.0 to 1.0
    float envMod{0.5f};        // Knob range 0.0 to 1.0
    Waveform waveform{Waveform::Saw};
    float masterVolume{0.8f};
    EmulationMode mode{EmulationMode::Accurate};

    // Dual ADSR Envelopes (ENV1 = VCF, ENV2 = AMP)
    float env1Attack{0.01f};   // 1ms to 3s
    float env1Decay{0.3f};    // 1ms to 5s
    float env1Sustain{0.4f};  // 0.0 to 1.0
    float env1Release{0.3f};  // 1ms to 5s

    float env2Attack{0.01f};   // 1ms to 3s
    float env2Decay{0.3f};    // 1ms to 5s
    float env2Sustain{0.7f};  // 0.0 to 1.0
    float env2Release{0.3f};  // 1ms to 5s

    // Dual LFOs
    float lfo1Rate{1.0f};      // 0.05 Hz to 30 Hz
    float lfo1Depth{0.0f};     // LFO1 -> Cutoff Modulation
    float lfo2Rate{2.0f};      // 0.05 Hz to 30 Hz
    float lfo2Depth{0.0f};     // LFO2 -> Pulse Width Modulation

    // Extended Core DSP Parameters
    Waveform vco2Waveform{Waveform::Saw};
    float vco1PulseWidth{0.5f};
    float vco2PulseWidth{0.5f};
    float vco2Detune{0.0f};     // Semitones (-24.0 to +24.0)
    float fmAmount{0.0f};       // 0.0 to 1.0
    bool hardSync{false};
    float vco1Level{1.0f};
    float vco2Level{0.0f};
    float subLevel{0.0f};
    float noiseLevel{0.0f};
    NoiseType noiseType{NoiseType::Crackle};

    FilterType filterType{FilterType::TransistorLadder};
    float preFilterDrive{1.0f};  // 1.0 to 5.0
    float overdriveAmount{0.0f}; // 0.0 to 1.0 post-filter tube/diode distortion
    float warmthAmount{0.0f};    // 0.0 to 1.0 even-harmonic analog warmth
    float powerSagAmount{0.0f};  // 0.0 to 1.0 dynamic rail voltage sag
    float thermalDrift{0.1f};    // Thermal walk scaling
};

class SynthEngine {
public:
    SynthEngine();
    ~SynthEngine() = default;

    void setSampleRate(double sampleRate);
    void reset();

    void noteOn(int noteNumber, float velocity);
    void noteOff(int noteNumber);

    void processAudio(float* outLeft, float* outRight, int numFrames);

    SynthParameters& getParams() { return params_; }
    const SynthParameters& getParams() const { return params_; }

    Oscillator& getOscillator() { return osc_; }
    Filter& getFilter() { return filter_; }

    // Modulated Realtime Values for UI double-arc rendering
    float getEffectiveCutoffNorm() const { return effectiveCutoffNorm_; }
    float getEffectivePw1Norm() const { return effectivePw1Norm_; }
    float getEffectivePw2Norm() const { return effectivePw2Norm_; }

private:
    double sampleRate_{44100.0};
    SynthParameters params_;

    Oscillator osc_;
    Envelope env1_; // VCF Envelope
    Envelope env2_; // AMP Envelope
    Filter filter_;
    Lfo lfo1_;      // Cutoff LFO
    Lfo lfo2_;      // Pulse Width LFO

    int currentNote_{-1};
    bool isNoteActive_{false};

    // Effective modulated parameter values for UI feedback
    float effectiveCutoffNorm_{0.5f};
    float effectivePw1Norm_{0.5f};
    float effectivePw2Norm_{0.5f};

    // Power Supply Rail Sag Simulation State
    float railVoltage_{1.0f};
    float powerSagLpf_{0.0f};
};

} // namespace gritbaal

#endif // GRITBAAL_SYNTH_ENGINE_HPP
