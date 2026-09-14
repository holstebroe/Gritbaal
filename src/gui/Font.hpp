#ifndef SYREBAS_FONT_HPP
#define SYREBAS_FONT_HPP

#include <cstdint>
#include <cstddef>

namespace syrebas {

class Font {
public:
    Font(int width = 5, int height = 7, const uint8_t (*glyphData)[5] = nullptr);

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    const uint8_t* getGlyphData(char c) const;

    static const Font& default5x7();

private:
    int width_{5};
    int height_{7};
    const uint8_t (*glyphData_)[5]{nullptr};
};

} // namespace syrebas

#endif // SYREBAS_FONT_HPP
