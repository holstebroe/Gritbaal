#ifndef GRITBAAL_CONTROL_RENDERER_HPP
#define GRITBAAL_CONTROL_RENDERER_HPP

#include "IControlRenderer.hpp"

namespace gritbaal {

class TB303ControlRenderer : public IControlRenderer {
public:
    TB303ControlRenderer() = default;
    ~TB303ControlRenderer() override = default;

    void drawKnob(Graphics& g, const Control& ctrl, const Font& font) override;
    void drawToggleSwitch(Graphics& g, const Control& ctrl, const Font& font) override;
};

} // namespace gritbaal

#endif // GRITBAAL_CONTROL_RENDERER_HPP
