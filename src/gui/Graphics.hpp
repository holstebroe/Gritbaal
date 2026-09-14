#ifndef SYREBAS_GRAPHICS_HPP
#define SYREBAS_GRAPHICS_HPP

#include <cstdint>
#include "Font.hpp"

namespace syrebas {

class Graphics {
public:
    Graphics(uint32_t* buffer, int width, int height, int scale = 2);

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    int getScale() const { return scale_; }

    void clear(uint32_t color);

    void drawRect(int x, int y, int w, int h, uint32_t color);
    void drawCircle(int cx, int cy, int radius, uint32_t color);
    void drawCircleOutline(int cx, int cy, int radius, uint32_t color);
    void drawLine(int x0, int y0, int x1, int y1, uint32_t color, int thickness = 1);

    void drawChar(int x, int y, char c, uint32_t color, const Font& font, int scale = 1);
    void drawText(int x, int y, const char* text, uint32_t color, const Font& font, int scale = 1);

    static uint32_t blendColors(uint32_t src, uint32_t dst, float alpha);

private:
    uint32_t* buffer_{nullptr};
    int width_{0};
    int height_{0};
    int scale_{2};
    int bufferWidth_{0};
    int bufferHeight_{0};
};

} // namespace syrebas

#endif // SYREBAS_GRAPHICS_HPP
