#include "core/SynthEngine.hpp"
#include <vector>
#include <fstream>
#include <iostream>
#include <cmath>
#include <cassert>

void writeWav(const std::string& filename, const std::vector<float>& samples, int sampleRate = 44100) {
    std::ofstream file(filename, std::ios::binary);

    int numSamples = static_cast<int>(samples.size());
    int dataSize = numSamples * 2;
    int chunkSize = 36 + dataSize;
    int byteRate = sampleRate * 2;

    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&chunkSize), 4);
    file.write("WAVE", 4);

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

    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);

    for (float s : samples) {
        float clamped = std::max(-1.0f, std::min(1.0f, s));
        short intVal = static_cast<short>(clamped * 32767.0f);
        file.write(reinterpret_cast<const char*>(&intVal), 2);
    }

    std::cout << "Wrote " << filename << " (" << samples.size() << " samples)\n";
}

static float runDspTest(const std::string& wavFilename) {
    gritbaal::SynthEngine engine;
    engine.setSampleRate(44100.0);

    auto& params = engine.getParams();
    params.cutoff = 0.4f;
    params.resonance = 0.85f;
    params.env1Amount = 0.8f;
    params.env1Target = gritbaal::ModTarget::Cutoff;
    params.waveform = gritbaal::Waveform::Saw;
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
        { sampleRate / 4, true, 36, 1.0f },    // C2 Note On
        { sampleRate / 2, true, 36, 1.0f },    // C2 Note On
        { 3 * sampleRate / 4, true, 36, 1.0f },// C2 Note On
        { sampleRate, true, 48, 1.0f },       // C3
        { 5 * sampleRate / 4, false, 48, 0.0f },// Note off
        { 3 * sampleRate / 2, true, 43, 0.5f },// G2 normal
        { 7 * sampleRate / 4, true, 36, 0.5f },// C2
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

static void testPhase2ExtendedDsp() {
    std::cout << "\n--- Testing Phase 2 Extended Core DSP Engine ---" << std::endl;
    gritbaal::SynthEngine engine;
    engine.setSampleRate(44100.0);

    auto& params = engine.getParams();
    params.vco2Waveform = gritbaal::Waveform::Pulse;
    params.vco1Level = 0.7f;
    params.vco2Level = 0.6f;
    params.vco2Detune = 7.0f; // Perfect fifth
    params.fmAmount = 0.3f;
    params.hardSync = true;
    params.subLevel = 0.4f;
    params.noiseLevel = 0.2f;
    params.noiseType = gritbaal::NoiseType::Crackle;
    params.warmthAmount = 0.5f;
    params.lfo1Rate = 2.0f;
    params.lfo1Depth = 0.3f;
    params.lfo2Rate = 3.0f;
    params.lfo2Depth = 0.2f;
    params.filterModel = gritbaal::VintageFilterModel::MS20;
    params.preFilterDrive = 2.5f;
    params.overdriveAmount = 0.5f;
    params.powerSagAmount = 0.4f;
    params.thermalDrift = 0.2f;

    engine.noteOn(36, 0.9f); // Note On C2

    std::vector<float> left(512);
    std::vector<float> right(512);
    engine.processAudio(left.data(), right.data(), 512);

    float maxAbs = 0.0f;
    for (int i = 0; i < 512; ++i) {
        assert(!std::isnan(left[i]) && !std::isinf(left[i]));
        if (std::abs(left[i]) > maxAbs) {
            maxAbs = std::abs(left[i]);
        }
    }

    std::cout << "Phase 2 Extended DSP Test Passed! Peak output: " << maxAbs << std::endl;
    assert(maxAbs > 0.01f);
}

static void testAllVintageFilterModels() {
    std::cout << "\n--- Testing all VintageFilterModel presets ---" << std::endl;

    const gritbaal::VintageFilterModel models[] = {
        gritbaal::VintageFilterModel::Minimoog,
        gritbaal::VintageFilterModel::Arp2600,
        gritbaal::VintageFilterModel::TB303,
        gritbaal::VintageFilterModel::MS20,
    };
    const char* names[] = { "Minimoog", "ARP2600", "TB303", "MS20" };

    for (int m = 0; m < 4; ++m) {
        gritbaal::SynthEngine engine;
        engine.setSampleRate(44100.0);

        auto& params = engine.getParams();
        params.cutoff = 0.5f;
        params.resonance = 0.95f; // push near self-oscillation to stress the solver
        params.filterModel = models[m];
        params.waveform = gritbaal::Waveform::Saw;
        params.masterVolume = 0.8f;

        engine.noteOn(36, 1.0f);

        std::vector<float> left(2048);
        std::vector<float> right(2048);
        engine.processAudio(left.data(), right.data(), 2048);

        float maxAbs = 0.0f;
        for (float sample : left) {
            assert(!std::isnan(sample) && !std::isinf(sample));
            maxAbs = std::max(maxAbs, std::abs(sample));
        }

        if (maxAbs > 1.5f) {
            std::cerr << "ERROR: " << names[m] << " filter model output exploded! Max abs: " << maxAbs << "\n";
            exit(1);
        }

        std::cout << "  " << names[m] << " OK, peak output: " << maxAbs << std::endl;
    }

    std::cout << "All VintageFilterModel presets stable." << std::endl;
}

int main() {
    float accurateMax = runDspTest("test_gritbaal_accurate.wav");
    std::cout << "Core DSP test completed. Max peak amplitude: " << accurateMax << "\n";

    testPhase2ExtendedDsp();
    testAllVintageFilterModels();

    std::cout << "All Phase 2 DSP tests completed successfully.\n";
    return 0;
}
