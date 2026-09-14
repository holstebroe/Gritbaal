#ifndef SYREBAS_CONTROL_RENDERER_HPP
#define SYREBAS_CONTROL_RENDERER_HPP

#include "IControlRenderer.hpp"

namespace syrebas {

class TB303ControlRenderer : public IControlRenderer {
public:
    TB303ControlRenderer() = default;
    ~TB303ControlRenderer() override = default;

    void drawKnob(Graphics& g, const Control& ctrl, const Font& font) override;
    void drawToggleSwitch(Graphics& g, const Control& ctrl, const Font& font) override;
};

} // namespace syrebas

#endif // SYREBAS_CONTROL_RENDERER_HPP
