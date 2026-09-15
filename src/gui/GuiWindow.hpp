#ifndef GRITBAAL_GUI_WINDOW_HPP
#define GRITBAAL_GUI_WINDOW_HPP

#include <clap/clap.h>
#include <clap/ext/gui.h>
#include <vector>
#include <cstdint>
#include <atomic>
#include <thread>
#include <memory>
#include "Font.hpp"
#include "IControlRenderer.hpp"
#include "PanelLayout.hpp"

namespace gritbaal {

class GritbaalClap;

extern const clap_plugin_gui_t g_gritbaalGuiExtension;

enum class ControlType {
    Knob,
    ToggleSwitch,
    PushButton
};

struct Control {
    int id;
    const char* label;
    ControlType type;
    int x, y;
    int radius;
    double minVal, maxVal, currentVal;
    bool isStepped;
};

class GuiWindow {
public:
    explicit GuiWindow(GritbaalClap* plugin);
    ~GuiWindow();

    bool setParent(const clap_window_t* window);
    bool setSize(uint32_t width, uint32_t height);
    bool show();
    bool hide();
    void destroy();

    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }

    const std::vector<uint32_t>& getPixelBuffer() const { return pixelBuffer_; }

    void setFont(const Font& font) { font_ = font; }
    const Font& getFont() const { return font_; }

    void setControlRenderer(std::unique_ptr<IControlRenderer> renderer) {
        if (renderer) {
            controlRenderer_ = std::move(renderer);
        }
    }

    void renderFrame();
    void handleMouseDown(int x, int y, bool isShift = false);
    void handleMouseDrag(int x, int y, bool isShift = false);
    void handleMouseUp();

private:
    GritbaalClap* plugin_{nullptr};
    uint32_t width_{710};
    uint32_t height_{180};

    std::vector<uint32_t> pixelBuffer_; // ARGB format (32-bit)
    std::vector<uint32_t> hiResBuffer_; // 2x supersampled buffer
    std::vector<Control> controls_;
    bool lastShiftState_{false};

    Font font_{Font::default5x7()};
    std::unique_ptr<IControlRenderer> controlRenderer_;
    std::unique_ptr<PanelLayout> layout_;

    int activeControlIndex_{-1};
    int dragStartY_{0};
    double dragStartVal_{0.0};

    std::atomic<bool> isRunning_{false};
    std::thread eventThread_;

#if defined(__linux__) && !defined(__APPLE__)
    void* x11Display_{nullptr};
    unsigned long x11Window_{0};
    unsigned long x11ParentWindow_{0};
    bool x11Created_{false};

    void initX11Window();
    void drawX11Frame();
    void eventLoopX11();
#elif defined(_WIN32)
    void* hwnd_{nullptr};
    void* parentHwnd_{nullptr};
public:
    void initWin32Window();
    void drawWin32Frame();
private:
#elif defined(__APPLE__)
    void* nsView_{nullptr};
    void* parentNsView_{nullptr};
    void initCocoaWindow();
    void drawCocoaFrame();
#endif

    void initControls();
    void updateKnobValuesFromPlugin();
    void drawGritbaalTitle(Graphics& g, int x, int y);
};

} // namespace gritbaal

#endif // GRITBAAL_GUI_WINDOW_HPP
