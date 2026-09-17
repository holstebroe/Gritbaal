#include "Graphics.hpp"
#include "core/MathConstants.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>

#ifndef M_PI
#define M_PI gritbaal::kPI
#endif

namespace gritbaal {

OffscreenBuffer::OffscreenBuffer(int width, int height)
    : width_(width), height_(height), data_(width * height, 0) {}

void OffscreenBuffer::clear(uint32_t color) {
    std::fill(data_.begin(), data_.end(), color);
}

Graphics::Graphics(uint32_t* buffer, int width, int height, int scale)
    : buffer_(buffer), width_(width), height_(height), scale_(scale),
      bufferWidth_(width * scale), bufferHeight_(height * scale) {}

void Graphics::clear(uint32_t color) {
    if (!buffer_) return;
    int size = bufferWidth_ * bufferHeight_;
    std::fill(buffer_, buffer_ + size, color);
}

uint32_t Graphics::blendColors(uint32_t src, uint32_t dst, float alpha) {
    if (alpha <= 0.0f) return dst;
    if (alpha >= 1.0f) return src;

    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >> 8) & 0xFF;
    uint32_t sb = src & 0xFF;

    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >> 8) & 0xFF;
    uint32_t db = dst & 0xFF;

    uint32_t r = static_cast<uint32_t>(sr * alpha + dr * (1.0f - alpha) + 0.5f);
    uint32_t g = static_cast<uint32_t>(sg * alpha + dg * (1.0f - alpha) + 0.5f);
    uint32_t b = static_cast<uint32_t>(sb * alpha + db * (1.0f - alpha) + 0.5f);

    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

void Graphics::drawRect(int x, int y, int w, int h, uint32_t color) {
    if (!buffer_) return;
    uint32_t alpha = (color >> 24) & 0xFF;
    if (alpha == 0) return;

    int xStart = (std::max)(0, x * scale_);
    int yStart = (std::max)(0, y * scale_);
    int xEnd = (std::min)(bufferWidth_, (x + w) * scale_);
    int yEnd = (std::min)(bufferHeight_, (y + h) * scale_);

    if (alpha == 0xFF) {
        for (int py = yStart; py < yEnd; ++py) {
            for (int px = xStart; px < xEnd; ++px) {
                buffer_[py * bufferWidth_ + px] = color;
            }
        }
    } else {
        float a = alpha / 255.0f;
        uint32_t opaqueColor = 0xFF000000 | (color & 0x00FFFFFF);
        for (int py = yStart; py < yEnd; ++py) {
            for (int px = xStart; px < xEnd; ++px) {
                int idx = py * bufferWidth_ + px;
                buffer_[idx] = blendColors(opaqueColor, buffer_[idx], a);
            }
        }
    }
}

void Graphics::drawRectOutline(int x, int y, int w, int h, uint32_t color, int thickness) {
    drawRect(x, y, w, thickness, color);
    drawRect(x, y + h - thickness, w, thickness, color);
    drawRect(x, y + thickness, thickness, h - 2 * thickness, color);
    drawRect(x + w - thickness, y + thickness, thickness, h - 2 * thickness, color);
}

void Graphics::drawVerticalGradient(int x, int y, int w, int h, uint32_t topColor, uint32_t bottomColor) {
    if (h <= 0) return;
    for (int i = 0; i < h; ++i) {
        float alpha = static_cast<float>(i) / static_cast<float>(h - 1);
        uint32_t blended = blendColors(bottomColor, topColor, alpha);
        drawRect(x, y + i, w, 1, blended);
    }
}

void Graphics::drawHorizontalGradient(int x, int y, int w, int h, uint32_t leftColor, uint32_t rightColor) {
    if (w <= 0) return;
    for (int i = 0; i < w; ++i) {
        float alpha = static_cast<float>(i) / static_cast<float>(w - 1);
        uint32_t blended = blendColors(rightColor, leftColor, alpha);
        drawRect(x + i, y, 1, h, blended);
    }
}

void Graphics::drawCircle(int cx, int cy, int radius, uint32_t color) {
    if (!buffer_) return;
    float scaleF = static_cast<float>(scale_);
    float cx2 = cx * scaleF + (scaleF * 0.5f);
    float cy2 = cy * scaleF + (scaleF * 0.5f);
    float r2 = radius * scaleF;
    float rOuter = r2 + 0.75f;
    float rInner = r2 - 0.75f;
    float rOuterSq = rOuter * rOuter;
    float rInnerSq = (rInner > 0.0f) ? (rInner * rInner) : -1.0f;

    int minY = (std::max)(0, static_cast<int>(cy2 - rOuter - 1.0f));
    int maxY = (std::min)(bufferHeight_ - 1, static_cast<int>(cy2 + rOuter + 1.0f));

    for (int py = minY; py <= maxY; ++py) {
        float dy = static_cast<float>(py) - cy2;
        float dy2 = dy * dy;
        if (dy2 > rOuterSq) continue;

        float dxMax = std::sqrt(rOuterSq - dy2);
        int xStart = (std::max)(0, static_cast<int>(std::floor(cx2 - dxMax)));
        int xEnd = (std::min)(bufferWidth_ - 1, static_cast<int>(std::ceil(cx2 + dxMax)));

        if (rInnerSq > 0.0f && dy2 < rInnerSq) {
            float dxSolid = std::sqrt(rInnerSq - dy2);
            int xSolidStart = static_cast<int>(std::ceil(cx2 - dxSolid));
            int xSolidEnd = static_cast<int>(std::floor(cx2 + dxSolid));

            xSolidStart = (std::clamp)(xSolidStart, xStart, xEnd + 1);
            xSolidEnd = (std::clamp)(xSolidEnd, xStart - 1, xEnd);

            // Left anti-aliased edge span
            for (int px = xStart; px < xSolidStart; ++px) {
                float dx = static_cast<float>(px) - cx2;
                float dist = std::hypot(dx, dy);
                if (dist < rOuter) {
                    float alpha = (rOuter - dist) / 1.5f;
                    if (alpha > 0.0f) {
                        int idx = py * bufferWidth_ + px;
                        buffer_[idx] = blendColors(color, buffer_[idx], alpha);
                    }
                }
            }

            // Solid inner span (direct pixel assignment)
            for (int px = xSolidStart; px <= xSolidEnd; ++px) {
                buffer_[py * bufferWidth_ + px] = color;
            }

            // Right anti-aliased edge span
            for (int px = xSolidEnd + 1; px <= xEnd; ++px) {
                float dx = static_cast<float>(px) - cx2;
                float dist = std::hypot(dx, dy);
                if (dist < rOuter) {
                    float alpha = (rOuter - dist) / 1.5f;
                    if (alpha > 0.0f) {
                        int idx = py * bufferWidth_ + px;
                        buffer_[idx] = blendColors(color, buffer_[idx], alpha);
                    }
                }
            }
        } else {
            for (int px = xStart; px <= xEnd; ++px) {
                float dx = static_cast<float>(px) - cx2;
                float dist = std::hypot(dx, dy);
                if (dist < rOuter) {
                    float alpha = (dist <= rInner) ? 1.0f : ((rOuter - dist) / 1.5f);
                    if (alpha > 0.0f) {
                        int idx = py * bufferWidth_ + px;
                        buffer_[idx] = blendColors(color, buffer_[idx], alpha);
                    }
                }
            }
        }
    }
}

void Graphics::drawCircleOutline(int cx, int cy, int radius, uint32_t color) {
    if (!buffer_) return;
    float scaleF = static_cast<float>(scale_);
    float cx2 = cx * scaleF + (scaleF * 0.5f);
    float cy2 = cy * scaleF + (scaleF * 0.5f);
    float rOuter = radius * scaleF;
    float rInner = (radius - 1) * scaleF;

    float rOutMax = rOuter + 0.75f;
    float rOutMin = rOuter - 0.75f;
    float rInMax = rInner + 0.75f;
    float rInMin = (std::max)(0.0f, rInner - 0.75f);

    float rOutMaxSq = rOutMax * rOutMax;
    float rInMinSq = rInMin * rInMin;

    int minY = (std::max)(0, static_cast<int>(cy2 - rOutMax - 1.0f));
    int maxY = (std::min)(bufferHeight_ - 1, static_cast<int>(cy2 + rOutMax + 1.0f));

    for (int py = minY; py <= maxY; ++py) {
        float dy = static_cast<float>(py) - cy2;
        float dy2 = dy * dy;
        if (dy2 > rOutMaxSq) continue;

        float dxOut = std::sqrt(rOutMaxSq - dy2);
        int xStart = (std::max)(0, static_cast<int>(std::floor(cx2 - dxOut)));
        int xEnd = (std::min)(bufferWidth_ - 1, static_cast<int>(std::ceil(cx2 + dxOut)));

        auto renderSpan = [&](int startX, int endX) {
            for (int px = startX; px <= endX; ++px) {
                float dx = static_cast<float>(px) - cx2;
                float dist = std::hypot(dx, dy);
                if (dist >= rInMin && dist <= rOutMax) {
                    float aOuter = (dist <= rOutMin) ? 1.0f : ((rOutMax - dist) / 1.5f);
                    float aInner = (dist >= rInMax) ? 1.0f : ((dist - rInMin) / 1.5f);
                    float alpha = (std::min)(aOuter, aInner);
                    if (alpha > 0.0f) {
                        int idx = py * bufferWidth_ + px;
                        buffer_[idx] = blendColors(color, buffer_[idx], alpha);
                    }
                }
            }
        };

        if (rInMinSq > 0.0f && dy2 < rInMinSq) {
            float dxIn = std::sqrt(rInMinSq - dy2);
            int xInLeft = static_cast<int>(std::floor(cx2 - dxIn));
            int xInRight = static_cast<int>(std::ceil(cx2 + dxIn));

            int leftEnd = (std::min)(xEnd, xInLeft);
            if (xStart <= leftEnd) {
                renderSpan(xStart, leftEnd);
            }

            int rightStart = (std::max)(xStart, xInRight);
            if (rightStart <= xEnd) {
                renderSpan(rightStart, xEnd);
            }
        } else {
            renderSpan(xStart, xEnd);
        }
    }
}

void Graphics::drawLine(int x0, int y0, int x1, int y1, uint32_t color, int thickness) {
    if (!buffer_) return;
    float scaleF = static_cast<float>(scale_);
    float p0x = x0 * scaleF + (scaleF * 0.5f);
    float p0y = y0 * scaleF + (scaleF * 0.5f);
    float p1x = x1 * scaleF + (scaleF * 0.5f);
    float p1y = y1 * scaleF + (scaleF * 0.5f);
    float halfThick = thickness * (scaleF * 0.5f);

    float dx = p1x - p0x;
    float dy = p1y - p0y;
    float l2 = dx * dx + dy * dy;
    float r = halfThick + 0.75f;

    int minY = (std::max)(0, static_cast<int>((std::min)(p0y, p1y) - r - 1.0f));
    int maxY = (std::min)(bufferHeight_ - 1, static_cast<int>((std::max)(p0y, p1y) + r + 1.0f));

    float l = std::sqrt(l2);

    for (int py = minY; py <= maxY; ++py) {
        float pyf = static_cast<float>(py);
        float Y = pyf - p0y;

        int minXrow = 0;
        int maxXrow = bufferWidth_ - 1;

        if (l2 == 0.0f) {
            if (std::abs(Y) > r) continue;
            float dxMax = std::sqrt((std::max)(0.0f, r * r - Y * Y));
            minXrow = (std::max)(0, static_cast<int>(std::floor(p0x - dxMax)));
            maxXrow = (std::min)(bufferWidth_ - 1, static_cast<int>(std::ceil(p0x + dxMax)));
        } else if (std::abs(dy) < 1e-5f) {
            if (std::abs(Y) > r) continue;
            float x0_cap = (std::min)(0.0f, dx) - r;
            float x1_cap = (std::max)(0.0f, dx) + r;
            minXrow = (std::max)(0, static_cast<int>(std::floor(p0x + x0_cap)));
            maxXrow = (std::min)(bufferWidth_ - 1, static_cast<int>(std::ceil(p0x + x1_cap)));
        } else {
            float absDy = std::abs(dy);
            float xCenter = (Y * dx) / dy;
            float xHalfSpan = (r * l) / absDy;

            float xStartLine = xCenter - xHalfSpan;
            float xEndLine = xCenter + xHalfSpan;

            float xStartCap = (std::min)(0.0f, dx) - r;
            float xEndCap = (std::max)(0.0f, dx) + r;

            float xStart = (std::max)(xStartLine, xStartCap);
            float xEnd = (std::min)(xEndLine, xEndCap);

            if (xStart > xEnd) continue;

            minXrow = (std::max)(0, static_cast<int>(std::floor(p0x + xStart)));
            maxXrow = (std::min)(bufferWidth_ - 1, static_cast<int>(std::ceil(p0x + xEnd)));
        }

        for (int px = minXrow; px <= maxXrow; ++px) {
            float pxf = static_cast<float>(px);
            float dist = 0.0f;
            if (l2 == 0.0f) {
                dist = std::hypot(pxf - p0x, pyf - p0y);
            } else {
                float t = ((pxf - p0x) * dx + (pyf - p0y) * dy) / l2;
                t = (std::min)((std::max)(t, 0.0f), 1.0f);
                float projX = p0x + t * dx;
                float projY = p0y + t * dy;
                dist = std::hypot(pxf - projX, pyf - projY);
            }

            float alpha = 0.0f;
            if (dist <= halfThick - 0.75f) {
                alpha = 1.0f;
            } else if (dist < halfThick + 0.75f) {
                alpha = (halfThick + 0.75f - dist) / 1.5f;
            }

            if (alpha > 0.0f) {
                int idx = py * bufferWidth_ + px;
                buffer_[idx] = blendColors(color, buffer_[idx], alpha);
            }
        }
    }
}

void Graphics::drawChar(int x, int y, char c, uint32_t color, const Font& font, int scale) {
    const uint8_t* glyph = font.getGlyphData(c);
    if (!glyph) return;

    int w = font.getWidth();
    int h = font.getHeight();

    for (int col = 0; col < w; ++col) {
        uint8_t line = glyph[col];
        for (int row = 0; row < h; ++row) {
            if (line & (1 << row)) {
                drawRect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

void Graphics::drawText(int x, int y, const char* text, uint32_t color, const Font& font, int scale) {
    int currX = x;
    int charWidth = font.getWidth();
    while (*text) {
        drawChar(currX, y, *text, color, font, scale);
        currX += (charWidth + 1) * scale;
        text++;
    }
}

void Graphics::drawArc(int cx, int cy, int radius, float startAngleRad, float endAngleRad, uint32_t color, int thickness) {
    if (!buffer_) return;
    float scaleF = static_cast<float>(scale_);
    float cx2 = cx * scaleF + (scaleF * 0.5f);
    float cy2 = cy * scaleF + (scaleF * 0.5f);

    float halfThick = thickness * (scaleF * 0.5f);
    float rMid = radius * scaleF;
    float rOuter = rMid + halfThick + 0.75f;
    float rInner = (std::max)(0.0f, rMid - halfThick - 0.75f);
    float rOuterSq = rOuter * rOuter;
    float rInnerSq = rInner * rInner;

    // Normalize angles
    while (endAngleRad < startAngleRad) endAngleRad += kTWO_PI_F;
    float sweep = endAngleRad - startAngleRad;
    bool fullCircle = sweep >= (kTWO_PI_F - 1e-4f);

    // Angular membership without a per-pixel atan2(): rotate (dx,dy) into a
    // frame where the arc's start ray sits on the local +x axis (cosStart/
    // sinStart, computed once), then decide which side of the start/sweep
    // rays a point falls on with a cross-product sign test instead of an
    // inverse-trig call per pixel (measured as ~28% of total render time).
    float cosStart = std::cos(startAngleRad);
    float sinStart = std::sin(startAngleRad);
    float cosSweep = std::cos(sweep);
    float sinSweep = std::sin(sweep);
    bool sweepIsSmall = sweep <= kPI_F;

    int minY = (std::max)(0, static_cast<int>(cy2 - rOuter - 1.0f));
    int maxY = (std::min)(bufferHeight_ - 1, static_cast<int>(cy2 + rOuter + 1.0f));

    // Narrow each scanline to the outer circle's x-span (as drawCircleOutline
    // does) instead of scanning the whole bounding square, and reject with a
    // cheap squared-distance test before paying for sqrt. An arc's ring is a
    // small fraction of its bounding box, so this avoids doing work over area
    // that can never be inside the ring.
    for (int py = minY; py <= maxY; ++py) {
        float dy = static_cast<float>(py) - cy2;
        float dy2 = dy * dy;
        if (dy2 > rOuterSq) continue;

        float dxOut = std::sqrt(rOuterSq - dy2);
        int xStart = (std::max)(0, static_cast<int>(std::floor(cx2 - dxOut)));
        int xEnd = (std::min)(bufferWidth_ - 1, static_cast<int>(std::ceil(cx2 + dxOut)));

        for (int px = xStart; px <= xEnd; ++px) {
            float dx = static_cast<float>(px) - cx2;
            float distSq = dx * dx + dy2;
            if (distSq < rInnerSq || distSq > rOuterSq) continue;

            if (!fullCircle) {
                // (dxr, dyr): pixel direction relative to the start ray.
                float dxr = dx * cosStart + dy * sinStart;
                float dyr = -dx * sinStart + dy * cosStart;
                bool inRange;
                if (sweepIsSmall) {
                    inRange = (dyr >= 0.0f) && (dxr * sinSweep - dyr * cosSweep >= 0.0f);
                } else {
                    // Reflex sweep (>180 deg): test the small complementary
                    // gap on the far side (relative to the end ray) instead,
                    // then invert -- same cross-product test as above, just
                    // rotated into the end ray's frame.
                    float dxe = dxr * cosSweep + dyr * sinSweep;
                    float dye = -dxr * sinSweep + dyr * cosSweep;
                    bool inGap = (dye >= 0.0f) && (-dxe * sinSweep - dye * cosSweep >= 0.0f);
                    inRange = !inGap;
                }
                if (!inRange) continue;
            }

            float dist = std::sqrt(distSq);
            float distFromMid = std::abs(dist - rMid);
            float alpha = 1.0f;
            if (distFromMid > halfThick - 0.75f) {
                alpha = (halfThick + 0.75f - distFromMid) / 1.5f;
            }
            if (alpha > 0.0f) {
                int idx = py * bufferWidth_ + px;
                buffer_[idx] = blendColors(color, buffer_[idx], alpha);
            }
        }
    }
}

void Graphics::drawRivet(int cx, int cy, int radius) {
    // Dark metallic industrial rivet with copper/steel highlight
    drawCircle(cx, cy, radius, 0xFF121416);
    drawCircleOutline(cx, cy, radius, 0xFF3A3D40);
    drawCircle(cx, cy, radius - 1, 0xFF282A2C);
    // Beveled highlight dot
    drawRect(cx - 1, cy - 1, 1, 1, 0xFF888C90);
}

void Graphics::drawPanelFrame(int x, int y, int w, int h, const char* title, const Font& font) {
    // Heavy dark forged iron panel with copper trim and rivets
    // 1. Panel outer border (Dark Gunmetal)
    drawRect(x, y, w, h, 0xFF16181A);

    // 2. Recessed inner panel background with vertical metallic gradient
    drawVerticalGradient(x + 2, y + 2, w - 4, h - 4, 0xFF202326, 0xFF121416);

    // 3. Copper accent frame line
    drawRectOutline(x + 3, y + 3, w - 6, h - 6, 0xFF8C5224, 1);
    drawRectOutline(x + 4, y + 4, w - 8, h - 8, 0xFF1A1C1E, 1);

    // 4. Rivets in four corners of panel frame
    drawRivet(x + 8, y + 8, 3);
    drawRivet(x + w - 8, y + 8, 3);
    drawRivet(x + 8, y + h - 8, 3);
    drawRivet(x + w - 8, y + h - 8, 3);

    // 5. Header Title Bar (Dark plate with Amber text)
    if (title && strlen(title) > 0) {
        int titleLen = static_cast<int>(strlen(title));
        int titleW = titleLen * (font.getWidth() + 1) + 12;
        int titleX = x + (w - titleW) / 2;

        drawRect(titleX, y, titleW, 14, 0xFF101214);
        drawRectOutline(titleX, y, titleW, 14, 0xFF8C5224, 1);
        drawText(titleX + 6, y + 3, title, 0xFFFF8A00, font, 1);
    }
}

void Graphics::blitBuffer(int dstX, int dstY, const OffscreenBuffer& srcBuffer) {
    if (!buffer_) return;
    int srcW = srcBuffer.getWidth();
    int srcH = srcBuffer.getHeight();

    int startX = (std::max)(0, dstX * scale_);
    int startY = (std::max)(0, dstY * scale_);
    int endX = (std::min)(bufferWidth_, (dstX + srcW) * scale_);
    int endY = (std::min)(bufferHeight_, (dstY + srcH) * scale_);

    const uint32_t* srcData = srcBuffer.data();

    for (int py = startY; py < endY; ++py) {
        int srcY = (py - dstY * scale_) / scale_;
        for (int px = startX; px < endX; ++px) {
            int srcX = (px - dstX * scale_) / scale_;
            uint32_t color = srcData[srcY * srcW + srcX];
            uint32_t alpha = (color >> 24) & 0xFF;
            if (alpha == 0xFF) {
                buffer_[py * bufferWidth_ + px] = color;
            } else if (alpha > 0) {
                int idx = py * bufferWidth_ + px;
                buffer_[idx] = blendColors(color, buffer_[idx], alpha / 255.0f);
            }
        }
    }
}

void Graphics::blitRotatedBuffer(int dstCx, int dstCy, const OffscreenBuffer& srcBuffer, float angleRad) {
    if (!buffer_) return;
    int srcW = srcBuffer.getWidth();
    int srcH = srcBuffer.getHeight();
    float srcHalfW = srcW * 0.5f;
    float srcHalfH = srcH * 0.5f;

    float cosA = std::cos(-angleRad);
    float sinA = std::sin(-angleRad);

    float scaleF = static_cast<float>(scale_);
    float dstCxScaled = dstCx * scaleF;
    float dstCyScaled = dstCy * scaleF;

    float maxR = std::hypot(srcHalfW, srcHalfH) * scaleF;
    int minX = (std::max)(0, static_cast<int>(dstCxScaled - maxR - 1.0f));
    int maxX = (std::min)(bufferWidth_ - 1, static_cast<int>(dstCxScaled + maxR + 1.0f));
    int minY = (std::max)(0, static_cast<int>(dstCyScaled - maxR - 1.0f));
    int maxY = (std::min)(bufferHeight_ - 1, static_cast<int>(dstCyScaled + maxR + 1.0f));

    const uint32_t* srcData = srcBuffer.data();

    for (int py = minY; py <= maxY; ++py) {
        float dy = (static_cast<float>(py) - dstCyScaled) / scaleF;
        for (int px = minX; px <= maxX; ++px) {
            float dx = (static_cast<float>(px) - dstCxScaled) / scaleF;

            float srcXf = dx * cosA - dy * sinA + srcHalfW;
            float srcYf = dx * sinA + dy * cosA + srcHalfH;

            int srcX = static_cast<int>(srcXf);
            int srcY = static_cast<int>(srcYf);

            if (srcX >= 0 && srcX < srcW && srcY >= 0 && srcY < srcH) {
                uint32_t color = srcData[srcY * srcW + srcX];
                uint32_t alpha = (color >> 24) & 0xFF;
                if (alpha == 0xFF) {
                    buffer_[py * bufferWidth_ + px] = color;
                } else if (alpha > 0) {
                    int idx = py * bufferWidth_ + px;
                    buffer_[idx] = blendColors(color, buffer_[idx], alpha / 255.0f);
                }
            }
        }
    }
}

} // namespace gritbaal
