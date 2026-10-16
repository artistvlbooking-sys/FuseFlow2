#include "AudioEngine.h"

#include <algorithm>
#include <array>
#include <cmath>

class AudioEngine::ScanJob final : public juce::ThreadPoolJob
{
public:
    ScanJob (AudioEngine& engine, juce::File scanDirectory, Completion done)
        : juce::ThreadPoolJob ("FuseFlow VST3 scan"), owner (engine), directory (std::move (scanDirectory)), completion (std::move (done))
    {
    }

    JobStatus runJob() override
    {
        const auto result = owner.scanVst3Directory (directory);
        auto callback = std::move (completion);

        if (callback != nullptr)
            juce::MessageManager::callAsync ([callback = std::move (callback), result]() mutable { callback (result); });

        return jobHasFinished;
    }

private:
    AudioEngine& owner;
    juce::File directory;
    Completion completion;
};

AudioEngine::AudioEngine()
{
    formatManager.addFormat (std::make_unique<juce::VST3PluginFormat>());
    loadPluginCache();
    writerThread.startThread();

    const auto stateFile = getAppDataDirectory().getChildFile ("audio-device-state.xml");
    std::unique_ptr<juce::XmlElement> savedState;
    if (stateFile.existsAsFile())
        savedState = juce::XmlDocument::parse (stateFile);

    const auto error = deviceManager.initialise (1, 2, savedState.get(), true);
    if (error.isNotEmpty())
        audioDeviceError (error);

    deviceManager.addAudioCallback (this);
}

AudioEngine::~AudioEngine()
{
    deviceManager.removeAudioCallback (this);
    stopRecording();

    const auto stateFile = getAppDataDirectory().getChildFile ("audio-device-state.xml");
    if (auto state = deviceManager.createStateXml())
        state->writeTo (stateFile);
}

juce::File AudioEngine::getAppDataDirectory() const
{
    auto directory = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                         .getChildFile ("FuseFlow");
    directory.createDirectory();
    return directory;
}

juce::AudioPluginFormat* AudioEngine::getVst3Format() const
{
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        auto* format = formatManager.getFormat (i);
        if (format != nullptr && format->getName().containsIgnoreCase ("VST3"))
            return format;
    }
    return nullptr;
}

void AudioEngine::loadPluginCache()
{
    const auto cache = getAppDataDirectory().getChildFile ("known-vst3.xml");
    if (cache.existsAsFile())
    {
        if (auto xml = juce::XmlDocument::parse (cache))
            knownPluginList.recreateFromXml (*xml);
    }
}

void AudioEngine::savePluginCache()
{
    const std::lock_guard<std::mutex> lock (pluginListMutex);
    const auto cache = getAppDataDirectory().getChildFile ("known-vst3.xml");
    if (auto xml = knownPluginList.createXml())
        xml->writeTo (cache);
}

juce::Array<juce::PluginDescription> AudioEngine::getDiscoveredPlugins() const
{
    const std::lock_guard<std::mutex> lock (pluginListMutex);
    return knownPluginList.getTypes();
}

std::vector<AudioEngine::PluginInfo> AudioEngine::getPluginList() const
{
    std::vector<PluginInfo> result;
    const std::lock_guard<std::mutex> lock (chainMutex);
    result.reserve (plugins.size());

    for (const auto& slot : plugins)
        result.push_back ({ slot->id,
                            slot->processor->getName(),
                            slot->processor->getPluginDescription().pluginFormatName,
                            slot->bypassed.load() });

    return result;
}

void AudioEngine::scanVst3DirectoryAsync (const juce::File& directory, Completion completion)
{
    scannerPool.addJob (new ScanJob (*this, directory, std::move (completion)), true);
}

juce::String AudioEngine::scanVst3Directory (const juce::File& directory)
{
    if (! directory.isDirectory())
        return "The selected folder does not exist.";

    auto* format = getVst3Format();
    if (format == nullptr)
        return "VST3 hosting is not enabled in this build.";

    auto appData = getAppDataDirectory();
    const auto deadMansPedal = appData.getChildFile ("dead-mans-pedal");
    juce::FileSearchPath searchPath;
    searchPath.add (directory);

    int countAfter = 0;
    {
        const std::lock_guard<std::mutex> lock (pluginListMutex);
        juce::PluginDirectoryScanner scanner (knownPluginList,
                                              *format,
                                              searchPath,
                                              true,
                                              deadMansPedal,
                                              true);
        juce::String fileBeingScanned;
        while (scanner.scanNextFile (true, fileBeingScanned))
        {
            // The scanner updates KnownPluginList as it discovers plug-ins.
        }

        countAfter = knownPluginList.getNumTypes();
    }

    savePluginCache();
    return "Scan complete · " + juce::String (countAfter) + " VST3 plug-in(s) indexed";
}

void AudioEngine::loadPluginAsync (const juce::PluginDescription& description, Completion completion)
{
    const auto sampleRateToUse = sampleRate.load() > 0.0 ? sampleRate.load() : 48000.0;
    const auto blockSizeToUse = bufferSize.load() > 0 ? bufferSize.load() : 128;

    formatManager.createPluginInstanceAsync (description,
                                             sampleRateToUse,
                                             blockSizeToUse,
                                             [this, completion = std::move (completion), sampleRateToUse, blockSizeToUse]
                                             (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error) mutable
    {
        if (instance == nullptr)
        {
            if (completion != nullptr)
                completion (error.isNotEmpty() ? error : "The plug-in could not be loaded.");
            return;
        }

        // The first version hosts stereo insert effects. Most VST3 vocal effects
        // expose a stereo main bus; the mono mic input is duplicated to L/R.
        instance->setPlayConfigDetails (2, 2, sampleRateToUse, blockSizeToUse);
        instance->prepareToPlay (sampleRateToUse, blockSizeToUse);

        auto slot = std::make_unique<PluginSlot>();
        slot->id = nextPluginId.fetch_add (1);
        slot->processor = std::move (instance);

        {
            const std::lock_guard<std::mutex> lock (chainMutex);
            plugins.push_back (std::move (slot));
        }

        if (completion != nullptr)
            completion ({});
    });
}

void AudioEngine::removePlugin (std::uint64_t id)
{
    const std::lock_guard<std::mutex> lock (chainMutex);
    const auto it = std::find_if (plugins.begin(), plugins.end(), [id] (const auto& item) { return item->id == id; });
    if (it != plugins.end())
    {
        (*it)->processor->releaseResources();
        plugins.erase (it);
    }
}

void AudioEngine::movePlugin (std::uint64_t id, int direction)
{
    if (direction == 0)
        return;

    const std::lock_guard<std::mutex> lock (chainMutex);
    const auto it = std::find_if (plugins.begin(), plugins.end(), [id] (const auto& item) { return item->id == id; });
    if (it == plugins.end())
        return;

    const auto index = static_cast<int> (std::distance (plugins.begin(), it));
    const auto target = index + (direction < 0 ? -1 : 1);
    if (target < 0 || target >= static_cast<int> (plugins.size()))
        return;

    std::iter_swap (plugins.begin() + index, plugins.begin() + target);
}

void AudioEngine::setPluginBypassed (std::uint64_t id, bool shouldBypass)
{
    const std::lock_guard<std::mutex> lock (chainMutex);
    for (auto& slot : plugins)
        if (slot->id == id)
            slot->bypassed.store (shouldBypass);
}

juce::AudioProcessor* AudioEngine::getPluginProcessor (std::uint64_t id)
{
    const std::lock_guard<std::mutex> lock (chainMutex);
    for (auto& slot : plugins)
        if (slot->id == id)
            return slot->processor.get();
    return nullptr;
}

bool AudioEngine::startRecording (const juce::File& file, juce::String& error)
{
    stopRecording();

    const auto currentRate = sampleRate.load();
    if (currentRate <= 0.0)
    {
        error = "Open an audio device before recording.";
        return false;
    }

    if (file.existsAsFile() && ! file.deleteFile())
    {
        error = "The target WAV file could not be replaced.";
        return false;
    }

    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
    if (stream == nullptr)
    {
        error = "Could not create the recording file.";
        return false;
    }

    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriterOptions()
                             .withSampleRate (currentRate)
                             .withNumChannels (2)
                             .withBitsPerSample (24);
    auto writer = wav.createWriterFor (stream, options);
    if (writer == nullptr)
    {
        error = "Could not initialise the WAV writer.";
        return false;
    }

    auto threaded = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), writerThread, 32768);
    {
        const std::lock_guard<std::mutex> lock (writerMutex);
        activeWriter = std::move (threaded);
        recording.store (true);
    }

    error.clear();
    return true;
}

void AudioEngine::stopRecording()
{
    recording.store (false);
    const std::lock_guard<std::mutex> lock (writerMutex);
    activeWriter.reset(); // Flushes the WAV writer's background FIFO.
}

void AudioEngine::setInputGainDb (float db) noexcept
{
    inputGainDb.store (juce::jlimit (-36.0f, 18.0f, db));
}

void AudioEngine::setOutputGainDb (float db) noexcept
{
    outputGainDb.store (juce::jlimit (-36.0f, 12.0f, db));
}

juce::String AudioEngine::getInputDeviceName() const
{
    const auto setup = deviceManager.getAudioDeviceSetup();
    return setup.inputDeviceName.isNotEmpty() ? setup.inputDeviceName : "Default input";
}

juce::String AudioEngine::getOutputDeviceName() const
{
    const auto setup = deviceManager.getAudioDeviceSetup();
    return setup.outputDeviceName.isNotEmpty() ? setup.outputDeviceName : "Default output";
}

juce::String AudioEngine::getDeviceTypeName() const
{
    const auto type = deviceManager.getCurrentAudioDeviceType();
    return type.isNotEmpty() ? type : "No audio driver";
}

juce::String AudioEngine::getDeviceError() const
{
    const std::lock_guard<std::mutex> lock (statusMutex);
    return deviceError;
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device == nullptr)
        return;

    const auto newRate = device->getCurrentSampleRate();
    const auto newBlockSize = device->getCurrentBufferSizeSamples();
    sampleRate.store (newRate);
    bufferSize.store (newBlockSize);

    const auto capacity = juce::jmax (8192, newBlockSize * 8);
    for (auto& channel : scratch)
        channel.assign (static_cast<std::size_t> (capacity), 0.0f);

    const std::lock_guard<std::mutex> lock (chainMutex);
    for (auto& slot : plugins)
    {
        slot->processor->releaseResources();
        slot->processor->setPlayConfigDetails (2, 2, newRate, newBlockSize);
        slot->processor->prepareToPlay (newRate, newBlockSize);
    }

    {
        const std::lock_guard<std::mutex> statusLock (statusMutex);
        deviceError.clear();
    }
}

void AudioEngine::audioDeviceStopped()
{
    sampleRate.store (0.0);
    bufferSize.store (0);

    const std::lock_guard<std::mutex> lock (chainMutex);
    for (auto& slot : plugins)
        slot->processor->releaseResources();
}

void AudioEngine::audioDeviceError (const juce::String& errorMessage)
{
    const std::lock_guard<std::mutex> lock (statusMutex);
    deviceError = errorMessage;
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                     int numInputChannels,
                                                     float* const* outputChannelData,
                                                     int numOutputChannels,
                                                     int numSamples,
                                                     const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused (context);
    juce::ScopedNoDenormals noDenormals;

    if (numSamples <= 0)
        return;

    if (scratch[0].size() < static_cast<std::size_t> (numSamples)
        || scratch[1].size() < static_cast<std::size_t> (numSamples))
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)
            if (outputChannelData[channel] != nullptr)
                juce::FloatVectorOperations::clear (outputChannelData[channel], numSamples);
        return;
    }

    std::array<float*, 2> channelPointers { scratch[0].data(), scratch[1].data() };
    juce::AudioBuffer<float> buffer (channelPointers.data(), 2, numSamples);
    buffer.clear();

    if (numInputChannels > 0 && inputChannelData != nullptr && inputChannelData[0] != nullptr)
    {
        const auto* left = inputChannelData[0];
        const auto* right = (numInputChannels > 1 && inputChannelData[1] != nullptr)
                               ? inputChannelData[1]
                               : inputChannelData[0];
        buffer.copyFrom (0, 0, left, numSamples);
        buffer.copyFrom (1, 0, right, numSamples);
    }

    float rawPeak = 0.0f;
    for (int channel = 0; channel < 2; ++channel)
        rawPeak = juce::jmax (rawPeak, buffer.getMagnitude (channel, 0, numSamples));
    inputPeak.store (rawPeak);

    const auto inputGain = juce::Decibels::decibelsToGain (inputGainDb.load());
    buffer.applyGain (inputGain);

    {
        std::unique_lock<std::mutex> lock (chainMutex, std::try_to_lock);
        if (lock.owns_lock())
        {
            for (auto& slot : plugins)
            {
                if (! slot->bypassed.load())
                {
                    midiBuffer.clear();
                    slot->processor->processBlock (buffer, midiBuffer);
                }
            }
        }
    }

    buffer.applyGain (juce::Decibels::decibelsToGain (outputGainDb.load()));

    float processedPeak = 0.0f;
    for (int channel = 0; channel < 2; ++channel)
        processedPeak = juce::jmax (processedPeak, buffer.getMagnitude (channel, 0, numSamples));
    outputPeak.store (processedPeak);

    // Record the post-insert, post-output-gain signal, even if software monitoring is muted.
    if (recording.load())
    {
        std::unique_lock<std::mutex> writerLock (writerMutex, std::try_to_lock);
        if (writerLock.owns_lock() && activeWriter != nullptr)
            activeWriter->write (buffer.getArrayOfReadPointers(), numSamples);
    }

    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        auto* destination = outputChannelData != nullptr ? outputChannelData[channel] : nullptr;
        if (destination == nullptr)
            continue;

        if (! monitorEnabled.load())
        {
            juce::FloatVectorOperations::clear (destination, numSamples);
            continue;
        }

        const auto sourceChannel = juce::jmin (channel, 1);
        juce::FloatVectorOperations::copy (destination, buffer.getReadPointer (sourceChannel), numSamples);
    }
}
