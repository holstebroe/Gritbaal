#ifndef GRITBAAL_GRAPHICS_HPP
#define GRITBAAL_GRAPHICS_HPP

#include <cstdint>
#include <vector>
#include "Font.hpp"

namespace gritbaal {

class OffscreenBuffer {
public:
    OffscreenBuffer(int width, int height);

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    uint32_t* data() { return data_.data(); }
    const uint32_t* data() const { return data_.data(); }

    void clear(uint32_t color);

private:
    int width_{0};
    int height_{0};
    std::vector<uint32_t> data_;
};

class Graphics {
public:
    Graphics(uint32_t* buffer, int width, int height, int scale = 2);

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    int getScale() const { return scale_; }

    void clear(uint32_t color);

    void drawRect(int x, int y, int w, int h, uint32_t color);
    void drawRectOutline(int x, int y, int w, int h, uint32_t color, int thickness = 1);
    void drawVerticalGradient(int x, int y, int w, int h, uint32_t topColor, uint32_t bottomColor);
    void drawHorizontalGradient(int x, int y, int w, int h, uint32_t leftColor, uint32_t rightColor);

    void drawCircle(int cx, int cy, int radius, uint32_t color);
    void drawCircleOutline(int cx, int cy, int radius, uint32_t color);
    void drawLine(int x0, int y0, int x1, int y1, uint32_t color, int thickness = 1);

    void drawArc(int cx, int cy, int radius, float startAngleRad, float endAngleRad, uint32_t color, int thickness = 1);

    void drawRivet(int cx, int cy, int radius = 3);
    void drawPanelFrame(int x, int y, int w, int h, const char* title, const Font& font,
                         uint32_t traceSeed = 0, bool hexAccent = false);

    // Procedural background texture helpers (no external images/assets).
    void drawNoiseTexture(int x, int y, int w, int h, uint32_t baseColor, int variance, uint32_t seed);
    void drawCircuitTraces(int x, int y, int w, int h, uint32_t color, uint32_t seed, int count);
    void drawHexGridTexture(int x, int y, int w, int h, uint32_t color, int hexSize);

    void drawChar(int x, int y, char c, uint32_t color, const Font& font, int scale = 1);
    void drawText(int x, int y, const char* text, uint32_t color, const Font& font, int scale = 1);

    void blitBuffer(int dstX, int dstY, const OffscreenBuffer& srcBuffer);
    void blitRotatedBuffer(int dstCx, int dstCy, const OffscreenBuffer& srcBuffer, float angleRad);

    static uint32_t blendColors(uint32_t src, uint32_t dst, float alpha);

private:
    uint32_t* buffer_{nullptr};
    int width_{0};
    int height_{0};
    int scale_{2};
    int bufferWidth_{0};
    int bufferHeight_{0};
};

} // namespace gritbaal

#endif // GRITBAAL_GRAPHICS_HPP
