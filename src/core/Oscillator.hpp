#ifndef GRITBAAL_OSCILLATOR_HPP
#define GRITBAAL_OSCILLATOR_HPP

#include <cmath>

namespace gritbaal {

enum class Waveform {
    Saw = 0,
    Square = 1
};

class Oscillator {
public:
    Oscillator();
    ~Oscillator() = default;

    void setSampleRate(double sampleRate);
    void setWaveform(Waveform wave) { waveform_ = wave; }
    Waveform getWaveform() const { return waveform_; }

    void noteOn(int noteNumber, bool slide);
    void noteOff();

    float processNextSample();
    bool isSliding() const { return isSliding_; }

    void resetFilterStates();

private:
    double sampleRate_{44100.0};
    Waveform waveform_{Waveform::Saw};

    double phase_{0.0};
    double currentFreq_{440.0};
    double targetFreq_{440.0};
    bool isSliding_{false};
    double slideCoeff_{0.0};

    // Filter coefficients & states
    double lpfSawCoeff_{0.0};
    double lpfSawState_{0.0};

    double hpfSqCoeff_{0.0};
    double hpfSqX1_{0.0};
    double hpfSqY1_{0.0};

    static double noteToFreq(int note) {
        return 440.0 * std::pow(2.0, (note - 69) / 12.0);
    }
};

} // namespace gritbaal

#endif // GRITBAAL_OSCILLATOR_HPP
