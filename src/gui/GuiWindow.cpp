#include "GuiWindow.hpp"
#include "Graphics.hpp"
#include "ControlRenderer.hpp"
#include "clap/GritbaalClap.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <iostream>

#if defined(__linux__) && !defined(__APPLE__)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace gritbaal {

GuiWindow::GuiWindow(GritbaalClap* plugin)
    : plugin_(plugin), controlRenderer_(std::make_unique<IndustrialGritbaalRenderer>()),
      width_(750), height_(220) {
    pixelBuffer_.resize(width_ * height_, 0xFF141517);

    // Initialize Panel Layout Engine
    layout_ = std::make_unique<PanelLayout>(width_, height_);
    layout_->addPanel("VCF SECTION", 10, 20, 310, 185);
    layout_->addPanel("ENVELOPE", 330, 20, 180, 185);
    layout_->addPanel("VCO & MAIN", 520, 20, 220, 185);

    initControls();
}

GuiWindow::~GuiWindow() {
    destroy();
}

void GuiWindow::initControls() {
    controls_.clear();
    // VCF Panel Controls (x: 10..320, y: 20..205)
    controls_.push_back({ PARAM_CUTOFF, "CUTOFF", ControlType::Knob, 60, 95, 20, 0.0, 1.0, 0.5, false });
    controls_.push_back({ PARAM_RESONANCE, "RESONANCE", ControlType::Knob, 165, 95, 20, 0.0, 1.0, 0.5, false });
    controls_.push_back({ PARAM_ENV_MOD, "ENV MOD", ControlType::Knob, 265, 95, 20, 0.0, 1.0, 0.5, false });

    // Envelope Panel Controls (x: 330..510, y: 20..205)
    controls_.push_back({ PARAM_DECAY, "DECAY", ControlType::Knob, 375, 95, 20, 0.0, 1.0, 0.5, false });
    controls_.push_back({ PARAM_ACCENT, "ACCENT", ControlType::Knob, 465, 95, 20, 0.0, 1.0, 0.5, false });

    // VCO & Main Panel Controls (x: 520..740, y: 20..205)
    controls_.push_back({ PARAM_WAVEFORM, "WAVEFORM", ControlType::ToggleSwitch, 570, 95, 15, 0.0, 1.0, 0.0, true });
    controls_.push_back({ PARAM_VOLUME, "VOLUME", ControlType::Knob, 670, 95, 20, 0.0, 1.0, 0.8, false });

    updateKnobValuesFromPlugin();
}

void GuiWindow::updateKnobValuesFromPlugin() {
    if (!plugin_) return;
    for (size_t i = 0; i < controls_.size(); ++i) {
        if (static_cast<int>(i) == activeControlIndex_) continue;
        double val = 0.0;
        if (plugin_->paramsValue(controls_[i].id, &val)) {
            controls_[i].currentVal = val;
        }
    }
}

void GuiWindow::drawGritbaalTitle(Graphics& g, int x, int y) {
    // Header Title Logo text "GRITBAAL SYNTH" in glowing Amber
    g.drawText(x, y, "GRITBAAL SYNTH", 0xFFFF8A00, font_, 2);
}

void GuiWindow::renderFrame() {
    updateKnobValuesFromPlugin();

    int hW = width_ * 2;
    int hH = height_ * 2;
    if (hiResBuffer_.size() != static_cast<size_t>(hW * hH)) {
        hiResBuffer_.resize(hW * hH);
    }

    Graphics g(hiResBuffer_.data(), width_, height_, 2);

    // 1. Render Layout (Modular Dark Iron Panels & Copper Trim)
    if (layout_) {
        layout_->drawLayout(g, font_);
    } else {
        g.clear(0xFF141517);
    }

    // 2. Render Controls
    if (controlRenderer_) {
        for (const auto& ctrl : controls_) {
            if (ctrl.type == ControlType::Knob) {
                controlRenderer_->drawKnob(g, ctrl, font_);
            } else if (ctrl.type == ControlType::ToggleSwitch) {
                controlRenderer_->drawToggleSwitch(g, ctrl, font_);
            } else if (ctrl.type == ControlType::PushButton) {
                controlRenderer_->drawPushButton(g, ctrl, font_);
            }
        }
    }

    // 3. Header title on chassis
    drawGritbaalTitle(g, 20, 3);

    // 4. Downsample hiResBuffer_ (2x2 box filter) into pixelBuffer_
    pixelBuffer_.resize(width_ * height_);
    for (uint32_t py = 0; py < height_; ++py) {
        for (uint32_t px = 0; px < width_; ++px) {
            uint32_t p00 = hiResBuffer_[(2 * py) * hW + (2 * px)];
            uint32_t p01 = hiResBuffer_[(2 * py) * hW + (2 * px + 1)];
            uint32_t p10 = hiResBuffer_[(2 * py + 1) * hW + (2 * px)];
            uint32_t p11 = hiResBuffer_[(2 * py + 1) * hW + (2 * px + 1)];

            uint32_t r = (((p00 >> 16) & 0xFF) + ((p01 >> 16) & 0xFF) + ((p10 >> 16) & 0xFF) + ((p11 >> 16) & 0xFF) + 2) >> 2;
            uint32_t gVal = (((p00 >> 8) & 0xFF) + ((p01 >> 8) & 0xFF) + ((p10 >> 8) & 0xFF) + ((p11 >> 8) & 0xFF) + 2) >> 2;
            uint32_t b = ((p00 & 0xFF) + (p01 & 0xFF) + (p10 & 0xFF) + (p11 & 0xFF) + 2) >> 2;

            pixelBuffer_[py * width_ + px] = 0xFF000000 | (r << 16) | (gVal << 8) | b;
        }
    }

#if defined(__linux__) && !defined(__APPLE__)
    drawX11Frame();
#elif defined(_WIN32)
    drawWin32Frame();
#elif defined(__APPLE__)
    drawCocoaFrame();
#endif
}

void GuiWindow::handleMouseDown(int x, int y, bool isShift) {
    lastShiftState_ = isShift;
    for (size_t i = 0; i < controls_.size(); ++i) {
        auto& ctrl = controls_[i];
        if (ctrl.type == ControlType::Knob) {
            int dx = x - ctrl.x;
            int dy = y - ctrl.y;
            if (dx * dx + dy * dy <= (ctrl.radius + 10) * (ctrl.radius + 10)) {
                activeControlIndex_ = static_cast<int>(i);
                dragStartY_ = y;
                dragStartVal_ = ctrl.currentVal;
                if (plugin_) {
                    plugin_->onBeginEditFromGui(ctrl.id);
                }
                break;
            }
        } else if (ctrl.type == ControlType::ToggleSwitch) {
            if (std::abs(x - ctrl.x) <= 20 && std::abs(y - ctrl.y) <= 25) {
                if (plugin_) {
                    plugin_->onBeginEditFromGui(ctrl.id);
                }
                double newVal = (ctrl.currentVal >= 0.5) ? 0.0 : 1.0;
                ctrl.currentVal = newVal;
                if (plugin_) {
                    plugin_->onParamValueFromGui(ctrl.id, newVal);
                    plugin_->onEndEditFromGui(ctrl.id);
                }
                renderFrame();
                break;
            }
        }
    }
}

void GuiWindow::handleMouseDrag(int x, int y, bool isShift) {
    if (activeControlIndex_ < 0 || activeControlIndex_ >= static_cast<int>(controls_.size())) return;

    auto& ctrl = controls_[activeControlIndex_];
    if (ctrl.type != ControlType::Knob) return;

    if (isShift != lastShiftState_) {
        dragStartY_ = y;
        dragStartVal_ = ctrl.currentVal;
        lastShiftState_ = isShift;
    }

    int deltaY = dragStartY_ - y;

    double range = ctrl.maxVal - ctrl.minVal;
    double sensitivity = (isShift ? 0.0025 : 0.0125) * range;
    double newVal = dragStartVal_ + deltaY * sensitivity;

    newVal = (std::min)((std::max)(newVal, ctrl.minVal), ctrl.maxVal);
    ctrl.currentVal = newVal;

    if (plugin_) {
        plugin_->onParamValueFromGui(ctrl.id, newVal);
    }

    renderFrame();
}

void GuiWindow::handleMouseUp() {
    if (activeControlIndex_ >= 0 && activeControlIndex_ < static_cast<int>(controls_.size())) {
        if (plugin_) {
            plugin_->onEndEditFromGui(controls_[activeControlIndex_].id);
        }
    }
    activeControlIndex_ = -1;
}

bool GuiWindow::setParent(const clap_window_t* window) {
    if (!window) return false;
#if defined(__linux__) && !defined(__APPLE__)
    if (std::strcmp(window->api, CLAP_WINDOW_API_X11) == 0) {
        x11ParentWindow_ = window->x11;
        initX11Window();
        return true;
    }
#elif defined(_WIN32)
    if (std::strcmp(window->api, CLAP_WINDOW_API_WIN32) == 0) {
        parentHwnd_ = window->win32;
        initWin32Window();
        return true;
    }
#elif defined(__APPLE__)
    if (std::strcmp(window->api, CLAP_WINDOW_API_COCOA) == 0) {
        parentNsView_ = window->cocoa;
        initCocoaWindow();
        return true;
    }
#endif
    return false;
}

bool GuiWindow::setSize(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    pixelBuffer_.resize(width_ * height_, 0xFFDBDFE1);
    renderFrame();
    return true;
}

bool GuiWindow::show() {
    renderFrame();
    return true;
}

bool GuiWindow::hide() {
    return true;
}

void GuiWindow::destroy() {
    isRunning_ = false;
    if (eventThread_.joinable()) {
        eventThread_.join();
    }
#if defined(__linux__) && !defined(__APPLE__)
    if (x11Display_ && x11Created_) {
        Display* display = static_cast<Display*>(x11Display_);
        XDestroyWindow(display, x11Window_);
        XCloseDisplay(display);
        x11Display_ = nullptr;
        x11Created_ = false;
    }
#endif
}

#if defined(__linux__) && !defined(__APPLE__)
void GuiWindow::initX11Window() {
    if (x11Created_) return;

    Display* display = XOpenDisplay(nullptr);
    if (!display) return;

    x11Display_ = display;
    int screen = DefaultScreen(display);
    Window parent = x11ParentWindow_ ? x11ParentWindow_ : RootWindow(display, screen);

    x11Window_ = XCreateSimpleWindow(display, parent, 0, 0, width_, height_, 0,
                                     BlackPixel(display, screen), WhitePixel(display, screen));

    XSelectInput(display, x11Window_, ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XMapWindow(display, x11Window_);
    XFlush(display);

    x11Created_ = true;
    renderFrame();

    isRunning_ = true;
    eventThread_ = std::thread(&GuiWindow::eventLoopX11, this);
}

void GuiWindow::eventLoopX11() {
    if (!x11Display_) return;
    Display* display = static_cast<Display*>(x11Display_);

    while (isRunning_) {
        while (XPending(display) > 0) {
            XEvent ev;
            XNextEvent(display, &ev);

            if (ev.type == Expose) {
                drawX11Frame();
            } else if (ev.type == ButtonPress) {
                bool isShift = (ev.xbutton.state & ShiftMask) != 0;
                handleMouseDown(ev.xbutton.x, ev.xbutton.y, isShift);
            } else if (ev.type == MotionNotify) {
                if (ev.xmotion.state & Button1Mask) {
                    bool isShift = (ev.xmotion.state & ShiftMask) != 0;
                    handleMouseDrag(ev.xmotion.x, ev.xmotion.y, isShift);
                }
            } else if (ev.type == ButtonRelease) {
                handleMouseUp();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void GuiWindow::drawX11Frame() {
    if (!x11Display_ || !x11Created_) return;

    Display* display = static_cast<Display*>(x11Display_);
    int screen = DefaultScreen(display);

    XImage* image = XCreateImage(display, DefaultVisual(display, screen),
                                 24, ZPixmap, 0,
                                 reinterpret_cast<char*>(pixelBuffer_.data()),
                                 width_, height_, 32, 0);

    GC gc = DefaultGC(display, screen);
    XPutImage(display, x11Window_, gc, image, 0, 0, 0, 0, width_, height_);

    image->data = nullptr;
    XDestroyImage(image);
    XFlush(display);
}
#endif

#if defined(_WIN32)
static const wchar_t* kGritbaalClassName = L"GritbaalWindowCLASS";
static bool g_win32ClassRegistered = false;

static LRESULT CALLBACK GritbaalWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GuiWindow* gui = reinterpret_cast<GuiWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            gui = reinterpret_cast<GuiWindow*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(gui));
            SetTimer(hwnd, 1, 16, NULL);
            return 0;
        }
        case WM_TIMER: {
            if (gui) {
                gui->renderFrame();
            }
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (gui) {
                gui->drawWin32Frame();
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (gui) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                bool isShift = (wParam & MK_SHIFT) != 0;
                SetCapture(hwnd);
                gui->handleMouseDown(x, y, isShift);
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (gui && (wParam & MK_LBUTTON)) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                bool isShift = (wParam & MK_SHIFT) != 0;
                gui->handleMouseDrag(x, y, isShift);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (gui) {
                ReleaseCapture();
                gui->handleMouseUp();
            }
            return 0;
        }
        case WM_DESTROY: {
            KillTimer(hwnd, 1);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void GuiWindow::initWin32Window() {
    if (hwnd_) return;

    HINSTANCE hInstance = GetModuleHandleW(NULL);

    if (!g_win32ClassRegistered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = GritbaalWndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = kGritbaalClassName;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassW(&wc);
        g_win32ClassRegistered = true;
    }

    HWND parent = static_cast<HWND>(parentHwnd_);

    hwnd_ = CreateWindowExW(
        0, kGritbaalClassName, L"Gritbaal 303",
        WS_CHILD | WS_VISIBLE,
        0, 0, width_, height_,
        parent, NULL, hInstance, this
    );

    renderFrame();
}

void GuiWindow::drawWin32Frame() {
    if (!hwnd_) return;

    HDC hdc = GetDC(static_cast<HWND>(hwnd_));
    if (!hdc) return;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width_;
    bmi.bmiHeader.biHeight = -static_cast<int>(height_);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetDIBitsToDevice(
        hdc,
        0, 0, width_, height_,
        0, 0, 0, height_,
        pixelBuffer_.data(),
        &bmi,
        DIB_RGB_COLORS
    );

    ReleaseDC(static_cast<HWND>(hwnd_), hdc);
}
#endif

#if defined(__APPLE__)
void GuiWindow::initCocoaWindow() {}
void GuiWindow::drawCocoaFrame() {}
#endif

// CLAP GUI Extension Callbacks
const clap_plugin_gui_t g_gritbaalGuiExtension = {
    [](const clap_plugin_t* plugin, const char* api, bool is_floating) -> bool {
#if defined(__linux__) && !defined(__APPLE__)
        return std::strcmp(api, CLAP_WINDOW_API_X11) == 0 && !is_floating;
#elif defined(_WIN32)
        return std::strcmp(api, CLAP_WINDOW_API_WIN32) == 0 && !is_floating;
#elif defined(__APPLE__)
        return std::strcmp(api, CLAP_WINDOW_API_COCOA) == 0 && !is_floating;
#else
        return false;
#endif
    },
    [](const clap_plugin_t* plugin, const char** api, bool* is_floating) -> bool {
#if defined(__linux__) && !defined(__APPLE__)
        *api = CLAP_WINDOW_API_X11;
#elif defined(_WIN32)
        *api = CLAP_WINDOW_API_WIN32;
#elif defined(__APPLE__)
        *api = CLAP_WINDOW_API_COCOA;
#endif
        *is_floating = false;
        return true;
    },
    [](const clap_plugin_t* plugin, const char* api, bool is_floating) -> bool {
        auto* self = static_cast<GritbaalClap*>(plugin->plugin_data);
        self->createGuiWindow();
        return true;
    },
    [](const clap_plugin_t* plugin) {
        auto* self = static_cast<GritbaalClap*>(plugin->plugin_data);
        self->destroyGuiWindow();
    },
    [](const clap_plugin_t* plugin, double scale) -> bool {
        return false;
    },
    [](const clap_plugin_t* plugin, uint32_t* width, uint32_t* height) -> bool {
        *width = 750;
        *height = 220;
        return true;
    },
    [](const clap_plugin_t* plugin) -> bool {
        return false;
    },
    [](const clap_plugin_t* plugin, clap_gui_resize_hints_t* hints) -> bool {
        return false;
    },
    [](const clap_plugin_t* plugin, uint32_t* width, uint32_t* height) -> bool {
        *width = 750;
        *height = 220;
        return true;
    },
    [](const clap_plugin_t* plugin, uint32_t width, uint32_t height) -> bool {
        auto* self = static_cast<GritbaalClap*>(plugin->plugin_data);
        if (self->getGuiWindow()) {
            return self->getGuiWindow()->setSize(width, height);
        }
        return true;
    },
    [](const clap_plugin_t* plugin, const clap_window_t* window) -> bool {
        auto* self = static_cast<GritbaalClap*>(plugin->plugin_data);
        if (!self->getGuiWindow()) {
            self->createGuiWindow();
        }
        return self->getGuiWindow()->setParent(window);
    },
    [](const clap_plugin_t* plugin, const clap_window_t* window) -> bool {
        return false;
    },
    [](const clap_plugin_t* plugin, const char* title) {},
    [](const clap_plugin_t* plugin) -> bool {
        auto* self = static_cast<GritbaalClap*>(plugin->plugin_data);
        if (self->getGuiWindow()) {
            return self->getGuiWindow()->show();
        }
        return false;
    },
    [](const clap_plugin_t* plugin) -> bool {
        auto* self = static_cast<GritbaalClap*>(plugin->plugin_data);
        if (self->getGuiWindow()) {
            return self->getGuiWindow()->hide();
        }
        return false;
    }
};

} // namespace gritbaal
