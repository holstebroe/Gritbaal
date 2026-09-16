#ifndef GRITBAAL_ICONTROL_RENDERER_HPP
#define GRITBAAL_ICONTROL_RENDERER_HPP

#include <cstdint>

namespace gritbaal {

class Graphics;
class Font;
struct Control;

class IControlRenderer {
public:
    virtual ~IControlRenderer() = default;

    virtual void drawKnob(Graphics& g, const Control& knob, const Font& font) = 0;
    virtual void drawKnobModulated(Graphics& g, const Control& knob, const Font& font, double modValNorm, bool isTargetHighlight = false) = 0;
    virtual void drawModeSelector(Graphics& g, const Control& ctrl, const Font& font, bool isSelectingTarget = false) = 0;
    virtual void drawToggleSwitch(Graphics& g, const Control& ctrl, const Font& font) = 0;
    virtual void drawPushButton(Graphics& g, const Control& ctrl, const Font& font) = 0;
    virtual void drawLedIndicator(Graphics& g, int cx, int cy, bool state, uint32_t activeColor = 0xFFFF0000) = 0;
};

} // namespace gritbaal

#endif // GRITBAAL_ICONTROL_RENDERER_HPP
