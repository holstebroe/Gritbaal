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

namespace syrebas {

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

} // namespace syrebas
