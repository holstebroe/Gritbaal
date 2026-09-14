#ifndef SYREBAS_CLAP_HPP
#define SYREBAS_CLAP_HPP

#include <clap/clap.h>
#include "core/SynthEngine.hpp"
#include <memory>
#include <vector>
#include <mutex>

namespace syrebas {

struct GuiParamEvent {
    uint16_t type; // CLAP_EVENT_PARAM_GESTURE_BEGIN, CLAP_EVENT_PARAM_VALUE, CLAP_EVENT_PARAM_GESTURE_END
    clap_id paramId;
    double value;
    uint32_t flags; // e.g. CLAP_EVENT_IS_LIVE, CLAP_EVENT_DONT_RECORD
};

// Parameter IDs
enum ParamId : clap_id {
    PARAM_CUTOFF = 0,
    PARAM_RESONANCE = 1,
    PARAM_ENV_MOD = 2,
    PARAM_DECAY = 3,
    PARAM_ACCENT = 4,
    PARAM_WAVEFORM = 5,
    PARAM_VOLUME = 6,
    PARAM_MODE = 7,
    PARAM_COUNT = 8
};

enum MidiParamId : clap_id {
    MIDI_PARAM_CUTOFF = 71,
    MIDI_PARAM_RESONANCE = 72,
    MIDI_PARAM_ENV_MOD = 73,
    MIDI_PARAM_DECAY = 74,
    MIDI_PARAM_ACCENT = 22,
    MIDI_PARAM_WAVEFORM = 23,
    MIDI_PARAM_VOLUME = 20,
    MIDI_PARAM_MODE = 24,
    MIDI_PARAM_COUNT = 8
};

class SyrebasClap {
public:
    explicit SyrebasClap(const clap_host_t* host);
    ~SyrebasClap() = default;

    const clap_plugin_t* getClapPlugin() const { return &clapPlugin_; }

    // CLAP Core Callbacks
    bool init();
    void destroy();
    bool activate(double sampleRate, uint32_t minFrames, uint32_t maxFrames);
    void deactivate();
    bool startProcessing();
    void stopProcessing();
    void reset();
    clap_process_status process(const clap_process_t* process);
    const void* getExtension(const char* id);
    void onMainThread();

    // CLAP Params Extension
    uint32_t paramsCount() const;
    bool paramsInfo(uint32_t paramIndex, clap_param_info_t* paramInfo) const;
    bool paramsValue(clap_id paramId, double* outValue);
    void setParamValueFromGui(clap_id paramId, double value);
    void onBeginEditFromGui(clap_id paramId);
    void onParamValueFromGui(clap_id paramId, double value);
    void onEndEditFromGui(clap_id paramId);
    bool paramsValueToText(clap_id paramId, double value, char* outBuffer, uint32_t outBufferCapacity);
    bool paramsTextToValue(clap_id paramId, const char* paramValueText, double* outValue);
    void paramsFlush(const clap_input_events_t* in, const clap_output_events_t* out);
    void pushPendingOutputEvents(const clap_output_events_t* out);
    void requestHostFlush();

    // CLAP State Extension
    bool stateSave(const clap_ostream_t* stream);
    bool stateLoad(const clap_istream_t* stream);

    SynthEngine& getEngine() { return engine_; }

    const clap_host_t* getHost() const { return host_; }

    class GuiWindow* getGuiWindow() { return guiWindow_.get(); }
    void createGuiWindow();
    void destroyGuiWindow();

private:
    const clap_host_t* host_{nullptr};
    clap_plugin_t clapPlugin_{};
    SynthEngine engine_;
    std::unique_ptr<class GuiWindow> guiWindow_;

    double paramValues_[PARAM_COUNT]{};

    std::mutex outEventQueueMutex_;
    std::vector<GuiParamEvent> outEventQueue_;

    void handleEvent(const clap_event_header_t* header);
    void syncParamsToEngine();
};

} // namespace syrebas

#endif // SYREBAS_CLAP_HPP
