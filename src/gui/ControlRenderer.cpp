#include "ControlRenderer.hpp"
#include "Graphics.hpp"
#include "Font.hpp"
#include "GuiWindow.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace gritbaal {

void TB303ControlRenderer::drawKnob(Graphics& g, const Control& knob, const Font& font) {
    // Label centered above knob
    int labelLen = static_cast<int>(strlen(knob.label));
    int labelX = knob.x - (labelLen * (font.getWidth() + 1)) / 2;
    g.drawText(labelX, knob.y - knob.radius - 28, knob.label, 0xFF101010, font, 1);

    // Circular dial tick marks
    int numTicks = 11;
    double startAngle = 135.0 * M_PI / 180.0; // 7 o'clock
    double totalAngle = 270.0 * M_PI / 180.0; // Clockwise to 5 o'clock

    for (int i = 0; i < numTicks; ++i) {
        double norm = static_cast<double>(i) / (numTicks - 1);
        double angle = startAngle + norm * totalAngle;

        int rIn = knob.radius + 4;
        int rOut = knob.radius + 8;

        int x1 = knob.x + static_cast<int>(std::cos(angle) * rIn);
        int y1 = knob.y + static_cast<int>(std::sin(angle) * rIn);
        int x2 = knob.x + static_cast<int>(std::cos(angle) * rOut);
        int y2 = knob.y + static_cast<int>(std::sin(angle) * rOut);

        g.drawLine(x1, y1, x2, y2, 0xFF202020, 1);

        // 12 o'clock tick mark (i == 5) gets the iconic 303 black square above it
        if (i == 5) {
            g.drawRect(knob.x - 2, knob.y - knob.radius - 14, 4, 4, 0xFF101010);
        }
    }

    // Outer shadow / bezel
    g.drawCircle(knob.x + 1, knob.y + 1, knob.radius + 2, 0xFF888A8C);
    g.drawCircle(knob.x, knob.y, knob.radius + 1, 0xFF202020);

    // Knob body (Metallic fluted silver)
    g.drawCircle(knob.x, knob.y, knob.radius, 0xFF808488);
    g.drawCircle(knob.x, knob.y, knob.radius - 1, 0xFFB4B8BC);

    // Fluted ridges around skirt
    for (int a = 0; a < 360; a += 30) {
        double rad = a * M_PI / 180.0;
        int rx1 = knob.x + static_cast<int>(std::cos(rad) * (knob.radius - 4));
        int ry1 = knob.y + static_cast<int>(std::sin(rad) * (knob.radius - 4));
        int rx2 = knob.x + static_cast<int>(std::cos(rad) * knob.radius);
        int ry2 = knob.y + static_cast<int>(std::sin(rad) * knob.radius);
        g.drawLine(rx1, ry1, rx2, ry2, 0xFF606468, 1);
    }

    // Conical top face
    g.drawCircle(knob.x, knob.y, knob.radius - 4, 0xFFD4D8DC);
    g.drawCircleOutline(knob.x, knob.y, knob.radius - 4, 0xFF909498);

    // Pointer indicator line
    double normVal = (knob.currentVal - knob.minVal) / (knob.maxVal - knob.minVal);
    normVal = (std::min)((std::max)(normVal, 0.0), 1.0);
    double ptrAngle = startAngle + normVal * totalAngle;

    int ptrX = knob.x + static_cast<int>(std::cos(ptrAngle) * (knob.radius - 3));
    int ptrY = knob.y + static_cast<int>(std::sin(ptrAngle) * (knob.radius - 3));

    g.drawLine(knob.x, knob.y, ptrX, ptrY, 0xFF101010, 2);
    g.drawCircle(ptrX, ptrY, 1, 0xFF101010);
}

void TB303ControlRenderer::drawModeSelector(Graphics& g, const Control& ctrl, const Font& font) {
    int idx = std::clamp(static_cast<int>(ctrl.currentVal + 0.5), 0, static_cast<int>(ctrl.options.size()) - 1);
    const char* text = (idx >= 0 && idx < static_cast<int>(ctrl.options.size())) ? ctrl.options[idx].c_str() : "";

    g.drawRect(ctrl.x - 26, ctrl.y - 8, 52, 16, 0xFF101010);
    g.drawRectOutline(ctrl.x - 26, ctrl.y - 8, 52, 16, 0xFF808488, 1);
    int textLen = static_cast<int>(strlen(text));
    int textX = ctrl.x - (textLen * (font.getWidth() + 1)) / 2;
    g.drawText(textX, ctrl.y - 4, text, 0xFFE0E4E8, font, 1);
}

void TB303ControlRenderer::drawToggleSwitch(Graphics& g, const Control& ctrl, const Font& font) {
    // Label above switch
    int labelLen = static_cast<int>(strlen(ctrl.label));
    int labelX = ctrl.x - (labelLen * (font.getWidth() + 1)) / 2;
    g.drawText(labelX, ctrl.y - 35, ctrl.label, 0xFF101010, font, 1);

    // SAW / SQUARE labels beside positions
    g.drawText(ctrl.x - 28, ctrl.y - 18, "SQR", 0xFF202020, font, 1);
    g.drawText(ctrl.x - 28, ctrl.y + 10, "SAW", 0xFF202020, font, 1);

    // Outer metal frame box
    g.drawRect(ctrl.x - 8, ctrl.y - 20, 16, 40, 0xFF202020);
    g.drawRect(ctrl.x - 7, ctrl.y - 19, 14, 38, 0xFF888C90);
    g.drawRect(ctrl.x - 5, ctrl.y - 17, 10, 34, 0xFF181818);

    // Toggle handle
    bool isSquare = (ctrl.currentVal >= 0.5);
    int handleY = isSquare ? (ctrl.y - 16) : (ctrl.y + 2);

    g.drawRect(ctrl.x - 7, handleY, 14, 14, 0xFF303030);
    g.drawRect(ctrl.x - 6, handleY + 1, 12, 12, 0xFFE0E4E8);
    g.drawRect(ctrl.x - 4, handleY + 3, 8, 8, 0xFFB0B4B8);
    g.drawLine(ctrl.x - 5, handleY + 7, ctrl.x + 5, handleY + 7, 0xFF101010, 1);
}

void TB303ControlRenderer::drawPushButton(Graphics& g, const Control& ctrl, const Font& font) {
    int labelLen = static_cast<int>(strlen(ctrl.label));
    int labelX = ctrl.x - (labelLen * (font.getWidth() + 1)) / 2;
    g.drawText(labelX, ctrl.y - 18, ctrl.label, 0xFF101010, font, 1);

    bool isPressed = (ctrl.currentVal >= 0.5);
    uint32_t btnColor = isPressed ? 0xFF808488 : 0xFFC0C4C8;
    g.drawRect(ctrl.x - 10, ctrl.y - 6, 20, 12, 0xFF202020);
    g.drawRect(ctrl.x - 9, ctrl.y - 5, 18, 10, btnColor);
}

void TB303ControlRenderer::drawLedIndicator(Graphics& g, int cx, int cy, bool state, uint32_t activeColor) {
    g.drawCircle(cx, cy, 4, 0xFF202020);
    uint32_t col = state ? activeColor : 0xFF401010;
    g.drawCircle(cx, cy, 3, col);
}

// ==========================================================
// IndustrialGritbaalRenderer Implementation
// ==========================================================

void IndustrialGritbaalRenderer::drawKnob(Graphics& g, const Control& knob, const Font& font) {
    drawKnobModulated(g, knob, font, knob.currentVal);
}

void IndustrialGritbaalRenderer::drawKnobModulated(Graphics& g, const Control& knob, const Font& font, double modValNorm) {
    // 1. Label centered above knob (if non-empty)
    if (knob.label && strlen(knob.label) > 0) {
        int labelLen = static_cast<int>(strlen(knob.label));
        int labelX = knob.x - (labelLen * (font.getWidth() + 1)) / 2;
        g.drawText(labelX, knob.y - knob.radius - 18, knob.label, 0xFFD89A40, font, 1);
    }

    // 2. Volcanic Amber Glow Arc around knob track
    double startAngle = 135.0 * M_PI / 180.0;
    double totalAngle = 270.0 * M_PI / 180.0;

    double normVal = (knob.currentVal - knob.minVal) / (knob.maxVal - knob.minVal);
    normVal = std::clamp(normVal, 0.0, 1.0);
    double activeAngle = startAngle + normVal * totalAngle;

    // Background track arc (Dark Copper)
    g.drawArc(knob.x, knob.y, knob.radius + 5, static_cast<float>(startAngle), static_cast<float>(startAngle + totalAngle), 0xFF3A2010, 2);

    if (knob.isBipolar) {
        double centerAngle = startAngle + 0.5 * totalAngle;
        if (normVal > 0.501) {
            g.drawArc(knob.x, knob.y, knob.radius + 5, static_cast<float>(centerAngle), static_cast<float>(activeAngle), 0xFFFF4500, 2);
        } else if (normVal < 0.499) {
            g.drawArc(knob.x, knob.y, knob.radius + 5, static_cast<float>(activeAngle), static_cast<float>(centerAngle), 0xFFFF4500, 2);
        }
        int rIn = knob.radius + 4;
        int rOut = knob.radius + 7;
        int midX1 = knob.x + static_cast<int>(std::cos(centerAngle) * rIn);
        int midY1 = knob.y + static_cast<int>(std::sin(centerAngle) * rIn);
        int midX2 = knob.x + static_cast<int>(std::cos(centerAngle) * rOut);
        int midY2 = knob.y + static_cast<int>(std::sin(centerAngle) * rOut);
        g.drawLine(midX1, midY1, midX2, midY2, 0xFFFF8A00, 1);
    } else {
        if (normVal > 0.01) {
            g.drawArc(knob.x, knob.y, knob.radius + 5, static_cast<float>(startAngle), static_cast<float>(activeAngle), 0xFFFF4500, 2);
        }
    }

    // Second Arc: Realtime Modulated Position (Bright Neon Cyan/Yellow glow)
    double mNorm = std::clamp(modValNorm, 0.0, 1.0);
    double modAngle = startAngle + mNorm * totalAngle;
    g.drawArc(knob.x, knob.y, knob.radius + 8, static_cast<float>(startAngle), static_cast<float>(modAngle), 0xFF00E5FF, 2);

    // Tick marks at minimum and maximum
    int rIn = knob.radius + 4;
    int rOut = knob.radius + 7;
    int minX1 = knob.x + static_cast<int>(std::cos(startAngle) * rIn);
    int minY1 = knob.y + static_cast<int>(std::sin(startAngle) * rIn);
    int minX2 = knob.x + static_cast<int>(std::cos(startAngle) * rOut);
    int minY2 = knob.y + static_cast<int>(std::sin(startAngle) * rOut);
    g.drawLine(minX1, minY1, minX2, minY2, 0xFF8C5224, 1);

    double endAngle = startAngle + totalAngle;
    int maxX1 = knob.x + static_cast<int>(std::cos(endAngle) * rIn);
    int maxY1 = knob.y + static_cast<int>(std::sin(endAngle) * rIn);
    int maxX2 = knob.x + static_cast<int>(std::cos(endAngle) * rOut);
    int maxY2 = knob.y + static_cast<int>(std::sin(endAngle) * rOut);
    g.drawLine(maxX1, maxY1, maxX2, maxY2, 0xFF8C5224, 1);

    // 3. Knob Outer Bezel (Copper / Heavy Dark Steel)
    g.drawCircle(knob.x + 1, knob.y + 1, knob.radius + 2, 0xFF0A0B0C);
    g.drawCircle(knob.x, knob.y, knob.radius + 1, 0xFF8C5224);

    // 4. Knob Body (Industrial Dark Gunmetal & Heavy Knurling)
    g.drawCircle(knob.x, knob.y, knob.radius, 0xFF24272A);
    g.drawCircle(knob.x, knob.y, knob.radius - 1, 0xFF383C40);

    // Deep heavy knurled teeth around skirt
    for (int a = 0; a < 360; a += 20) {
        double rad = a * M_PI / 180.0;
        int rx1 = knob.x + static_cast<int>(std::cos(rad) * (knob.radius - 4));
        int ry1 = knob.y + static_cast<int>(std::sin(rad) * (knob.radius - 4));
        int rx2 = knob.x + static_cast<int>(std::cos(rad) * knob.radius);
        int ry2 = knob.y + static_cast<int>(std::sin(rad) * knob.radius);
        g.drawLine(rx1, ry1, rx2, ry2, 0xFF141618, 1);
    }

    // 5. Conical Top Face (Brushed Gunmetal & Copper Inner Ring)
    g.drawCircle(knob.x, knob.y, knob.radius - 4, 0xFF202326);
    g.drawCircleOutline(knob.x, knob.y, knob.radius - 4, 0xFF8C5224);

    // 6. Glowing Red/Amber Pointer Line
    int ptrX = knob.x + static_cast<int>(std::cos(activeAngle) * (knob.radius - 3));
    int ptrY = knob.y + static_cast<int>(std::sin(activeAngle) * (knob.radius - 3));

    g.drawLine(knob.x, knob.y, ptrX, ptrY, 0xFFFF3300, 2);
    g.drawCircle(ptrX, ptrY, 1, 0xFFFFCC00);
}

void IndustrialGritbaalRenderer::drawModeSelector(Graphics& g, const Control& ctrl, const Font& font) {
    int idx = std::clamp(static_cast<int>(ctrl.currentVal + 0.5), 0, static_cast<int>(ctrl.options.size()) - 1);
    const char* text = (idx >= 0 && idx < static_cast<int>(ctrl.options.size())) ? ctrl.options[idx].c_str() : "";

    int w = 54;
    int h = 16;
    int bx = ctrl.x - w / 2;
    int by = ctrl.y - h / 2;

    g.drawRect(bx, by, w, h, 0xFF0E1012);
    g.drawRectOutline(bx, by, w, h, 0xFF8C5224, 1);

    int textLen = static_cast<int>(strlen(text));
    int textX = ctrl.x - (textLen * (font.getWidth() + 1)) / 2;
    g.drawText(textX, ctrl.y - 3, text, 0xFFFFCC00, font, 1);
}

void IndustrialGritbaalRenderer::drawToggleSwitch(Graphics& g, const Control& ctrl, const Font& font) {
    // Label above switch
    int labelLen = static_cast<int>(strlen(ctrl.label));
    int labelX = ctrl.x - (labelLen * (font.getWidth() + 1)) / 2;
    g.drawText(labelX, ctrl.y - 28, ctrl.label, 0xFFD89A40, font, 1);

    // Outer industrial dark iron frame box with copper trim
    g.drawRect(ctrl.x - 10, ctrl.y - 18, 20, 36, 0xFF101214);
    g.drawRectOutline(ctrl.x - 10, ctrl.y - 18, 20, 36, 0xFF8C5224, 1);
    g.drawRect(ctrl.x - 7, ctrl.y - 15, 14, 30, 0xFF181B1D);

    // Toggle handle (Chunky metallic copper/steel lever)
    bool state = (ctrl.currentVal >= 0.5);
    int handleY = state ? (ctrl.y - 13) : (ctrl.y + 1);

    g.drawRect(ctrl.x - 8, handleY, 16, 12, 0xFF0E1012);
    g.drawRect(ctrl.x - 7, handleY + 1, 14, 10, 0xFF8C5224);
    g.drawRect(ctrl.x - 5, handleY + 3, 10, 6, 0xFFD89A40);

    // Glowing LED position indicator
    drawLedIndicator(g, ctrl.x, state ? (ctrl.y + 10) : (ctrl.y - 10), true, state ? 0xFFFF3300 : 0xFFFF8A00);
}

void IndustrialGritbaalRenderer::drawPushButton(Graphics& g, const Control& ctrl, const Font& font) {
    int labelLen = static_cast<int>(strlen(ctrl.label));
    int labelX = ctrl.x - (labelLen * (font.getWidth() + 1)) / 2;
    g.drawText(labelX, ctrl.y - 16, ctrl.label, 0xFFD89A40, font, 1);

    bool isPressed = (ctrl.currentVal >= 0.5);
    uint32_t btnColor = isPressed ? 0xFFFF4500 : 0xFF2A2D30;

    g.drawRect(ctrl.x - 14, ctrl.y - 8, 28, 16, 0xFF101214);
    g.drawRectOutline(ctrl.x - 14, ctrl.y - 8, 28, 16, 0xFF8C5224, 1);
    g.drawRect(ctrl.x - 12, ctrl.y - 6, 24, 12, btnColor);

    if (isPressed) {
        g.drawRectOutline(ctrl.x - 12, ctrl.y - 6, 24, 12, 0xFFFFCC00, 1);
    }
}

void IndustrialGritbaalRenderer::drawLedIndicator(Graphics& g, int cx, int cy, bool state, uint32_t activeColor) {
    g.drawCircle(cx, cy, 4, 0xFF101214);
    g.drawCircleOutline(cx, cy, 4, 0xFF383C40);
    uint32_t col = state ? activeColor : 0xFF301008;
    g.drawCircle(cx, cy, 3, col);
    if (state) {
        g.drawRect(cx - 1, cy - 1, 1, 1, 0xFFFFFFFF);
    }
}

} // namespace gritbaal
