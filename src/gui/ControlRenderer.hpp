#ifndef GRITBAAL_CONTROL_RENDERER_HPP
#define GRITBAAL_CONTROL_RENDERER_HPP

#include <memory>
#include "IControlRenderer.hpp"
#include "Graphics.hpp"

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
    IndustrialGritbaalRenderer();
    ~IndustrialGritbaalRenderer() override = default;

    void drawKnob(Graphics& g, const Control& knob, const Font& font) override;
    void drawKnobModulated(Graphics& g, const Control& knob, const Font& font, double modValNorm) override;
    void drawToggleSwitch(Graphics& g, const Control& ctrl, const Font& font) override;
    void drawPushButton(Graphics& g, const Control& ctrl, const Font& font) override;
    void drawLedIndicator(Graphics& g, int cx, int cy, bool state, uint32_t activeColor = 0xFFFF3300) override;

private:
    void ensureKnobSprites(int scale);
    const OffscreenBuffer* getKnobSprite(int radius, int scale);

    std::unique_ptr<OffscreenBuffer> knobCapSmall_;  // radius 18
    std::unique_ptr<OffscreenBuffer> knobCapMedium_; // radius 20
    std::unique_ptr<OffscreenBuffer> knobCapLarge_;  // radius 24
    int cachedScale_{0};
};

} // namespace gritbaal

#endif // GRITBAAL_CONTROL_RENDERER_HPP
