#include "clap/GritbaalClap.hpp"
#include "gui/GuiWindow.hpp"
#include "gui/Graphics.hpp"
#include "gui/Font.hpp"
#include "gui/IControlRenderer.hpp"
#include <fstream>
#include <iostream>
#include <cassert>
#include <cmath>

int main() {
    gritbaal::GritbaalClap plugin(nullptr);
    gritbaal::GuiWindow gui(&plugin);

    gui.renderFrame();

    const auto& buffer = gui.getPixelBuffer();
    uint32_t w = gui.getWidth();
    uint32_t h = gui.getHeight();

    // Write raw ARGB buffer
    std::ofstream ofs("/tmp/gritbaal_gui_buffer.raw", std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(buffer.data()), buffer.size() * sizeof(uint32_t));
    ofs.close();

    std::cout << "GUI Frame rendered: " << w << "x" << h << ", saved to /tmp/gritbaal_gui_buffer.raw" << std::endl;

    // Mock output event collector
    struct TestOutEvents {
        std::vector<uint16_t> types;
        std::vector<clap_id> paramIds;
        std::vector<double> values;
        std::vector<uint32_t> flags;

        static bool tryPush(const clap_output_events_t* list, const clap_event_header_t* event) {
            auto* self = static_cast<TestOutEvents*>(list->ctx);
            self->types.push_back(event->type);
            if (event->type == CLAP_EVENT_PARAM_GESTURE_BEGIN || event->type == CLAP_EVENT_PARAM_GESTURE_END) {
                const auto* ge = reinterpret_cast<const clap_event_param_gesture_t*>(event);
                self->paramIds.push_back(ge->param_id);
                self->values.push_back(0.0);
            } else if (event->type == CLAP_EVENT_PARAM_VALUE) {
                const auto* ve = reinterpret_cast<const clap_event_param_value_t*>(event);
                self->paramIds.push_back(ve->param_id);
                self->values.push_back(ve->value);
            }
            self->flags.push_back(event->flags);
            return true;
        }
    } testCtx;

    clap_output_events_t mockOutList{};
    mockOutList.ctx = &testCtx;
    mockOutList.try_push = TestOutEvents::tryPush;

    // Test mouse interaction and Shift fine tuning
    // Cutoff Knob is at x=580, y=100 (minVal=0.0, maxVal=1.0)
    // 1. Standard mouse drag test (isShift = false)
    gui.handleMouseDown(580, 100, false);
    gui.handleMouseDrag(580, 20, false); // Drag up 80 pixels
    gui.handleMouseUp();

    double valNormal = 0.0;
    plugin.paramsValue(gritbaal::PARAM_CUTOFF, &valNormal);
    std::cout << "Cutoff after 80px normal drag: " << valNormal << std::endl;
    assert(valNormal >= 0.99);

    // Flush GUI output events to mockOutList
    plugin.paramsFlush(nullptr, &mockOutList);

    assert(!testCtx.types.empty());
    assert(testCtx.types.front() == CLAP_EVENT_PARAM_GESTURE_BEGIN);
    assert(testCtx.types.back() == CLAP_EVENT_PARAM_GESTURE_END);
    assert(testCtx.paramIds.front() == gritbaal::PARAM_CUTOFF);
    std::cout << "GUI output event gesture queue test passed successfully! Events recorded: " << testCtx.types.size() << std::endl;

    // 1b. ModeSelector target label drag test
    // LFO1_TARGET ModeSelector is at x=218, y=277
    gui.handleMouseDown(218, 277, false);
    gui.handleMouseDrag(218, 252, false); // Drag UP 25 pixels (2 steps)
    gui.handleMouseUp();

    double lfo1TargetVal = 0.0;
    plugin.paramsValue(gritbaal::PARAM_LFO1_TARGET, &lfo1TargetVal);
    std::cout << "LFO1 Target after 25px UP drag on ModeSelector: " << lfo1TargetVal << std::endl;
    assert(lfo1TargetVal == 2.0); // 0 (Cutoff) + 2 = 2 (Pitch)

    // 1c. Double-click reset test
    gui.handleMouseDown(580, 100, false);
    gui.handleMouseDrag(580, 150, false);
    gui.handleMouseUp();
    gui.handleMouseDown(580, 100, false);
    gui.handleMouseDown(580, 100, false);
    gui.handleMouseUp();

    double resetCutoffVal = 0.0;
    plugin.paramsValue(gritbaal::PARAM_CUTOFF, &resetCutoffVal);
    std::cout << "Cutoff after double click reset: " << resetCutoffVal << std::endl;
    assert(resetCutoffVal == 0.5);

    // 2. Fine mouse drag test (isShift = true)
    gui.handleMouseDown(670, 100, true); // Resonance knob at (670, 100) with Shift
    gui.handleMouseDrag(670, 20, true);  // Drag up 80 pixels with Shift (from initial 0.5)
    gui.handleMouseUp();

    double valFine = 0.0;
    plugin.paramsValue(gritbaal::PARAM_RESONANCE, &valFine);
    std::cout << "Resonance after 80px fine drag with Shift: " << valFine << std::endl;
    assert(std::abs(valFine - 0.70) < 0.01);

    // 3. Test MIDI CC 74 (Cutoff) mapping to absolute range
    testCtx.types.clear();
    testCtx.paramIds.clear();
    testCtx.values.clear();
    testCtx.flags.clear();

    clap_event_midi_t midiCcEv{};
    midiCcEv.header.size = sizeof(midiCcEv);
    midiCcEv.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    midiCcEv.header.type = CLAP_EVENT_MIDI;
    midiCcEv.port_index = 0;
    midiCcEv.data[0] = 0xB0; // Control Change Ch 1
    midiCcEv.data[1] = 71;   // CC 71 (Cutoff)
    midiCcEv.data[2] = 127;  // Max CC value

    // Simulate process block or flush with incoming MIDI CC
    struct TestInEvents {
        const clap_event_header_t* ev;
        static uint32_t size(const clap_input_events_t* list) { return 1; }
        static const clap_event_header_t* get(const clap_input_events_t* list, uint32_t index) {
            auto* self = static_cast<TestInEvents*>(list->ctx);
            return self->ev;
        }
    } inCtx;
    inCtx.ev = &midiCcEv.header;

    clap_input_events_t mockInList{};
    mockInList.ctx = &inCtx;
    mockInList.size = TestInEvents::size;
    mockInList.get = TestInEvents::get;

    plugin.paramsFlush(&mockInList, &mockOutList);

    double ccCutoffVal = 0.0;
    plugin.paramsValue(gritbaal::PARAM_CUTOFF, &ccCutoffVal);
    std::cout << "Cutoff after MIDI CC 74 (127): " << ccCutoffVal << std::endl;
    assert(ccCutoffVal == 1.0);
    assert(!testCtx.types.empty());
    assert(testCtx.types.back() == CLAP_EVENT_PARAM_VALUE);
    assert(testCtx.flags.back() == CLAP_EVENT_DONT_RECORD);

    // 4. Test Font and Custom Control Renderer interface
    gritbaal::Font customFont(6, 8);
    assert(customFont.getWidth() == 6);
    assert(customFont.getHeight() == 8);
    gui.setFont(customFont);
    assert(gui.getFont().getWidth() == 6);

    class TestCustomRenderer : public gritbaal::IControlRenderer {
    public:
        bool knobDrawn = false;
        bool switchDrawn = false;
        void drawKnob(gritbaal::Graphics& g, const gritbaal::Control& ctrl, const gritbaal::Font& font) override {
            knobDrawn = true;
        }
        void drawKnobModulated(gritbaal::Graphics& g, const gritbaal::Control& ctrl, const gritbaal::Font& font, double modValNorm) override {
            knobDrawn = true;
        }
        void drawModeSelector(gritbaal::Graphics& g, const gritbaal::Control& ctrl, const gritbaal::Font& font) override {}
        void drawToggleSwitch(gritbaal::Graphics& g, const gritbaal::Control& ctrl, const gritbaal::Font& font) override {
            switchDrawn = true;
        }
        void drawPushButton(gritbaal::Graphics& g, const gritbaal::Control& ctrl, const gritbaal::Font& font) override {}
        void drawLedIndicator(gritbaal::Graphics& g, int cx, int cy, bool state, uint32_t activeColor = 0xFFFF3300) override {}
    };

    auto customRenderer = std::make_unique<TestCustomRenderer>();
    auto* rawPtr = customRenderer.get();
    gui.setControlRenderer(std::move(customRenderer));
    gui.renderFrame();
    assert(rawPtr->knobDrawn);
    assert(rawPtr->switchDrawn);
    std::cout << "Custom Font and IControlRenderer interface tests passed successfully!" << std::endl;

    std::cout << "Mouse drag, gesture events, MIDI CC, and refactored GUI tests passed successfully!" << std::endl;

    return 0;
}
