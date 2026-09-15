#ifndef GRITBAAL_ENVELOPE_HPP
#define GRITBAAL_ENVELOPE_HPP

#include <cmath>
#include <algorithm>

namespace gritbaal {

class Envelope {
public:
    Envelope();
    ~Envelope() = default;

    void setSampleRate(double sampleRate);

    // Full ADSR setters (times in seconds)
    void setAttack(float attackSec);
    void setDecay(float decaySec);
    void setSustain(float sustainLevel); // 0.0 to 1.0
    void setRelease(float releaseSec);

    void noteOn();
    void noteOff();

    void processNextSample();

    float getValue() const { return currentVal_; }
    bool isActive() const { return gate_ || (currentVal_ > 0.0001f); }

private:
    double sampleRate_{44100.0};

    bool gate_{false};
    float attackSec_{0.01f};
    float decaySec_{0.2f};
    float sustainLevel_{0.5f};
    float releaseSec_{0.3f};

    enum class State {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };

    State state_{State::Idle};
    float currentVal_{0.0f};

    float attackCoeff_{0.0f};
    float decayCoeff_{0.0f};
    float releaseCoeff_{0.0f};

    void updateCoefficients();
};

} // namespace gritbaal

#endif // GRITBAAL_ENVELOPE_HPP
