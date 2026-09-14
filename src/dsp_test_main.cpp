#include "core/SynthEngine.hpp"
#include <vector>
#include <fstream>
#include <iostream>
#include <cmath>

void writeWav(const std::string& filename, const std::vector<float>& samples, int sampleRate = 44100) {
    std::ofstream file(filename, std::ios::binary);

    int numSamples = static_cast<int>(samples.size());
    int dataSize = numSamples * 2;
    int chunkSize = 36 + dataSize;
    int byteRate = sampleRate * 2;

    // RIFF Header
    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&chunkSize), 4);
    file.write("WAVE", 4);

    // fmt chunk
    file.write("fmt ", 4);
    int subchunk1Size = 16;
    short audioFormat = 1; // PCM
    short numChannels = 1; // Mono
    short bitsPerSample = 16;
    short blockAlign = 2;

    file.write(reinterpret_cast<const char*>(&subchunk1Size), 4);
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    file.write(reinterpret_cast<const char*>(&numChannels), 2);
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // data chunk
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);

    for (float s : samples) {
        float clamped = std::max(-1.0f, std::min(1.0f, s));
        short intVal = static_cast<short>(clamped * 32767.0f);
        file.write(reinterpret_cast<const char*>(&intVal), 2);
    }

    std::cout << "Wrote " << filename << " (" << samples.size() << " samples)\n";
}

static float runTestForMode(syrebas::EmulationMode mode, const std::string& wavFilename) {
    syrebas::SynthEngine engine;
    engine.setSampleRate(44100.0);

    auto& params = engine.getParams();
    params.mode = mode;
    params.cutoff = 0.4f;
    params.resonance = 0.85f;
    params.envMod = 0.8f;
    params.decay = 0.5f;
    params.accent = 0.9f;
    params.waveform = syrebas::Waveform::Saw;
    params.masterVolume = 0.8f;

    std::vector<float> audioBuffer;
    int sampleRate = 44100;
    int frameSize = 256;
    std::vector<float> left(frameSize);
    std::vector<float> right(frameSize);

    struct Event {
        int sampleOffset;
        bool isNoteOn;
        int note;
        float vel;
    };

    std::vector<Event> events = {
        { 0, true, 36, 0.5f },                 // C2 normal
        { sampleRate / 4, true, 36, 1.0f },    // C2 Accent 1
        { sampleRate / 2, true, 36, 1.0f },    // C2 Accent 2
        { 3 * sampleRate / 4, true, 36, 1.0f },// C2 Accent 3
        { sampleRate, true, 48, 1.0f },       // C3 slide + accent
        { 5 * sampleRate / 4, false, 48, 0.0f },// Note off
        { 3 * sampleRate / 2, true, 43, 0.5f },// G2 normal
        { 7 * sampleRate / 4, true, 36, 0.5f },// C2 slide
        { sampleRate * 2, false, 36, 0.0f }
    };

    int currentSample = 0;
    int totalSamples = sampleRate * 3;
    size_t eventIdx = 0;

    while (currentSample < totalSamples) {
        while (eventIdx < events.size() && events[eventIdx].sampleOffset <= currentSample) {
            const auto& ev = events[eventIdx];
            if (ev.isNoteOn) {
                engine.noteOn(ev.note, ev.vel);
            } else {
                engine.noteOff(ev.note);
            }
            eventIdx++;
        }

        engine.processAudio(left.data(), right.data(), frameSize);
        for (int i = 0; i < frameSize; ++i) {
            audioBuffer.push_back(left[i]);
        }
        currentSample += frameSize;
    }

    writeWav(wavFilename, audioBuffer, sampleRate);

    bool hasNonZeroOutput = false;
    float maxAbs = 0.0f;
    for (float sample : audioBuffer) {
        if (std::abs(sample) > 0.0001f) {
            hasNonZeroOutput = true;
        }
        if (std::abs(sample) > maxAbs) {
            maxAbs = std::abs(sample);
        }
    }

    if (!hasNonZeroOutput) {
        std::cerr << "ERROR: Audio buffer is silent for " << wavFilename << "!\n";
        exit(1);
    }

    if (maxAbs > 1.5f) {
        std::cerr << "ERROR: Output clipped abnormally for " << wavFilename << "! Max abs: " << maxAbs << "\n";
        exit(1);
    }

    return maxAbs;
}

int main() {
    float accurateMax = runTestForMode(syrebas::EmulationMode::Accurate, "test_syrebas_accurate.wav");
    std::cout << "Accurate mode DSP test completed. Max peak amplitude: " << accurateMax << "\n";

    float simplifiedMax = runTestForMode(syrebas::EmulationMode::Simplified, "test_syrebas_simplified.wav");
    std::cout << "Simplified mode DSP test completed. Max peak amplitude: " << simplifiedMax << "\n";

    std::cout << "All DSP tests completed successfully.\n";
    return 0;
}
