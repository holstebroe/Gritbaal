#ifndef SYREBAS_ENVELOPE_HPP
#define SYREBAS_ENVELOPE_HPP

namespace syrebas {

class Envelope {
public:
    Envelope();
    ~Envelope() = default;

    void setSampleRate(double sampleRate);
    void setDecay(float decayParam); // 0.0 to 1.0 -> 200ms to 2.5s

    void noteOn(bool isAccent, bool isSlide, float accentKnob = 1.0f);
    void noteOff();

    void processNextSample();

    float getVcfEnv() const { return vcfEnv_; }
    float getVcaEnv() const { return vcaEnv_; }
    float getAccentCap() const { return accentCap_; }
    float getAccentVca() const { return accentVca_; }
    bool isAccent() const { return isAccent_; }
    bool isActive() const { return gate_ || (vcaEnv_ > 0.0001f) || (vcfEnv_ > 0.0001f); }

private:
    double sampleRate_{44100.0};

    bool gate_{false};
    bool isAccent_{false};

    float vcfDecayTimeSec_{0.20f};
    float vcfAttackCoeff_{0.0f};
    float vcfDecayCoeff_{0.0f};

    float vcaAttackCoeff_{0.0f};
    float vcaGateHighDecayCoeff_{0.0f};
    float vcaQuickDrainCoeff_{0.0f};

    float accentChargeCoeff_{0.0f};
    float accentDischargeCoeff_{0.0f};
    float accentVcaCoeff_{0.0f};

    float vcfEnv_{0.0f};
    float vcfTarget_{0.0f};

    float vcaEnv_{0.0f};
    float vcaTarget_{0.0f};

    float accentCap_{0.0f};
    float accentVca_{0.0f};

    void updateCoefficients();
};

} // namespace syrebas

#endif // SYREBAS_ENVELOPE_HPP
