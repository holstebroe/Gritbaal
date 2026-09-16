#include "GuiWindow.hpp"
#include "Graphics.hpp"
#include "IndustrialGritbaalRenderer.hpp"
#include "clap/GritbaalClap.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <cstdio>

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

static const char* kFactoryPresetNames[] = {
    "01: Hellfire Bass",
    "02: Screaming Acid",
    "03: Dark Energy Lead",
    "04: Industrial Pad"
};
static const int kNumFactoryPresets = 4;

static const char* kTargetFullNames[] = {
    "Filter Cutoff", "Filter Resonance", "Oscillator Pitch", "VCO1 Pulse Width", "VCO2 Pulse Width",
    "VCO2 Detune", "Pitch FM Amount", "VCO1 Volume", "VCO2 Volume", "Sub Osc Volume",
    "Ring Modulation", "Crackle Noise Volume", "Pre-Filter Drive", "Tube Overdrive",
    "VCA Amplitude", "LFO1 Rate", "LFO1 Amount", "LFO2 Rate", "LFO2 Amount"
};

GuiWindow::GuiWindow(GritbaalClap* plugin)
    : plugin_(plugin), controlRenderer_(std::make_unique<IndustrialGritbaalRenderer>()),
      width_(980), height_(480) {
    pixelBuffer_.resize(width_ * height_, 0xFF141517);

    // Initialize Panel Layout Engine matching Section 2 of docs/design_document.md
    layout_ = std::make_unique<PanelLayout>(width_, height_);
    // Top Row Panels (y: 35, height: 200)
    layout_->addPanel("VCO SECTION", 15, 35, 260, 200);
    layout_->addPanel("MIX & DRIVE", 285, 35, 230, 200);
    layout_->addPanel("VCF SECTION", 525, 35, 220, 200);
    layout_->addPanel("GLOBAL & DRIFT", 755, 35, 210, 200);

    // Bottom Row Panels (y: 245, height: 220)
    layout_->addPanel("MODULATION & LFO", 15, 245, 260, 220);
    layout_->addPanel("ENVELOPES (ENV1: VCF / ENV2: VCA)", 285, 245, 470, 220);
    layout_->addPanel("OUTPUT & MASTER", 765, 245, 200, 220);

    initControls();
}

GuiWindow::~GuiWindow() {
    destroy();
}

void GuiWindow::initControls() {
    controls_.clear();

    std::vector<std::string> targetOpts = {
        "CUTOFF", "RESON", "PITCH", "PW1", "PW2", "DETUNE", "FM",
        "V1VOL", "V2VOL", "SUBVOL", "RINGMOD", "NOISE", "PREDRV", "TUBEDRV",
        "AMP", "LFO1R", "LFO1A", "LFO2R", "LFO2A"
    };

    // 1. VCO SECTION (x: 15..275, y: 35..235)
    controls_.push_back({ PARAM_WAVEFORM, "VCO1 WAVE", ControlType::ToggleSwitch, 60, 100, 15, 0.0, 1.0, 0.0, true, false, {}, 0.0 });
    controls_.push_back({ PARAM_VCO1_PW, "VCO1 PW", ControlType::Knob, 135, 100, 18, 0.0, 1.0, 0.5, false, false, {}, 0.5 });
    controls_.push_back({ PARAM_VCO2_DETUNE, "DETUNE", ControlType::Knob, 210, 100, 18, 0.0, 1.0, 0.5, false, true, {}, 0.5 });

    controls_.push_back({ PARAM_VCO2_WAVE, "VCO2 WAVE", ControlType::ToggleSwitch, 60, 175, 15, 0.0, 1.0, 0.0, true, false, {}, 0.0 });
    controls_.push_back({ PARAM_VCO2_PW, "VCO2 PW", ControlType::Knob, 135, 175, 18, 0.0, 1.0, 0.5, false, false, {}, 0.5 });
    controls_.push_back({ PARAM_FM_AMOUNT, "PITCH FM", ControlType::Knob, 210, 175, 18, 0.0, 1.0, 0.0, false, false, {}, 0.0 });

    // 2. MIX & DRIVE SECTION (x: 285..515, y: 35..235)
    controls_.push_back({ PARAM_VCO1_VOL, "VCO1 VOL", ControlType::Knob, 335, 100, 18, 0.0, 1.0, 1.0, false, false, {}, 1.0 });
    controls_.push_back({ PARAM_SUB_VOL, "SUB VOL", ControlType::Knob, 400, 100, 18, 0.0, 1.0, 0.0, false, false, {}, 0.0 });
    controls_.push_back({ PARAM_RING_MOD, "RING MOD", ControlType::Knob, 465, 100, 18, 0.0, 1.0, 0.0, false, false, {}, 0.0 });

    controls_.push_back({ PARAM_VCO2_VOL, "VCO2 VOL", ControlType::Knob, 335, 175, 18, 0.0, 1.0, 0.0, false, false, {}, 0.0 });
    controls_.push_back({ PARAM_NOISE_VOL, "CRACKLE", ControlType::Knob, 400, 175, 18, 0.0, 1.0, 0.0, false, false, {}, 0.0 });
    controls_.push_back({ PARAM_PRE_DRIVE, "PRE DRIVE", ControlType::Knob, 465, 175, 18, 0.0, 1.0, 0.0, false, false, {}, 0.0 });

    // 3. VCF SECTION (x: 525..745, y: 35..235)
    controls_.push_back({ PARAM_CUTOFF, "CUTOFF", ControlType::Knob, 580, 100, 20, 0.0, 1.0, 0.5, false, false, {}, 0.5 });
    controls_.push_back({ PARAM_RESONANCE, "RESONANCE", ControlType::Knob, 670, 100, 20, 0.0, 1.0, 0.5, false, false, {}, 0.5 });
    controls_.push_back({ PARAM_FILTER_TYPE, "MS20/LADDER", ControlType::ToggleSwitch, 625, 175, 15, 0.0, 1.0, 0.0, true, false, {}, 0.0 });

    // 4. GLOBAL & DRIFT (x: 755..965, y: 35..235)
    controls_.push_back({ PARAM_OVERDRIVE, "TUBE DRIVE", ControlType::Knob, 810, 100, 18, 0.0, 1.0, 0.0, false, false, {}, 0.0 });
    controls_.push_back({ PARAM_THERMAL_DRIFT, "THERMAL", ControlType::Knob, 900, 100, 18, 0.0, 1.0, 0.1, false, false, {}, 0.1 });
    controls_.push_back({ PARAM_POWER_SAG, "POWER SAG", ControlType::Knob, 855, 175, 18, 0.0, 1.0, 0.1, false, false, {}, 0.1 });

    // 5. MODULATION & LFO (x: 15..275, y: 245..465)
    controls_.push_back({ PARAM_HARD_SYNC, "HARD SYNC", ControlType::ToggleSwitch, 48, 310, 15, 0.0, 1.0, 0.0, true, false, {}, 0.0 });
    controls_.push_back({ PARAM_LFO1_SYNC, "L1 SYNC", ControlType::ToggleSwitch, 98, 310, 15, 0.0, 1.0, 0.0, true, false, {}, 0.0 });
    controls_.push_back({ PARAM_LFO1_RATE, "LFO1 RATE", ControlType::Knob, 152, 310, 18, 0.0, 1.0, 0.1, false, false, {}, 0.1 });

    // ModeSelector centered at y = 310 - 18 - 15 = 277 (text inside box aligns at y=274 with non-target labels!)
    controls_.push_back({ PARAM_LFO1_TARGET, "LFO1 TGT", ControlType::ModeSelector, 218, 277, 0, 0.0, 18.0, 0.0, true, false, targetOpts, 0.0 });
    controls_.push_back({ PARAM_LFO1_DEPTH, "", ControlType::Knob, 218, 310, 18, 0.0, 1.0, 0.5, false, true, {}, 0.5 });

    controls_.push_back({ PARAM_LFO2_SYNC, "L2 SYNC", ControlType::ToggleSwitch, 98, 390, 15, 0.0, 1.0, 0.0, true, false, {}, 0.0 });
    controls_.push_back({ PARAM_LFO2_RATE, "LFO2 RATE", ControlType::Knob, 152, 390, 18, 0.0, 1.0, 0.1, false, false, {}, 0.1 });

    controls_.push_back({ PARAM_LFO2_TARGET, "LFO2 TGT", ControlType::ModeSelector, 218, 357, 0, 0.0, 18.0, 4.0, true, false, targetOpts, 4.0 });
    controls_.push_back({ PARAM_LFO2_DEPTH, "", ControlType::Knob, 218, 390, 18, 0.0, 1.0, 0.5, false, true, {}, 0.5 });

    // 6. ENVELOPES (ENV1 & ENV2) (x: 285..755, y: 245..465)
    controls_.push_back({ PARAM_ENV1_A, "ENV1 ATK", ControlType::Knob, 325, 310, 18, 0.0, 1.0, 0.01, false, false, {}, 0.01 });
    controls_.push_back({ PARAM_ENV1_D, "ENV1 DEC", ControlType::Knob, 380, 310, 18, 0.0, 1.0, 0.3, false, false, {}, 0.3 });
    controls_.push_back({ PARAM_ENV1_S, "ENV1 SUS", ControlType::Knob, 435, 310, 18, 0.0, 1.0, 0.4, false, false, {}, 0.4 });
    controls_.push_back({ PARAM_ENV1_R, "ENV1 REL", ControlType::Knob, 490, 310, 18, 0.0, 1.0, 0.3, false, false, {}, 0.3 });

    controls_.push_back({ PARAM_ENV1_TARGET, "ENV1 TGT", ControlType::ModeSelector, 555, 277, 0, 0.0, 18.0, 0.0, true, false, targetOpts, 0.0 });
    controls_.push_back({ PARAM_ENV1_AMT, "", ControlType::Knob, 555, 310, 18, 0.0, 1.0, 0.75, false, true, {}, 0.75 });

    controls_.push_back({ PARAM_ENV2_A, "ENV2 ATK", ControlType::Knob, 325, 390, 18, 0.0, 1.0, 0.01, false, false, {}, 0.01 });
    controls_.push_back({ PARAM_ENV2_D, "ENV2 DEC", ControlType::Knob, 380, 390, 18, 0.0, 1.0, 0.3, false, false, {}, 0.3 });
    controls_.push_back({ PARAM_ENV2_S, "ENV2 SUS", ControlType::Knob, 435, 390, 18, 0.0, 1.0, 0.7, false, false, {}, 0.7 });
    controls_.push_back({ PARAM_ENV2_R, "ENV2 REL", ControlType::Knob, 490, 390, 18, 0.0, 1.0, 0.3, false, false, {}, 0.3 });

    // 7. OUTPUT & MASTER (x: 765..965, y: 245..465)
    controls_.push_back({ PARAM_WARMTH, "WARMTH", ControlType::Knob, 815, 350, 20, 0.0, 1.0, 0.0, false, false, {}, 0.0 });
    controls_.push_back({ PARAM_VOLUME, "MASTER VOL", ControlType::Knob, 905, 350, 24, 0.0, 1.0, 0.8, false, false, {}, 0.8 });

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

    // Render Header Preset Control: PRESETS: [<] [ 01: Hellfire Bass ] [>] [SAVE]
    int presetX = 220;
    g.drawText(presetX, y + 2, "PRESETS:", 0xFFD89A40, font_, 1);

    // [<] button
    g.drawRect(presetX + 70, y, 16, 14, 0xFF101214);
    g.drawRectOutline(presetX + 70, y, 16, 14, 0xFF8C5224, 1);
    g.drawText(presetX + 75, y + 3, "<", 0xFFFF8A00, font_, 1);

    // Preset Display Box
    g.drawRect(presetX + 90, y, 160, 14, 0xFF0E1012);
    g.drawRectOutline(presetX + 90, y, 160, 14, 0xFF8C5224, 1);
    const char* presetName = kFactoryPresetNames[currentPresetIndex_ % kNumFactoryPresets];
    g.drawText(presetX + 95, y + 3, presetName, 0xFFFFCC00, font_, 1);

    // [>] button
    g.drawRect(presetX + 254, y, 16, 14, 0xFF101214);
    g.drawRectOutline(presetX + 254, y, 16, 14, 0xFF8C5224, 1);
    g.drawText(presetX + 259, y + 3, ">", 0xFFFF8A00, font_, 1);

    // [SAVE] button
    g.drawRect(presetX + 274, y, 40, 14, 0xFF101214);
    g.drawRectOutline(presetX + 274, y, 40, 14, 0xFF8C5224, 1);
    g.drawText(presetX + 281, y + 3, "SAVE", 0xFFD89A40, font_, 1);

    // Currently touched/active control display bar
    int touchedX = 550;
    g.drawRect(touchedX, y, 410, 14, 0xFF0A0C0D);
    g.drawRectOutline(touchedX, y, 410, 14, 0xFF8C5224, 1);

    char touchedBuf[96];
    if (activeControlIndex_ >= 0 && activeControlIndex_ < static_cast<int>(controls_.size())) {
        const auto& activeCtrl = controls_[activeControlIndex_];
        if (activeCtrl.type == ControlType::ModeSelector) {
            int targetIdx = std::clamp(static_cast<int>(activeCtrl.currentVal + 0.5), 0, 18);
            snprintf(touchedBuf, sizeof(touchedBuf), "%s: %s", activeCtrl.label, kTargetFullNames[targetIdx]);
        } else {
            char valText[32];
            if (plugin_) {
                plugin_->paramsValueToText(activeCtrl.id, activeCtrl.currentVal, valText, sizeof(valText));
            } else {
                snprintf(valText, sizeof(valText), "%.2f", activeCtrl.currentVal);
            }
            if (activeCtrl.label && strlen(activeCtrl.label) > 0) {
                snprintf(touchedBuf, sizeof(touchedBuf), "%s: %s", activeCtrl.label, valText);
            } else {
                snprintf(touchedBuf, sizeof(touchedBuf), "Mod Amount: %s", valText);
            }
        }
    } else {
        snprintf(touchedBuf, sizeof(touchedBuf), "TOUCH CONTROL TO VIEW VALUE");
    }
    g.drawText(touchedX + 8, y + 3, touchedBuf, 0xFF00E5FF, font_, 1);
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

    // Helper lambda to query effective normalized value per parameter ID
    auto getEffectiveVal = [&](int paramId, double curVal) -> float {
        if (!plugin_) return static_cast<float>(curVal);
        const auto& eng = plugin_->getEngine();
        switch (paramId) {
            case PARAM_CUTOFF:       return eng.getEffectiveNormForTarget(ModTarget::Cutoff);
            case PARAM_RESONANCE:    return eng.getEffectiveNormForTarget(ModTarget::Resonance);
            case PARAM_VCO1_PW:      return eng.getEffectiveNormForTarget(ModTarget::Pw1);
            case PARAM_VCO2_PW:      return eng.getEffectiveNormForTarget(ModTarget::Pw2);
            case PARAM_VCO2_DETUNE:  return eng.getEffectiveNormForTarget(ModTarget::Detune);
            case PARAM_FM_AMOUNT:    return eng.getEffectiveNormForTarget(ModTarget::FmAmount);
            case PARAM_VCO1_VOL:     return eng.getEffectiveNormForTarget(ModTarget::Vco1Vol);
            case PARAM_VCO2_VOL:     return eng.getEffectiveNormForTarget(ModTarget::Vco2Vol);
            case PARAM_SUB_VOL:      return eng.getEffectiveNormForTarget(ModTarget::SubVol);
            case PARAM_RING_MOD:     return eng.getEffectiveNormForTarget(ModTarget::RingMod);
            case PARAM_NOISE_VOL:    return eng.getEffectiveNormForTarget(ModTarget::NoiseVol);
            case PARAM_PRE_DRIVE:    return eng.getEffectiveNormForTarget(ModTarget::PreDrive);
            case PARAM_OVERDRIVE:    return eng.getEffectiveNormForTarget(ModTarget::TubeDrive);
            case PARAM_VOLUME:       return eng.getEffectiveNormForTarget(ModTarget::Amp);
            case PARAM_LFO1_RATE:    return eng.getEffectiveNormForTarget(ModTarget::Lfo1Rate);
            case PARAM_LFO1_DEPTH:   return eng.getEffectiveNormForTarget(ModTarget::Lfo1Amount);
            case PARAM_LFO2_RATE:    return eng.getEffectiveNormForTarget(ModTarget::Lfo2Rate);
            case PARAM_LFO2_DEPTH:   return eng.getEffectiveNormForTarget(ModTarget::Lfo2Amount);
            default:                 return static_cast<float>(curVal);
        }
    };

    // Helper function to check if a knob parameter ID maps to a valid ModTarget
    auto getModTargetForParam = [](int paramId) -> int {
        switch (paramId) {
            case PARAM_CUTOFF:       return static_cast<int>(ModTarget::Cutoff);
            case PARAM_RESONANCE:    return static_cast<int>(ModTarget::Resonance);
            case PARAM_VCO1_PW:      return static_cast<int>(ModTarget::Pw1);
            case PARAM_VCO2_PW:      return static_cast<int>(ModTarget::Pw2);
            case PARAM_VCO2_DETUNE:  return static_cast<int>(ModTarget::Detune);
            case PARAM_FM_AMOUNT:    return static_cast<int>(ModTarget::FmAmount);
            case PARAM_VCO1_VOL:     return static_cast<int>(ModTarget::Vco1Vol);
            case PARAM_VCO2_VOL:     return static_cast<int>(ModTarget::Vco2Vol);
            case PARAM_SUB_VOL:      return static_cast<int>(ModTarget::SubVol);
            case PARAM_RING_MOD:     return static_cast<int>(ModTarget::RingMod);
            case PARAM_NOISE_VOL:    return static_cast<int>(ModTarget::NoiseVol);
            case PARAM_PRE_DRIVE:    return static_cast<int>(ModTarget::PreDrive);
            case PARAM_OVERDRIVE:    return static_cast<int>(ModTarget::TubeDrive);
            case PARAM_VOLUME:       return static_cast<int>(ModTarget::Amp);
            case PARAM_LFO1_RATE:    return static_cast<int>(ModTarget::Lfo1Rate);
            case PARAM_LFO1_DEPTH:   return static_cast<int>(ModTarget::Lfo1Amount);
            case PARAM_LFO2_RATE:    return static_cast<int>(ModTarget::Lfo2Rate);
            case PARAM_LFO2_DEPTH:   return static_cast<int>(ModTarget::Lfo2Amount);
            default:                 return -1;
        }
    };

    // 2. Render Controls
    if (controlRenderer_) {
        for (size_t i = 0; i < controls_.size(); ++i) {
            const auto& ctrl = controls_[i];
            if (ctrl.type == ControlType::Knob) {
                float effVal = getEffectiveVal(ctrl.id, ctrl.currentVal);
                bool isHighlight = (targetSelectingControlIndex_ >= 0 && getModTargetForParam(ctrl.id) >= 0);
                controlRenderer_->drawKnobModulated(g, ctrl, font_, effVal, isHighlight);
            } else if (ctrl.type == ControlType::ModeSelector) {
                bool isSelecting = (targetSelectingControlIndex_ == static_cast<int>(i));
                controlRenderer_->drawModeSelector(g, ctrl, font_, isSelecting);
            } else if (ctrl.type == ControlType::ToggleSwitch) {
                controlRenderer_->drawToggleSwitch(g, ctrl, font_);
            } else if (ctrl.type == ControlType::PushButton) {
                controlRenderer_->drawPushButton(g, ctrl, font_);
            }
        }
    }

    // 3. Header title on chassis
    drawGritbaalTitle(g, 15, 3);

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

void GuiWindow::handleRightClick(int x, int y) {
    auto getModTargetForParam = [](int paramId) -> int {
        switch (paramId) {
            case PARAM_CUTOFF:       return static_cast<int>(ModTarget::Cutoff);
            case PARAM_RESONANCE:    return static_cast<int>(ModTarget::Resonance);
            case PARAM_VCO1_PW:      return static_cast<int>(ModTarget::Pw1);
            case PARAM_VCO2_PW:      return static_cast<int>(ModTarget::Pw2);
            case PARAM_VCO2_DETUNE:  return static_cast<int>(ModTarget::Detune);
            case PARAM_FM_AMOUNT:    return static_cast<int>(ModTarget::FmAmount);
            case PARAM_VCO1_VOL:     return static_cast<int>(ModTarget::Vco1Vol);
            case PARAM_VCO2_VOL:     return static_cast<int>(ModTarget::Vco2Vol);
            case PARAM_SUB_VOL:      return static_cast<int>(ModTarget::SubVol);
            case PARAM_RING_MOD:     return static_cast<int>(ModTarget::RingMod);
            case PARAM_NOISE_VOL:    return static_cast<int>(ModTarget::NoiseVol);
            case PARAM_PRE_DRIVE:    return static_cast<int>(ModTarget::PreDrive);
            case PARAM_OVERDRIVE:    return static_cast<int>(ModTarget::TubeDrive);
            case PARAM_VOLUME:       return static_cast<int>(ModTarget::Amp);
            case PARAM_LFO1_RATE:    return static_cast<int>(ModTarget::Lfo1Rate);
            case PARAM_LFO1_DEPTH:   return static_cast<int>(ModTarget::Lfo1Amount);
            case PARAM_LFO2_RATE:    return static_cast<int>(ModTarget::Lfo2Rate);
            case PARAM_LFO2_DEPTH:   return static_cast<int>(ModTarget::Lfo2Amount);
            default:                 return -1;
        }
    };

    for (size_t i = 0; i < controls_.size(); ++i) {
        auto& ctrl = controls_[i];
        if (ctrl.type == ControlType::ModeSelector) {
            if (std::abs(x - ctrl.x) <= 30 && std::abs(y - ctrl.y) <= 12) {
                if (targetSelectingControlIndex_ == static_cast<int>(i)) {
                    // Cancel target selection mode
                    targetSelectingControlIndex_ = -1;
                } else {
                    // Enter target selection mode for this ModeSelector
                    targetSelectingControlIndex_ = static_cast<int>(i);
                }
                renderFrame();
                return;
            }
        } else if (ctrl.type == ControlType::Knob) {
            int dx = x - ctrl.x;
            int dy = y - ctrl.y;
            if (dx * dx + dy * dy <= (ctrl.radius + 10) * (ctrl.radius + 10)) {
                if (targetSelectingControlIndex_ >= 0) {
                    int targetIdx = getModTargetForParam(ctrl.id);
                    if (targetIdx >= 0) {
                        auto& selCtrl = controls_[targetSelectingControlIndex_];
                        selCtrl.currentVal = static_cast<double>(targetIdx);
                        if (plugin_) {
                            plugin_->onParamValueFromGui(selCtrl.id, selCtrl.currentVal);
                        }
                    }
                    targetSelectingControlIndex_ = -1;
                    renderFrame();
                    return;
                }
            }
        }
    }

    // Right clicking anywhere else cancels target selection mode
    if (targetSelectingControlIndex_ >= 0) {
        targetSelectingControlIndex_ = -1;
        renderFrame();
    }
}

void GuiWindow::handleMouseDown(int x, int y, bool isShift) {
    if (targetSelectingControlIndex_ >= 0) {
        targetSelectingControlIndex_ = -1;
        renderFrame();
    }
    lastShiftState_ = isShift;

    // Double click detection
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    bool isDoubleClick = ((nowMs - lastClickTimestampMs_) < 300) &&
                         (std::abs(x - lastClickX_) <= 5) && (std::abs(y - lastClickY_) <= 5);
    lastClickTimestampMs_ = static_cast<uint32_t>(nowMs);
    lastClickX_ = x;
    lastClickY_ = y;

    // Check click on Preset Header [<] and [>] buttons
    if (y >= 0 && y <= 20) {
        int presetX = 220;
        if (x >= presetX + 70 && x <= presetX + 86) {
            // [<] clicked
            currentPresetIndex_ = (currentPresetIndex_ + kNumFactoryPresets - 1) % kNumFactoryPresets;
            renderFrame();
            return;
        }
        if (x >= presetX + 254 && x <= presetX + 270) {
            // [>] clicked
            currentPresetIndex_ = (currentPresetIndex_ + 1) % kNumFactoryPresets;
            renderFrame();
            return;
        }
    }

    for (size_t i = 0; i < controls_.size(); ++i) {
        auto& ctrl = controls_[i];
        if (ctrl.type == ControlType::Knob) {
            int dx = x - ctrl.x;
            int dy = y - ctrl.y;
            if (dx * dx + dy * dy <= (ctrl.radius + 10) * (ctrl.radius + 10)) {
                activeControlIndex_ = static_cast<int>(i);
                if (isDoubleClick) {
                    ctrl.currentVal = ctrl.defaultVal;
                    if (plugin_) {
                        plugin_->onParamValueFromGui(ctrl.id, ctrl.currentVal);
                    }
                }
                dragStartY_ = y;
                dragStartVal_ = ctrl.currentVal;
                if (plugin_) {
                    plugin_->onBeginEditFromGui(ctrl.id);
                }
                renderFrame();
                break;
            }
        } else if (ctrl.type == ControlType::ModeSelector) {
            if (std::abs(x - ctrl.x) <= 30 && std::abs(y - ctrl.y) <= 12) {
                activeControlIndex_ = static_cast<int>(i);
                dragStartY_ = y;
                dragStartVal_ = ctrl.currentVal;
                if (plugin_) {
                    plugin_->onBeginEditFromGui(ctrl.id);
                }
                renderFrame();
                break;
            }
        } else if (ctrl.type == ControlType::ToggleSwitch) {
            if (std::abs(x - ctrl.x) <= 20 && std::abs(y - ctrl.y) <= 25) {
                activeControlIndex_ = static_cast<int>(i);
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
    if (ctrl.type == ControlType::ModeSelector) {
        int deltaY = dragStartY_ - y;
        int stepChange = deltaY / 12;
        int numOpts = static_cast<int>(ctrl.options.size());
        int newIdx = std::clamp(static_cast<int>(dragStartVal_) + stepChange, 0, numOpts - 1);
        if (newIdx != static_cast<int>(ctrl.currentVal)) {
            ctrl.currentVal = static_cast<double>(newIdx);
            if (plugin_) {
                plugin_->onParamValueFromGui(ctrl.id, ctrl.currentVal);
            }
            renderFrame();
        }
        return;
    }

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
    // Retain activeControlIndex_ for display in header until next interaction
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
                if (ev.xbutton.button == Button3) {
                    handleRightClick(ev.xbutton.x, ev.xbutton.y);
                } else if (ev.xbutton.button == Button1) {
                    handleMouseDown(ev.xbutton.x, ev.xbutton.y, isShift);
                }
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
        case WM_RBUTTONDOWN: {
            if (gui) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                gui->handleRightClick(x, y);
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
        0, kGritbaalClassName, L"Gritbaal Synthesizer",
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
        *width = 980;
        *height = 480;
        return true;
    },
    [](const clap_plugin_t* plugin) -> bool {
        return false;
    },
    [](const clap_plugin_t* plugin, clap_gui_resize_hints_t* hints) -> bool {
        return false;
    },
    [](const clap_plugin_t* plugin, uint32_t* width, uint32_t* height) -> bool {
        *width = 980;
        *height = 480;
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
