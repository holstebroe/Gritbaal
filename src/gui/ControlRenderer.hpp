#ifndef GRITBAAL_CONTROL_RENDERER_HPP
#define GRITBAAL_CONTROL_RENDERER_HPP

#include "IControlRenderer.hpp"

namespace gritbaal {

class TB303ControlRenderer : public IControlRenderer {
public:
    TB303ControlRenderer() = default;
    ~TB303ControlRenderer() override = default;

    void drawKnob(Graphics& g, const Control& knob, const Font& font) override;
    void drawKnobModulated(Graphics& g, const Control& knob, const Font& font, double modValNorm) override {
        drawKnob(g, knob, font);
    }
    void drawToggleSwitch(Graphics& g, const Control& ctrl, const Font& font) override;
    void drawPushButton(Graphics& g, const Control& ctrl, const Font& font) override;
    void drawLedIndicator(Graphics& g, int cx, int cy, bool state, uint32_t activeColor = 0xFFFF0000) override;
};

class IndustrialGritbaalRenderer : public IControlRenderer {
public:
    IndustrialGritbaalRenderer() = default;
    ~IndustrialGritbaalRenderer() override = default;

    void drawKnob(Graphics& g, const Control& knob, const Font& font) override;
    void drawKnobModulated(Graphics& g, const Control& knob, const Font& font, double modValNorm) override;
    void drawToggleSwitch(Graphics& g, const Control& ctrl, const Font& font) override;
    void drawPushButton(Graphics& g, const Control& ctrl, const Font& font) override;
    void drawLedIndicator(Graphics& g, int cx, int cy, bool state, uint32_t activeColor = 0xFFFF3300) override;
};

} // namespace gritbaal

#endif // GRITBAAL_CONTROL_RENDERER_HPP
