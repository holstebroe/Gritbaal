#ifndef GRITBAAL_CLAP_HPP
#define GRITBAAL_CLAP_HPP

#include <clap/clap.h>
#include "core/SynthEngine.hpp"
#include <memory>
#include <vector>
#include <mutex>

namespace gritbaal {

struct GuiParamEvent {
    uint16_t type; // CLAP_EVENT_PARAM_GESTURE_BEGIN, CLAP_EVENT_PARAM_VALUE, CLAP_EVENT_PARAM_GESTURE_END
    clap_id paramId;
    double value;
    uint32_t flags; // e.g. CLAP_EVENT_IS_LIVE, CLAP_EVENT_DONT_RECORD
};

// Parameter IDs
enum ParamId : clap_id {
    // VCF & Main
    PARAM_CUTOFF = 0,
    PARAM_RESONANCE = 1,
    PARAM_WAVEFORM = 2,
    PARAM_VOLUME = 3,

    // Dual ADSR Envelopes
    PARAM_ENV1_A = 4,
    PARAM_ENV1_D = 5,
    PARAM_ENV1_S = 6,
    PARAM_ENV1_R = 7,
    PARAM_ENV1_TARGET = 8,
    PARAM_ENV1_AMT = 9,

    PARAM_ENV2_A = 10,
    PARAM_ENV2_D = 11,
    PARAM_ENV2_S = 12,
    PARAM_ENV2_R = 13,

    // Dual LFOs
    PARAM_LFO1_RATE = 14,
    PARAM_LFO1_TARGET = 15,
    PARAM_LFO1_DEPTH = 16,
    PARAM_LFO1_SYNC = 17,

    PARAM_LFO2_RATE = 18,
    PARAM_LFO2_TARGET = 19,
    PARAM_LFO2_DEPTH = 20,
    PARAM_LFO2_SYNC = 21,

    // VCO Section
    PARAM_VCO1_WAVE = 22,
    PARAM_VCO1_PW = 23,
    PARAM_VCO2_WAVE = 24,
    PARAM_VCO2_PW = 25,
    PARAM_VCO2_DETUNE = 26,
    PARAM_FM_AMOUNT = 27,
    PARAM_HARD_SYNC = 28,

    // Mix & Drive Section
    PARAM_VCO1_VOL = 29,
    PARAM_VCO2_VOL = 30,
    PARAM_RING_MOD = 31,
    PARAM_SUB_VOL = 32,
    PARAM_NOISE_VOL = 33,
    PARAM_PRE_DRIVE = 34,
    PARAM_OVERDRIVE = 35,
    PARAM_WARMTH = 36,

    // VCF Type
    PARAM_FILTER_TYPE = 37,

    // Global, Drift & Sag
    PARAM_THERMAL_DRIFT = 38,
    PARAM_POWER_SAG = 39,

    PARAM_COUNT = 40
};

enum MidiParamId : clap_id {
    MIDI_PARAM_CUTOFF = 71,
    MIDI_PARAM_RESONANCE = 72,
    MIDI_PARAM_ENV1_D = 74,
    MIDI_PARAM_WARMTH = 22,
    MIDI_PARAM_WAVEFORM = 23,
    MIDI_PARAM_VOLUME = 20,
    MIDI_PARAM_COUNT = 7
};

class GritbaalClap {
public:
    explicit GritbaalClap(const clap_host_t* host);
    ~GritbaalClap() = default;

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

} // namespace gritbaal

#endif // GRITBAAL_CLAP_HPP
