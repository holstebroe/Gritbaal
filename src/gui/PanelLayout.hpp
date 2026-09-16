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
        if (!cachedBackground_) {
            createCachedBackground(g.getScale(), font);
        }
        if (cachedBackground_) {
            g.blitBuffer(0, 0, *cachedBackground_);
        }
    }

    void invalidateCache() {
        cachedBackground_.reset();
    }

private:
    void createCachedBackground(int scale, const Font& font) const {
        cachedBackground_ = std::make_unique<OffscreenBuffer>(totalWidth_ * scale, totalHeight_ * scale);
        Graphics bgGraphics(cachedBackground_->data(), totalWidth_, totalHeight_, scale);

        // Main synth chassis background: Forged dark iron/gunmetal with subtle gradient
        bgGraphics.drawVerticalGradient(0, 0, totalWidth_, totalHeight_, 0xFF181A1D, 0xFF101113);

        // Header and chassis border trim
        bgGraphics.drawRect(0, 0, totalWidth_, 22, 0xFF0A0B0C);
        bgGraphics.drawRectOutline(0, 0, totalWidth_, totalHeight_, 0xFF2A2D30, 2);
        bgGraphics.drawRectOutline(2, 2, totalWidth_ - 4, totalHeight_ - 4, 0xFF8C5224, 1);

        // Render each modular panel section into the background cache
        for (const auto& panel : panels_) {
            bgGraphics.drawPanelFrame(panel.x, panel.y, panel.width, panel.height, panel.title.c_str(), font);
        }
    }

private:
    int totalWidth_;
    int totalHeight_;
    std::vector<PanelRegion> panels_;
    mutable std::unique_ptr<OffscreenBuffer> cachedBackground_;
};

} // namespace gritbaal

#endif // GRITBAAL_PANEL_LAYOUT_HPP
