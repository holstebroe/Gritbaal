#ifndef GRITBAAL_INDUSTRIAL_GRITBAAL_RENDERER_HPP
#define GRITBAAL_INDUSTRIAL_GRITBAAL_RENDERER_HPP

#include "IControlRenderer.hpp"

namespace gritbaal {

class IndustrialGritbaalRenderer : public IControlRenderer {
public:
    IndustrialGritbaalRenderer() = default;
    ~IndustrialGritbaalRenderer() override = default;

    void drawKnob(Graphics& g, const Control& knob, const Font& font) override;
    void drawKnobModulated(Graphics& g, const Control& knob, const Font& font, double modValNorm) override;
    void drawModeSelector(Graphics& g, const Control& ctrl, const Font& font) override;
    void drawToggleSwitch(Graphics& g, const Control& ctrl, const Font& font) override;
    void drawPushButton(Graphics& g, const Control& ctrl, const Font& font) override;
    void drawLedIndicator(Graphics& g, int cx, int cy, bool state, uint32_t activeColor = 0xFFFF3300) override;
};

} // namespace gritbaal

#endif // GRITBAAL_INDUSTRIAL_GRITBAAL_RENDERER_HPP
