#ifndef GRITBAAL_LFO_HPP
#define GRITBAAL_LFO_HPP

#include "MathConstants.hpp"
#include <cmath>
#include <algorithm>

namespace gritbaal {

class Lfo {
public:
    Lfo() = default;
    ~Lfo() = default;

    void setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }
    void setRate(float rateHz) { rateHz_ = std::clamp(rateHz, 0.05f, 30.0f); }
    void setDepth(float depth) { depth_ = std::clamp(depth, 0.0f, 1.0f); }
    void reset() { phase_ = 0.0; }

    float processNextSample() {
        double phaseInc = static_cast<double>(rateHz_) / sampleRate_;
        phase_ += phaseInc;
        if (phase_ >= 1.0) {
            phase_ -= 1.0;
        }
        // Pure Sine shape in range [-depth_, +depth_]
        double val = std::sin(kTWO_PI * phase_);
        return static_cast<float>(val * depth_);
    }

private:
    double sampleRate_{44100.0};
    float rateHz_{1.0f};
    float depth_{0.0f};
    double phase_{0.0};
};

} // namespace gritbaal

#endif // GRITBAAL_LFO_HPP
