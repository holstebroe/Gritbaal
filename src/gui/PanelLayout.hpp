#ifndef GRITBAAL_PANEL_LAYOUT_HPP
#define GRITBAAL_PANEL_LAYOUT_HPP

#include <vector>
#include <string>
#include "Graphics.hpp"
#include "Font.hpp"

namespace gritbaal {

struct PanelRegion {
    std::string title;
    int x, y, width, height;
};

class PanelLayout {
public:
    PanelLayout(int totalWidth, int totalHeight)
        : totalWidth_(totalWidth), totalHeight_(totalHeight) {}

    int getTotalWidth() const { return totalWidth_; }
    int getTotalHeight() const { return totalHeight_; }

    void addPanel(const std::string& title, int x, int y, int width, int height) {
        panels_.push_back({title, x, y, width, height});
    }

    void drawLayout(Graphics& g, const Font& font) const {
        // Main synth chassis background: Forged dark iron/gunmetal with a subtle
        // brushed-metal grain so the chassis isn't a flat, lifeless fill.
        g.clear(0xFF141517);
        g.drawNoiseTexture(0, 0, totalWidth_, totalHeight_, 0xFF141517, 3, 0xC0FFEE);

        // Header and chassis border trim
        g.drawRect(0, 0, totalWidth_, 16, 0xFF0E0F10);
        g.drawRectOutline(0, 0, totalWidth_, totalHeight_, 0xFF2A2D30, 2);
        g.drawRectOutline(2, 2, totalWidth_ - 4, totalHeight_ - 4, 0xFF8C5224, 1);

        // Render each modular panel section. Every panel gets its own faint,
        // deterministic circuit-trace linework (seeded from its position so it
        // never changes between frames); the thermal/drift panel additionally
        // gets a cellular hex-grid accent, evoking heat-sink/insulation texture.
        for (const auto& panel : panels_) {
            uint32_t seed = static_cast<uint32_t>(panel.x) * 733u + static_cast<uint32_t>(panel.y) * 917u + 1u;
            bool hexAccent = panel.title.find("DRIFT") != std::string::npos;
            g.drawPanelFrame(panel.x, panel.y, panel.width, panel.height, panel.title.c_str(), font, seed, hexAccent);
        }
    }

private:
    int totalWidth_;
    int totalHeight_;
    std::vector<PanelRegion> panels_;
};

} // namespace gritbaal

#endif // GRITBAAL_PANEL_LAYOUT_HPP
