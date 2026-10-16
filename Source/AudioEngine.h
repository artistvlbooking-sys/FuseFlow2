#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class AudioEngine final : public juce::AudioIODeviceCallback
{
public:
    using Completion = std::function<void (juce::String)>;

    struct PluginInfo
    {
        std::uint64_t id = 0;
        juce::String name;
        juce::String format;
        bool bypassed = false;
    };

    AudioEngine();
    ~AudioEngine() override;

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }
    juce::Array<juce::PluginDescription> getDiscoveredPlugins() const;
    std::vector<PluginInfo> getPluginList() const;

    void scanVst3DirectoryAsync (const juce::File& directory, Completion completion);
    void loadPluginAsync (const juce::PluginDescription& description, Completion completion);
    void removePlugin (std::uint64_t id);
    void movePlugin (std::uint64_t id, int direction);
    void setPluginBypassed (std::uint64_t id, bool shouldBypass);
    juce::AudioProcessor* getPluginProcessor (std::uint64_t id);

    bool startRecording (const juce::File& file, juce::String& error);
    void stopRecording();
    bool isRecording() const noexcept { return recording.load(); }

    void setMonitorEnabled (bool enabled) noexcept { monitorEnabled.store (enabled); }
    bool isMonitorEnabled() const noexcept { return monitorEnabled.load(); }
    void setInputGainDb (float db) noexcept;
    void setOutputGainDb (float db) noexcept;
    float getInputGainDb() const noexcept { return inputGainDb.load(); }
    float getOutputGainDb() const noexcept { return outputGainDb.load(); }

    float getInputPeak() const noexcept { return inputPeak.load(); }
    float getOutputPeak() const noexcept { return outputPeak.load(); }
    double getSampleRate() const noexcept { return sampleRate.load(); }
    int getBufferSize() const noexcept { return bufferSize.load(); }
    juce::String getInputDeviceName() const;
    juce::String getOutputDeviceName() const;
    juce::String getDeviceTypeName() const;
    juce::String getDeviceError() const;

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceError (const juce::String& errorMessage) override;

private:
    struct PluginSlot
    {
        std::uint64_t id = 0;
        std::unique_ptr<juce::AudioPluginInstance> processor;
        std::atomic<bool> bypassed { false };
    };

    class ScanJob;
    juce::String scanVst3Directory (const juce::File& directory);
    juce::File getAppDataDirectory() const;
    juce::AudioPluginFormat* getVst3Format() const;
    void savePluginCache();
    void loadPluginCache();

    juce::AudioDeviceManager deviceManager;
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;

    mutable std::mutex chainMutex;
    std::vector<std::unique_ptr<PluginSlot>> plugins;
    std::atomic<std::uint64_t> nextPluginId { 1 };

    std::mutex pluginListMutex;
    std::mutex writerMutex;
    juce::TimeSliceThread writerThread { "FuseFlow WAV writer" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> activeWriter;

    std::array<std::vector<float>, 2> scratch;
    juce::MidiBuffer midiBuffer;
    std::atomic<bool> recording { false };
    std::atomic<bool> monitorEnabled { true };
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<float> inputGainDb { 0.0f };
    std::atomic<float> outputGainDb { 0.0f };
    std::atomic<double> sampleRate { 0.0 };
    std::atomic<int> bufferSize { 0 };

    mutable std::mutex statusMutex;
    juce::String deviceError;

    // Declared last so background scans finish before the plugin format manager
    // and the known-plugin list are destroyed.
    juce::ThreadPool scannerPool { 1 };
};
