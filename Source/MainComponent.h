#pragma once

#include "AudioEngine.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <unordered_map>

class MainComponent final : public juce::Component, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class PluginRowsPanel;
    class PluginEditorWindow;

    void timerCallback() override;
    void showAudioSettings();
    void chooseVst3Folder();
    void startVst3Scan (const juce::File& folder);
    void showPluginMenu();
    void chooseRecordingFile();
    void rebuildPluginRows();
    void openPluginEditor (std::uint64_t id);
    void removePlugin (std::uint64_t id);
    void setStatus (const juce::String& message);

    AudioEngine engine;
    juce::LookAndFeel_V4 lookAndFeel;

    juce::TextButton routingButton { "AUDIO ROUTING" };
    juce::TextButton scanButton { "SCAN VST3" };
    juce::TextButton addPluginButton { "+  ADD EFFECT" };
    juce::TextButton recordButton { "●  RECORD WET" };
    juce::TextButton monitorButton { "FX MONITOR  ON" };
    juce::Slider inputTrimSlider;
    juce::Slider outputGainSlider;
    juce::Viewport pluginViewport;
    std::unique_ptr<PluginRowsPanel> pluginRowsPanel;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unordered_map<std::uint64_t, std::unique_ptr<PluginEditorWindow>> editorWindows;

    juce::Rectangle<int> inputCardBounds;
    juce::Rectangle<int> outputCardBounds;
    juce::Rectangle<int> chainCardBounds;
    juce::Rectangle<int> masterCardBounds;
    juce::String statusMessage { "Choose MiniFuse ASIO for the lowest software-monitoring latency." };
    float displayedInputPeak = 0.0f;
    float displayedOutputPeak = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
