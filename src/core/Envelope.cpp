#include "Envelope.hpp"

namespace gritbaal {

Envelope::Envelope() {
    setSampleRate(44100.0);
}

void Envelope::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
    updateCoefficients();
}

void Envelope::setAttack(float attackSec) {
    attackSec_ = std::max(0.001f, attackSec);
    updateCoefficients();
}

void Envelope::setDecay(float decaySec) {
    decaySec_ = std::max(0.001f, decaySec);
    updateCoefficients();
}

void Envelope::setSustain(float sustainLevel) {
    sustainLevel_ = std::clamp(sustainLevel, 0.0f, 1.0f);
}

void Envelope::setRelease(float releaseSec) {
    releaseSec_ = std::max(0.001f, releaseSec);
    updateCoefficients();
}

void Envelope::updateCoefficients() {
    // 1 - exp(-1 / (sr * t)) for attack, exp(-1 / (sr * (t / 6.907755))) for decay/release t60
    attackCoeff_ = 1.0f - std::exp(-1.0f / static_cast<float>(sampleRate_ * attackSec_));
    decayCoeff_ = std::exp(-1.0f / static_cast<float>(sampleRate_ * (decaySec_ / 6.907755f)));
    releaseCoeff_ = std::exp(-1.0f / static_cast<float>(sampleRate_ * (releaseSec_ / 6.907755f)));
}

void Envelope::noteOn() {
    gate_ = true;
    state_ = State::Attack;
}

void Envelope::noteOff() {
    gate_ = false;
    state_ = State::Release;
}

void Envelope::processNextSample() {
    switch (state_) {
        case State::Idle:
            currentVal_ = 0.0f;
            break;

        case State::Attack:
            currentVal_ += attackCoeff_ * (1.05f - currentVal_);
            if (currentVal_ >= 1.0f) {
                currentVal_ = 1.0f;
                state_ = State::Decay;
            }
            break;

        case State::Decay:
            currentVal_ = sustainLevel_ + decayCoeff_ * (currentVal_ - sustainLevel_);
            if (std::abs(currentVal_ - sustainLevel_) < 0.0001f) {
                currentVal_ = sustainLevel_;
                state_ = State::Sustain;
            }
            break;

        case State::Sustain:
            currentVal_ = sustainLevel_;
            if (!gate_) {
                state_ = State::Release;
            }
            break;

        case State::Release:
            currentVal_ *= releaseCoeff_;
            if (currentVal_ < 0.0001f) {
                currentVal_ = 0.0f;
                state_ = State::Idle;
            }
            break;
    }
}

} // namespace gritbaal
