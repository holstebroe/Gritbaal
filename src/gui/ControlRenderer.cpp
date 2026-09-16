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

IndustrialGritbaalRenderer::IndustrialGritbaalRenderer() = default;

static std::unique_ptr<OffscreenBuffer> createKnobCapSprite(int radius, int scale) {
    int dim = (radius + 4) * 2;
    auto buf = std::make_unique<OffscreenBuffer>(dim * scale, dim * scale);
    Graphics kg(buf->data(), dim, dim, scale);
    int cx = dim / 2;
    int cy = dim / 2;

    // 1. Drop Shadow & Outer Copper Trim Bezel
    kg.drawCircle(cx + 1, cy + 1, radius + 2, 0xFF08090A);
    kg.drawCircle(cx, cy, radius + 1, 0xFF8C5224);

    // 2. Heavy Gunmetal Knurled Skirt
    kg.drawCircle(cx, cy, radius, 0xFF282C30);
    kg.drawCircle(cx, cy, radius - 1, 0xFF3C4045);

    // Deep knurling ridges around edge
    for (int a = 0; a < 360; a += 20) {
        double rad = a * M_PI / 180.0;
        int rx1 = cx + static_cast<int>(std::cos(rad) * (radius - 4));
        int ry1 = cy + static_cast<int>(std::sin(rad) * (radius - 4));
        int rx2 = cx + static_cast<int>(std::cos(rad) * radius);
        int ry2 = cy + static_cast<int>(std::sin(rad) * radius);
        kg.drawLine(rx1, ry1, rx2, ry2, 0xFF121416, 1);
    }

    // 3. Conical Brushed Metal Face
    kg.drawCircle(cx, cy, radius - 4, 0xFF222528);
    kg.drawCircleOutline(cx, cy, radius - 4, 0xFF8C5224);
    kg.drawCircleOutline(cx, cy, radius - 6, 0xFF141618);

    // 4. Indicator Pointer facing UP (12 o'clock)
    kg.drawLine(cx, cy - 1, cx, cy - (radius - 2), 0xFFFF3300, 2);
    kg.drawCircle(cx, cy - (radius - 2), 1, 0xFFFFCC00);

    return buf;
}

void IndustrialGritbaalRenderer::ensureKnobSprites(int scale) {
    if (cachedScale_ == scale && knobCapSmall_) return;

    cachedScale_ = scale;
    knobCapSmall_ = createKnobCapSprite(18, scale);
    knobCapMedium_ = createKnobCapSprite(20, scale);
    knobCapLarge_ = createKnobCapSprite(24, scale);
}

const OffscreenBuffer* IndustrialGritbaalRenderer::getKnobSprite(int radius, int scale) {
    ensureKnobSprites(scale);
    if (radius <= 18) return knobCapSmall_.get();
    if (radius <= 20) return knobCapMedium_.get();
    return knobCapLarge_.get();
}

void IndustrialGritbaalRenderer::drawKnob(Graphics& g, const Control& knob, const Font& font) {
    drawKnobModulated(g, knob, font, knob.currentVal);
}

void IndustrialGritbaalRenderer::drawKnobModulated(Graphics& g, const Control& knob, const Font& font, double modValNorm) {
    // 1. Label centered above knob (Amber/Gold glow color on dark plate)
    int labelLen = static_cast<int>(strlen(knob.label));
    int labelX = knob.x - (labelLen * (font.getWidth() + 1)) / 2;
    g.drawText(labelX, knob.y - knob.radius - 18, knob.label, 0xFFD89A40, font, 1);

    // 2. Volcanic Amber Glow Arc around knob track
    double startAngle = 135.0 * M_PI / 180.0;
    double totalAngle = 270.0 * M_PI / 180.0;

    double normVal = (knob.currentVal - knob.minVal) / (knob.maxVal - knob.minVal);
    normVal = std::clamp(normVal, 0.0, 1.0);
    double activeAngle = startAngle + normVal * totalAngle;

    // Track background groove (recessed dark copper/steel)
    g.drawArc(knob.x, knob.y, knob.radius + 5, static_cast<float>(startAngle), static_cast<float>(startAngle + totalAngle), 0xFF2A180C, 3);
    g.drawArc(knob.x, knob.y, knob.radius + 5, static_cast<float>(startAngle), static_cast<float>(startAngle + totalAngle), 0xFF140B05, 1);

    // Base parameter value arc (Glowing Volcanic Amber/Orange)
    if (normVal > 0.01) {
        g.drawArc(knob.x, knob.y, knob.radius + 5, static_cast<float>(startAngle), static_cast<float>(activeAngle), 0xFFFF5500, 2);
    }

    // Realtime LFO/Envelope Modulation Arc (Outer Neon Cyan ring)
    double mNorm = std::clamp(modValNorm, 0.0, 1.0);
    double modAngle = startAngle + mNorm * totalAngle;
    g.drawArc(knob.x, knob.y, knob.radius + 8, static_cast<float>(startAngle), static_cast<float>(startAngle + totalAngle), 0xFF003040, 1);
    g.drawArc(knob.x, knob.y, knob.radius + 8, static_cast<float>(startAngle), static_cast<float>(modAngle), 0xFF00E5FF, 2);

    // Modulated tip indicator glowing dot
    int modTipX = knob.x + static_cast<int>(std::cos(modAngle) * (knob.radius + 8));
    int modTipY = knob.y + static_cast<int>(std::sin(modAngle) * (knob.radius + 8));
    g.drawCircle(modTipX, modTipY, 2, 0xFF00FFFF);

    // Min / Center / Max tick marks with copper/steel highlights
    int rIn = knob.radius + 4;
    int rOut = knob.radius + 7;
    int minX1 = knob.x + static_cast<int>(std::cos(startAngle) * rIn);
    int minY1 = knob.y + static_cast<int>(std::sin(startAngle) * rIn);
    int minX2 = knob.x + static_cast<int>(std::cos(startAngle) * rOut);
    int minY2 = knob.y + static_cast<int>(std::sin(startAngle) * rOut);
    g.drawLine(minX1, minY1, minX2, minY2, 0xFF8C5224, 1);

    double centerAngle = startAngle + totalAngle * 0.5;
    int cx1 = knob.x + static_cast<int>(std::cos(centerAngle) * rIn);
    int cy1 = knob.y + static_cast<int>(std::sin(centerAngle) * rIn);
    int cx2 = knob.x + static_cast<int>(std::cos(centerAngle) * rOut);
    int cy2 = knob.y + static_cast<int>(std::sin(centerAngle) * rOut);
    g.drawLine(cx1, cy1, cx2, cy2, 0xFF505458, 1);

    double endAngle = startAngle + totalAngle;
    int maxX1 = knob.x + static_cast<int>(std::cos(endAngle) * rIn);
    int maxY1 = knob.y + static_cast<int>(std::sin(endAngle) * rIn);
    int maxX2 = knob.x + static_cast<int>(std::cos(endAngle) * rOut);
    int maxY2 = knob.y + static_cast<int>(std::sin(endAngle) * rOut);
    g.drawLine(maxX1, maxY1, maxX2, maxY2, 0xFF8C5224, 1);

    // 3. Render pre-rendered rotated knob cap
    const OffscreenBuffer* knobBuf = getKnobSprite(knob.radius, g.getScale());
    if (knobBuf) {
        // Sprite pointer is created facing UP (12 o'clock, which is -M_PI / 2 in std math)
        // Rotate sprite by (activeAngle - (-M_PI / 2)) = activeAngle + M_PI / 2
        float rotationAngle = static_cast<float>(activeAngle + M_PI / 2.0);
        g.blitRotatedBuffer(knob.x, knob.y, *knobBuf, rotationAngle);
    }
}

void IndustrialGritbaalRenderer::drawToggleSwitch(Graphics& g, const Control& ctrl, const Font& font) {
    // Label above switch
    int labelLen = static_cast<int>(strlen(ctrl.label));
    int labelX = ctrl.x - (labelLen * (font.getWidth() + 1)) / 2;
    g.drawText(labelX, ctrl.y - 28, ctrl.label, 0xFFD89A40, font, 1);

    // Outer industrial dark iron frame box with copper trim
    g.drawRect(ctrl.x - 10, ctrl.y - 18, 20, 36, 0xFF0E1012);
    g.drawRectOutline(ctrl.x - 10, ctrl.y - 18, 20, 36, 0xFF8C5224, 1);
    g.drawRect(ctrl.x - 8, ctrl.y - 16, 16, 32, 0xFF181B1D);

    // Beveled switch slot recessed track
    g.drawRect(ctrl.x - 3, ctrl.y - 14, 6, 28, 0xFF0A0B0C);

    // Toggle handle (Chunky metallic copper/steel lever)
    bool state = (ctrl.currentVal >= 0.5);
    int handleY = state ? (ctrl.y - 13) : (ctrl.y + 1);

    g.drawRect(ctrl.x - 8, handleY, 16, 12, 0xFF0E1012);
    g.drawRect(ctrl.x - 7, handleY + 1, 14, 10, 0xFF8C5224);
    g.drawHorizontalGradient(ctrl.x - 6, handleY + 2, 12, 8, 0xFFD89A40, 0xFF8C5224);

    // Lever highlight ridge
    g.drawLine(ctrl.x - 5, handleY + 6, ctrl.x + 5, handleY + 6, 0xFFFFCC00, 1);

    // Dual status LEDs (top/bottom)
    drawLedIndicator(g, ctrl.x, ctrl.y - 11, !state, 0xFFFF8A00);
    drawLedIndicator(g, ctrl.x, ctrl.y + 11, state, 0xFFFF3300);
}

void IndustrialGritbaalRenderer::drawPushButton(Graphics& g, const Control& ctrl, const Font& font) {
    int labelLen = static_cast<int>(strlen(ctrl.label));
    int labelX = ctrl.x - (labelLen * (font.getWidth() + 1)) / 2;
    g.drawText(labelX, ctrl.y - 16, ctrl.label, 0xFFD89A40, font, 1);

    bool isPressed = (ctrl.currentVal >= 0.5);

    g.drawRect(ctrl.x - 14, ctrl.y - 8, 28, 16, 0xFF0E1012);
    g.drawRectOutline(ctrl.x - 14, ctrl.y - 8, 28, 16, 0xFF8C5224, 1);

    if (isPressed) {
        // Glowing illuminated active button state
        g.drawRect(ctrl.x - 12, ctrl.y - 6, 24, 12, 0xFFFF4500);
        g.drawRectOutline(ctrl.x - 12, ctrl.y - 6, 24, 12, 0xFFFFCC00, 1);
        g.drawRect(ctrl.x - 10, ctrl.y - 4, 20, 8, 0xFFFF8A00);
    } else {
        // Unpressed metallic cap
        g.drawVerticalGradient(ctrl.x - 12, ctrl.y - 6, 24, 12, 0xFF3A3D40, 0xFF202326);
        g.drawRectOutline(ctrl.x - 12, ctrl.y - 6, 24, 12, 0xFF141618, 1);
    }
}

void IndustrialGritbaalRenderer::drawLedIndicator(Graphics& g, int cx, int cy, bool state, uint32_t activeColor) {
    // Multi-stage radial ambient glow for illuminated state
    if (state) {
        g.drawCircle(cx, cy, 6, 0x33FF3300);
        g.drawCircle(cx, cy, 5, 0x66FF5500);
        g.drawCircle(cx, cy, 4, activeColor);
        g.drawCircle(cx, cy, 2, 0xFFFFCC00);
        g.drawRect(cx - 1, cy - 1, 1, 1, 0xFFFFFFFF);
    } else {
        g.drawCircle(cx, cy, 4, 0xFF0E1012);
        g.drawCircleOutline(cx, cy, 4, 0xFF2A2D30);
        g.drawCircle(cx, cy, 3, 0xFF200A04);
    }
}

} // namespace gritbaal
