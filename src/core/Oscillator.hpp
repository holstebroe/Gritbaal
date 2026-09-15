#ifndef GRITBAAL_OSCILLATOR_HPP
#define GRITBAAL_OSCILLATOR_HPP

#include <cmath>
#include <algorithm>
#include <random>

namespace gritbaal {

enum class Waveform {
    Saw = 0,
    Square = 1, // Backward compatibility alias for Pulse
    Pulse = 1,
    Triangle = 2
};

enum class NoiseType {
    White = 0,
    Pink = 1
};

class Oscillator {
public:
    Oscillator();
    ~Oscillator() = default;

    void setSampleRate(double sampleRate);

    // Legacy/Convenience setters
    void setWaveform(Waveform wave) { vco1Wave_ = wave; }
    Waveform getWaveform() const { return vco1Wave_; }

    // Dual VCO & Sub/Noise parameter setters
    void setVco1Waveform(Waveform wave) { vco1Wave_ = wave; }
    void setVco2Waveform(Waveform wave) { vco2Wave_ = wave; }
    void setVco1PulseWidth(double pw) { vco1PulseWidth_ = std::clamp(pw, 0.05, 0.95); }
    void setVco2PulseWidth(double pw) { vco2PulseWidth_ = std::clamp(pw, 0.05, 0.95); }
    void setVco2DetuneSemitones(double semitones) { vco2Detune_ = semitones; }
    void setFmAmount(double amount) { fmAmount_ = std::clamp(amount, 0.0, 1.0); }
    void setHardSync(bool enable) { hardSync_ = enable; }
    void setVco1Level(double level) { vco1Level_ = level; }
    void setVco2Level(double level) { vco2Level_ = level; }
    void setSubLevel(double level) { subLevel_ = level; }
    void setNoiseLevel(double level) { noiseLevel_ = level; }
    void setNoiseType(NoiseType type) { noiseType_ = type; }
    void setThermalDriftAmount(double amount) { thermalDrift_ = amount; }
    void setVoiceMismatchCents(double cents) { mismatchCents_ = cents; }

    void noteOn(int noteNumber, bool slide);
    void noteOff();

    float processNextSample();
    bool isSliding() const { return isSliding_; }

    void resetFilterStates();

private:
    double sampleRate_{44100.0};

    // VCO1 & VCO2 Parameters
    Waveform vco1Wave_{Waveform::Saw};
    Waveform vco2Wave_{Waveform::Saw};
    double vco1PulseWidth_{0.5};
    double vco2PulseWidth_{0.5};
    double vco2Detune_{0.0}; // in semitones (-24.0 to +24.0)
    double fmAmount_{0.0};   // 0.0 to 1.0 exponential FM
    bool hardSync_{false};
    double vco1Level_{1.0};
    double vco2Level_{0.0};
    double subLevel_{0.0};
    double noiseLevel_{0.0};
    NoiseType noiseType_{NoiseType::White};
    double thermalDrift_{0.1}; // Thermal pitch walk scaling
    double mismatchCents_{0.0};

    // Oscillator States
    double phase1_{0.0};
    double phase2_{0.0};
    double subPhase_{0.0};
    double triState1_{0.0};
    double triState2_{0.0};

    double currentFreq_{440.0};
    double targetFreq_{440.0};
    bool isSliding_{false};
    double slideCoeff_{0.0};

    // Thermal Pitch Walk (1/f process)
    double thermalWalk1_{0.0};
    double thermalWalk2_{0.0};
    std::mt19937 rng_{1337};
    std::normal_distribution<double> gaussianDist_{0.0, 1.0};

    // Pink Noise Filter States (Paul Kellet 3-pole/7-pole filter)
    double pinkB0_{0.0};
    double pinkB1_{0.0};
    double pinkB2_{0.0};
    double pinkB3_{0.0};
    double pinkB4_{0.0};
    double pinkB5_{0.0};
    double pinkB6_{0.0};

    // Legacy 1-pole filter states (retained for classic mode smoothing)
    double lpfSawCoeff_{0.0};
    double lpfSawState_{0.0};
    double hpfSqCoeff_{0.0};
    double hpfSqX1_{0.0};
    double hpfSqY1_{0.0};

    static double noteToFreq(int note) {
        return 440.0 * std::pow(2.0, (note - 69) / 12.0);
    }

    double generatePolyBlepWave(double phase, double phaseInc, Waveform wave, double pw, double& triState);
    double generateNoiseSample();
};

} // namespace gritbaal

#endif // GRITBAAL_OSCILLATOR_HPP
