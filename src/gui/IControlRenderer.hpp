#ifndef SYREBAS_I_CONTROL_RENDERER_HPP
#define SYREBAS_I_CONTROL_RENDERER_HPP

namespace syrebas {

class Graphics;
class Font;
struct Control;

class IControlRenderer {
public:
    virtual ~IControlRenderer() = default;

    virtual void drawKnob(Graphics& g, const Control& ctrl, const Font& font) = 0;
    virtual void drawToggleSwitch(Graphics& g, const Control& ctrl, const Font& font) = 0;
};

} // namespace syrebas

#endif // SYREBAS_I_CONTROL_RENDERER_HPP
