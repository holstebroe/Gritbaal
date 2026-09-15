#ifndef GRITBAAL_SYNTH_ENGINE_HPP
#define GRITBAAL_SYNTH_ENGINE_HPP

#include "Oscillator.hpp"
#include "Envelope.hpp"
#include "Filter.hpp"

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
    float decay{0.5f};         // Knob range 0.0 to 1.0
    float accent{0.5f};        // Knob range 0.0 to 1.0
    Waveform waveform{Waveform::Saw};
    float masterVolume{0.8f};
    EmulationMode mode{EmulationMode::Accurate};

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
    NoiseType noiseType{NoiseType::White};

    FilterType filterType{FilterType::TransistorLadder};
    float preFilterDrive{1.0f};  // 1.0 to 5.0
    float overdriveAmount{0.0f}; // 0.0 to 1.0 post-filter tube/diode distortion
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

private:
    double sampleRate_{44100.0};
    SynthParameters params_;

    Oscillator osc_;
    Envelope env_;
    Filter filter_;

    int currentNote_{-1};
    bool isNoteActive_{false};
    float accentLevel_{0.0f};

    // Power Supply Rail Sag Simulation State
    float railVoltage_{1.0f};
    float powerSagLpf_{0.0f};

    // Smooth VCA Gate Envelope to prevent Note On / Off clicks
    float vcaGateEnv_{0.0f};
    float vcaAttackCoeff_{0.0f};
    float vcaReleaseCoeff_{0.0f};
};

} // namespace gritbaal

#endif // GRITBAAL_SYNTH_ENGINE_HPP
