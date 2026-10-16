#include "MainComponent.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace
{
    const juce::Colour background      { 11, 16, 22 };
    const juce::Colour panel           { 18, 26, 35 };
    const juce::Colour panelRaised     { 24, 34, 45 };
    const juce::Colour panelHover      { 31, 43, 55 };
    const juce::Colour border          { 41, 55, 68 };
    const juce::Colour textPrimary     { 238, 243, 247 };
    const juce::Colour textSecondary   { 143, 159, 174 };
    const juce::Colour accent          { 255, 167, 84 };
    const juce::Colour accentDark      { 177, 93, 45 };
    const juce::Colour signalGreen     { 99, 220, 170 };
    const juce::Colour signalAmber     { 255, 190, 85 };
    const juce::Colour signalRed       { 255, 102, 99 };

    void setButtonColours (juce::TextButton& button,
                           juce::Colour offColour,
                           juce::Colour onColour = accent,
                           juce::Colour foreground = textPrimary)
    {
        button.setColour (juce::TextButton::buttonColourId, offColour);
        button.setColour (juce::TextButton::buttonOnColourId, onColour);
        button.setColour (juce::TextButton::textColourOffId, foreground);
        button.setColour (juce::TextButton::textColourOnId, juce::Colour (14, 20, 26));
        button.setConnectedEdges (0);
    }

    void drawCard (juce::Graphics& g, juce::Rectangle<int> bounds, juce::Colour fill = panel)
    {
        auto rect = bounds.toFloat();
        g.setColour (fill);
        g.fillRoundedRectangle (rect, 16.0f);
        g.setColour (border.withAlpha (0.72f));
        g.drawRoundedRectangle (rect.reduced (0.5f), 16.0f, 1.0f);
    }

    void drawMeter (juce::Graphics& g, juce::Rectangle<float> bounds, float peak)
    {
        g.setColour (juce::Colour (8, 13, 18));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (border.withAlpha (0.65f));
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

        const auto clamped = juce::jlimit (0.0f, 1.0f, peak);
        if (clamped <= 0.001f)
            return;

        auto fill = bounds.withWidth (juce::jmax (4.0f, bounds.getWidth() * clamped));
        const auto colour = clamped > 0.92f ? signalRed : (clamped > 0.72f ? signalAmber : signalGreen);
        g.setGradientFill (juce::ColourGradient (colour.brighter (0.12f), fill.getTopLeft(),
                                                colour.darker (0.10f), fill.getTopRight(), false));
        g.fillRoundedRectangle (fill, 5.0f);
    }

    juce::String formatPeak (float peak)
    {
        if (peak <= 0.00001f)
            return "−∞ dB";
        return juce::String (juce::Decibels::gainToDecibels (peak), 1) + " dBFS";
    }
}

class MainComponent::PluginEditorWindow final : public juce::DocumentWindow
{
public:
    PluginEditorWindow (const juce::String& title, juce::AudioProcessorEditor* editor)
        : juce::DocumentWindow (title, panelRaised, juce::DocumentWindow::closeButton)
    {
        const auto editorWidth = editor != nullptr ? editor->getWidth() : 520;
        const auto editorHeight = editor != nullptr ? editor->getHeight() : 360;

        setUsingNativeTitleBar (true);
        setResizable (true, true);
        setContentOwned (editor, true);
        centreWithSize (juce::jmax (360, editorWidth + 16), juce::jmax (240, editorHeight + 38));
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        setVisible (false);
    }
};

namespace
{
    class PluginRow final : public juce::Component
    {
    public:
        using IdAction = std::function<void (std::uint64_t)>;
        using BypassAction = std::function<void (std::uint64_t, bool)>;

        PluginRow (AudioEngine::PluginInfo infoToShow,
                   int rowNumber,
                   IdAction edit,
                   BypassAction toggleBypass,
                   IdAction remove,
                   IdAction moveUp,
                   IdAction moveDown)
            : info (std::move (infoToShow)), number (rowNumber), onEdit (std::move (edit)),
              onBypass (std::move (toggleBypass)), onRemove (std::move (remove)),
              onMoveUp (std::move (moveUp)), onMoveDown (std::move (moveDown))
        {
            addAndMakeVisible (editButton);
            addAndMakeVisible (bypassButton);
            addAndMakeVisible (upButton);
            addAndMakeVisible (downButton);
            addAndMakeVisible (removeButton);

            editButton.setButtonText ("EDIT");
            bypassButton.setButtonText ("BYP");
            upButton.setButtonText ("↑");
            downButton.setButtonText ("↓");
            removeButton.setButtonText ("×");

            setButtonColours (editButton, panelHover, accent);
            setButtonColours (bypassButton, panelHover, signalAmber);
            setButtonColours (upButton, panelHover, panelHover);
            setButtonColours (downButton, panelHover, panelHover);
            setButtonColours (removeButton, panelHover, signalRed);

            bypassButton.setClickingTogglesState (true);
            bypassButton.setToggleState (info.bypassed, juce::dontSendNotification);

            editButton.onClick = [this] { if (onEdit) onEdit (info.id); };
            bypassButton.onClick = [this]
            {
                info.bypassed = bypassButton.getToggleState();
                if (onBypass)
                    onBypass (info.id, info.bypassed);
                repaint();
            };
            removeButton.onClick = [this] { if (onRemove) onRemove (info.id); };
            upButton.onClick = [this] { if (onMoveUp) onMoveUp (info.id); };
            downButton.onClick = [this] { if (onMoveDown) onMoveDown (info.id); };
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            g.setColour (panelRaised);
            g.fillRoundedRectangle (bounds, 12.0f);
            g.setColour (border.withAlpha (0.56f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, 1.0f);

            g.setColour (accent.withAlpha (info.bypassed ? 0.25f : 0.95f));
            g.fillRoundedRectangle (juce::Rectangle<float> (9.0f, 13.0f, 3.0f, bounds.getHeight() - 26.0f), 2.0f);

            g.setColour (panelHover);
            g.fillEllipse (22.0f, (bounds.getHeight() - 30.0f) * 0.5f, 30.0f, 30.0f);
            g.setColour (textSecondary);
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText (juce::String (number).paddedLeft ('0', 2), 22, 0, 30,
                        static_cast<int> (bounds.getHeight()), juce::Justification::centred);

            const auto titleX = 64;
            const auto titleWidth = juce::jmax (120, getWidth() - 330);
            g.setColour (textPrimary.withAlpha (info.bypassed ? 0.60f : 1.0f));
            g.setFont (juce::Font (15.0f, juce::Font::bold));
            g.drawFittedText (info.name, titleX, 12, titleWidth, 24,
                              juce::Justification::centredLeft, 1);

            g.setColour (textSecondary);
            g.setFont (juce::Font (10.5f));
            const auto state = info.bypassed ? "BYPASSED" : "ACTIVE";
            g.drawText (info.format.toUpperCase() + "  ·  " + state,
                        titleX, 38, titleWidth, 17, juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (8, 9);
            auto actions = area.removeFromRight (244);
            const auto h = area.getHeight();

            removeButton.setBounds (actions.removeFromRight (32).withSizeKeepingCentre (30, h));
            actions.removeFromRight (5);
            downButton.setBounds (actions.removeFromRight (30).withSizeKeepingCentre (28, h));
            actions.removeFromRight (4);
            upButton.setBounds (actions.removeFromRight (30).withSizeKeepingCentre (28, h));
            actions.removeFromRight (8);
            bypassButton.setBounds (actions.removeFromRight (48).withSizeKeepingCentre (46, h));
            actions.removeFromRight (6);
            editButton.setBounds (actions.removeFromRight (58).withSizeKeepingCentre (56, h));
        }

    private:
        AudioEngine::PluginInfo info;
        int number = 1;
        IdAction onEdit, onRemove, onMoveUp, onMoveDown;
        BypassAction onBypass;
        juce::TextButton editButton, bypassButton, upButton, downButton, removeButton;
    };
}

class MainComponent::PluginRowsPanel final : public juce::Component
{
public:
    using EditAction = PluginRow::IdAction;
    using BypassAction = PluginRow::BypassAction;

    void setRows (const std::vector<AudioEngine::PluginInfo>& infos,
                  EditAction edit,
                  BypassAction bypass,
                  EditAction remove,
                  EditAction moveUp,
                  EditAction moveDown)
    {
        rows.clear();
        int number = 1;
        for (const auto& info : infos)
        {
            auto row = std::make_unique<PluginRow> (info, number++, edit, bypass, remove, moveUp, moveDown);
            addAndMakeVisible (row.get());
            rows.push_back (std::move (row));
        }
        resized();
        repaint();
    }

    int getDesiredHeight() const noexcept
    {
        return juce::jmax (1, static_cast<int> (rows.size()) * rowHeight + 8);
    }

    void paint (juce::Graphics& g) override
    {
        if (! rows.empty())
            return;

        auto area = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (panel.withAlpha (0.55f));
        g.fillRoundedRectangle (area, 12.0f);
        g.setColour (border.withAlpha (0.72f));
        g.drawRoundedRectangle (area, 12.0f, 1.0f);

        auto centre = area.getCentre();
        g.setColour (panelHover);
        g.fillEllipse (centre.x - 22.0f, centre.y - 44.0f, 44.0f, 44.0f);
        g.setColour (accent);
        g.setFont (juce::Font (25.0f));
        g.drawText ("+", static_cast<int> (centre.x - 20.0f), static_cast<int> (centre.y - 43.0f),
                    40, 42, juce::Justification::centred);
        g.setColour (textPrimary);
        g.setFont (juce::Font (14.0f, juce::Font::bold));
        g.drawText ("YOUR EFFECT CHAIN IS EMPTY", 20, static_cast<int> (centre.y + 10.0f),
                    getWidth() - 40, 24, juce::Justification::centred);
        g.setColour (textSecondary);
        g.setFont (juce::Font (11.5f));
        g.drawText ("Scan your VST3 folder, then add a plug-in to start processing the mic.",
                    18, static_cast<int> (centre.y + 34.0f), getWidth() - 36, 22,
                    juce::Justification::centred);
    }

    void resized() override
    {
        int y = 4;
        for (auto& row : rows)
        {
            row->setBounds (2, y, juce::jmax (1, getWidth() - 14), rowHeight - 8);
            y += rowHeight;
        }
    }

private:
    static constexpr int rowHeight = 78;
    std::vector<std::unique_ptr<PluginRow>> rows;
};

MainComponent::MainComponent()
{
    setOpaque (true);
    setSize (1220, 790);
    setLookAndFeel (&lookAndFeel);

    lookAndFeel.setColour (juce::ResizableWindow::backgroundColourId, background);
    lookAndFeel.setColour (juce::TextButton::buttonColourId, panelRaised);
    lookAndFeel.setColour (juce::TextButton::buttonOnColourId, accent);
    lookAndFeel.setColour (juce::TextButton::textColourOffId, textPrimary);
    lookAndFeel.setColour (juce::TextButton::textColourOnId, juce::Colour (14, 20, 26));

    setButtonColours (routingButton, panelRaised, panelHover);
    setButtonColours (scanButton, panelHover, panelRaised);
    setButtonColours (addPluginButton, accent, accent.brighter (0.1f), juce::Colour (25, 24, 21));
    setButtonColours (recordButton, juce::Colour (73, 31, 34), signalRed, textPrimary);
    setButtonColours (monitorButton, panelHover, signalGreen, juce::Colour (13, 23, 22));

    routingButton.onClick = [this] { showAudioSettings(); };
    scanButton.onClick = [this] { chooseVst3Folder(); };
    addPluginButton.onClick = [this] { showPluginMenu(); };
    recordButton.onClick = [this] { chooseRecordingFile(); };
    monitorButton.setClickingTogglesState (true);
    monitorButton.setToggleState (true, juce::dontSendNotification);
    monitorButton.onClick = [this]
    {
        const auto enabled = monitorButton.getToggleState();
        engine.setMonitorEnabled (enabled);
        monitorButton.setButtonText (enabled ? "FX MONITOR  ON" : "FX MONITOR  MUTED");
        setStatus (enabled ? "Software FX return is sent to the selected output." : "Software output muted; wet recording can continue.");
    };

    addAndMakeVisible (routingButton);
    addAndMakeVisible (scanButton);
    addAndMakeVisible (addPluginButton);
    addAndMakeVisible (recordButton);
    addAndMakeVisible (monitorButton);

    auto configureSlider = [this] (juce::Slider& slider, double minimum, double maximum)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange (minimum, maximum, 0.1);
        slider.setValue (0.0, juce::dontSendNotification);
        slider.setColour (juce::Slider::trackColourId, accent);
        slider.setColour (juce::Slider::thumbColourId, textPrimary);
        slider.setColour (juce::Slider::backgroundColourId, panelRaised);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, border);
        addAndMakeVisible (slider);
    };

    configureSlider (inputTrimSlider, -18.0, 12.0);
    configureSlider (outputGainSlider, -24.0, 6.0);
    inputTrimSlider.onValueChange = [this] { engine.setInputGainDb (static_cast<float> (inputTrimSlider.getValue())); };
    outputGainSlider.onValueChange = [this] { engine.setOutputGainDb (static_cast<float> (outputGainSlider.getValue())); };

    pluginRowsPanel = std::make_unique<PluginRowsPanel>();
    pluginViewport.setViewedComponent (pluginRowsPanel.get(), false);
    pluginViewport.setScrollBarsShown (true, false);
    pluginViewport.setScrollBarThickness (7);
    pluginViewport.setColour (juce::ScrollBar::thumbColourId, border.brighter (0.12f));
    pluginViewport.setColour (juce::ScrollBar::backgroundColourId, background);
    addAndMakeVisible (pluginViewport);

    rebuildPluginRows();
    startTimerHz (30);

    const auto defaultVst3Folder = juce::File ("C:/Program Files/Common Files/VST3");
    if (engine.getDiscoveredPlugins().isEmpty() && defaultVst3Folder.isDirectory())
    {
        setStatus ("Indexing VST3 plug-ins in the standard folder…");
        startVst3Scan (defaultVst3Folder);
    }
    else if (! engine.getDeviceError().isEmpty())
    {
        setStatus ("Audio device: " + engine.getDeviceError());
    }
}

MainComponent::~MainComponent()
{
    stopTimer();
    fileChooser.reset();
    editorWindows.clear();
    pluginViewport.setViewedComponent (nullptr, false);
    pluginRowsPanel.reset();
    setLookAndFeel (nullptr);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (background);

    juce::ColourGradient backgroundGradient (juce::Colour (15, 22, 30), 0.0f, 0.0f,
                                              background, 0.0f, static_cast<float> (getHeight()), false);
    g.setGradientFill (backgroundGradient);
    g.fillAll();

    // Brand mark and app title.
    g.setColour (panelRaised);
    g.fillRoundedRectangle (28.0f, 24.0f, 34.0f, 34.0f, 10.0f);
    g.setColour (accent);
    juce::Path wave;
    wave.startNewSubPath (35.0f, 41.0f);
    wave.lineTo (39.0f, 41.0f);
    wave.lineTo (42.0f, 33.0f);
    wave.lineTo (46.0f, 49.0f);
    wave.lineTo (50.0f, 37.0f);
    wave.lineTo (54.0f, 41.0f);
    wave.lineTo (57.0f, 41.0f);
    g.strokePath (wave, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (textPrimary);
    g.setFont (juce::Font (22.0f, juce::Font::bold));
    g.drawText ("FUSEFLOW", 74, 20, 190, 29, juce::Justification::centredLeft);
    g.setColour (textSecondary);
    g.setFont (juce::Font (10.5f, juce::Font::bold));
    g.drawText ("REAL-TIME  ·  VST3 VOCAL HOST", 75, 48, 230, 18, juce::Justification::centredLeft);

    const auto statusX = getWidth() - 220;
    g.setColour (panel);
    g.fillRoundedRectangle (static_cast<float> (statusX), 28.0f, 28.0f, 25.0f, 12.0f);
    g.setColour (engine.getSampleRate() > 0.0 ? signalGreen : signalAmber);
    g.fillEllipse (static_cast<float> (statusX + 9), 37.0f, 7.0f, 7.0f);
    g.setColour (textSecondary);
    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.drawText (engine.getSampleRate() > 0.0 ? "AUDIO ENGINE READY" : "AUDIO DEVICE OFFLINE",
                statusX + 30, 29, 155, 23, juce::Justification::centredLeft);

    // Input card.
    drawCard (g, inputCardBounds);
    g.setColour (accent);
    g.fillRoundedRectangle (static_cast<float> (inputCardBounds.getX() + 18),
                            static_cast<float> (inputCardBounds.getY() + 18), 3.0f, 14.0f, 1.5f);
    g.setColour (textPrimary);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText ("MIC INPUT  /  CHANNEL 1", inputCardBounds.getX() + 30, inputCardBounds.getY() + 13,
                inputCardBounds.getWidth() - 45, 20, juce::Justification::centredLeft);
    g.setColour (textSecondary);
    g.setFont (juce::Font (11.5f));
    g.drawFittedText (engine.getInputDeviceName(), inputCardBounds.getX() + 20, inputCardBounds.getY() + 37,
                      inputCardBounds.getWidth() - 40, 18, juce::Justification::centredLeft, 1);
    g.setColour (textSecondary);
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText ("INPUT LEVEL", inputCardBounds.getX() + 20, inputCardBounds.getY() + 62, 86, 15,
                juce::Justification::centredLeft);
    g.setColour (textPrimary);
    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.drawText (formatPeak (displayedInputPeak), inputCardBounds.getRight() - 105,
                inputCardBounds.getY() + 61, 86, 16, juce::Justification::centredRight);
    drawMeter (g, juce::Rectangle<float> (static_cast<float> (inputCardBounds.getX() + 20),
                                          static_cast<float> (inputCardBounds.getY() + 81),
                                          static_cast<float> (inputCardBounds.getWidth() - 40), 10.0f),
               displayedInputPeak);
    g.setColour (textSecondary);
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText ("SOFT TRIM", inputCardBounds.getX() + 20, inputCardBounds.getY() + 101, 90, 16,
                juce::Justification::centredLeft);
    g.setColour (textPrimary);
    g.setFont (juce::Font (10.0f));
    g.drawText (juce::String (engine.getInputGainDb(), 1) + " dB",
                inputCardBounds.getRight() - 64, inputCardBounds.getY() + 100, 44, 17,
                juce::Justification::centredRight);

    // Output card.
    drawCard (g, outputCardBounds);
    g.setColour (signalGreen);
    g.fillRoundedRectangle (static_cast<float> (outputCardBounds.getX() + 18),
                            static_cast<float> (outputCardBounds.getY() + 18), 3.0f, 14.0f, 1.5f);
    g.setColour (textPrimary);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText ("OUTPUT  /  WET RETURN", outputCardBounds.getX() + 30, outputCardBounds.getY() + 13,
                outputCardBounds.getWidth() - 45, 20, juce::Justification::centredLeft);
    g.setColour (textSecondary);
    g.setFont (juce::Font (11.5f));
    g.drawFittedText (engine.getOutputDeviceName(), outputCardBounds.getX() + 20, outputCardBounds.getY() + 37,
                      outputCardBounds.getWidth() - 40, 18, juce::Justification::centredLeft, 1);
    g.setColour (textSecondary);
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText ("PROCESSED OUTPUT", outputCardBounds.getX() + 20, outputCardBounds.getY() + 62, 125, 15,
                juce::Justification::centredLeft);
    g.setColour (textPrimary);
    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.drawText (engine.getDeviceTypeName() + "  ·  " + juce::String (engine.getSampleRate() / 1000.0, 1) + " kHz",
                outputCardBounds.getRight() - 230, outputCardBounds.getY() + 61, 210, 16,
                juce::Justification::centredRight);
    drawMeter (g, juce::Rectangle<float> (static_cast<float> (outputCardBounds.getX() + 20),
                                          static_cast<float> (outputCardBounds.getY() + 81),
                                          static_cast<float> (outputCardBounds.getWidth() - 40), 10.0f),
               displayedOutputPeak);
    g.setColour (textSecondary);
    g.setFont (juce::Font (9.5f));
    g.drawText ("Choose a physical MiniFuse output or a virtual playback endpoint in Audio Routing.",
                outputCardBounds.getX() + 20, outputCardBounds.getY() + 100,
                outputCardBounds.getWidth() - 40, 17, juce::Justification::centredLeft);

    // Effect chain card.
    drawCard (g, chainCardBounds);
    g.setColour (textPrimary);
    g.setFont (juce::Font (13.0f, juce::Font::bold));
    g.drawText ("EFFECT CHAIN", chainCardBounds.getX() + 20, chainCardBounds.getY() + 16, 180, 20,
                juce::Justification::centredLeft);
    g.setColour (textSecondary);
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText ("SERIAL VST3 INSERTS  ·  MIC → FX → OUTPUT", chainCardBounds.getX() + 20,
                chainCardBounds.getY() + 38, 270, 16, juce::Justification::centredLeft);
    const auto pluginCount = static_cast<int> (engine.getPluginList().size());
    g.setColour (panelHover);
    g.fillRoundedRectangle (static_cast<float> (chainCardBounds.getX() + 296),
                            static_cast<float> (chainCardBounds.getY() + 18), 78.0f, 22.0f, 10.0f);
    g.setColour (accent);
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText (juce::String (pluginCount) + " LOADED", chainCardBounds.getX() + 296,
                chainCardBounds.getY() + 18, 78, 22, juce::Justification::centred);

    // Master / recorder card.
    drawCard (g, masterCardBounds);
    g.setColour (textPrimary);
    g.setFont (juce::Font (13.0f, juce::Font::bold));
    g.drawText ("OUTPUT + RECORD", masterCardBounds.getX() + 18, masterCardBounds.getY() + 17,
                masterCardBounds.getWidth() - 36, 20, juce::Justification::centredLeft);
    g.setColour (textSecondary);
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText ("ROUTE TARGET", masterCardBounds.getX() + 18, masterCardBounds.getY() + 48,
                masterCardBounds.getWidth() - 36, 15, juce::Justification::centredLeft);
    g.setColour (textPrimary);
    g.setFont (juce::Font (11.0f));
    g.drawFittedText (engine.getOutputDeviceName(), masterCardBounds.getX() + 18, masterCardBounds.getY() + 64,
                      masterCardBounds.getWidth() - 36, 20, juce::Justification::centredLeft, 1);

    g.setColour (border);
    g.drawLine (static_cast<float> (masterCardBounds.getX() + 18), static_cast<float> (masterCardBounds.getY() + 98),
                static_cast<float> (masterCardBounds.getRight() - 18), static_cast<float> (masterCardBounds.getY() + 98), 1.0f);
    g.setColour (textSecondary);
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText ("SOFTWARE FX MONITOR", masterCardBounds.getX() + 18, masterCardBounds.getY() + 109,
                masterCardBounds.getWidth() - 36, 16, juce::Justification::centredLeft);
    g.setColour (textSecondary);
    g.setFont (juce::Font (9.5f));
    g.drawFittedText ("For wet-only monitoring, turn off the MiniFuse 1 Direct button.",
                      masterCardBounds.getX() + 18, masterCardBounds.getY() + 153,
                      masterCardBounds.getWidth() - 36, 34, juce::Justification::topLeft, 2);

    g.setColour (border);
    g.drawLine (static_cast<float> (masterCardBounds.getX() + 18), static_cast<float> (masterCardBounds.getY() + 202),
                static_cast<float> (masterCardBounds.getRight() - 18), static_cast<float> (masterCardBounds.getY() + 202), 1.0f);
    g.setColour (textSecondary);
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText ("MASTER OUTPUT", masterCardBounds.getX() + 18, masterCardBounds.getY() + 213,
                masterCardBounds.getWidth() - 82, 16, juce::Justification::centredLeft);
    g.setColour (textPrimary);
    g.setFont (juce::Font (10.0f));
    g.drawText (juce::String (engine.getOutputGainDb(), 1) + " dB",
                masterCardBounds.getRight() - 66, masterCardBounds.getY() + 212, 48, 17,
                juce::Justification::centredRight);

    g.setColour (textSecondary);
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText ("WET RECORDING  ·  WAV 24-BIT", masterCardBounds.getX() + 18,
                masterCardBounds.getY() + 262, masterCardBounds.getWidth() - 36, 17,
                juce::Justification::centredLeft);
    g.setColour (textSecondary);
    g.setFont (juce::Font (9.5f));
    g.drawFittedText (statusMessage, masterCardBounds.getX() + 18, masterCardBounds.getY() + 333,
                      masterCardBounds.getWidth() - 36, 48, juce::Justification::topLeft, 3);

    // Footer hint.
    const auto footerY = getHeight() - 22;
    g.setColour (border.withAlpha (0.65f));
    g.drawLine (28.0f, static_cast<float> (footerY - 8), static_cast<float> (getWidth() - 28),
                static_cast<float> (footerY - 8), 1.0f);
    g.setColour (textSecondary);
    g.setFont (juce::Font (9.5f));
    const auto info = engine.getDeviceError().isNotEmpty()
                        ? "AUDIO: " + engine.getDeviceError()
                        : "LOW-LATENCY TIP  ·  Select Arturia MiniFuse ASIO and start with a 128-sample buffer; lower it if stable.";
    g.drawFittedText (info, 30, footerY - 2, getWidth() - 60, 18, juce::Justification::centredLeft, 1);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (28);
    area.removeFromTop (70);
    routingButton.setBounds (getWidth() - 28 - 166, 25, 166, 36);

    area.removeFromTop (12);
    auto topCards = area.removeFromTop (132);
    const auto cardGap = 14;
    const auto cardWidth = (topCards.getWidth() - cardGap) / 2;
    inputCardBounds = topCards.removeFromLeft (cardWidth);
    topCards.removeFromLeft (cardGap);
    outputCardBounds = topCards;

    area.removeFromTop (18);
    area.removeFromBottom (30);
    const auto masterWidth = 280;
    masterCardBounds = area.removeFromRight (masterWidth);
    area.removeFromRight (18);
    chainCardBounds = area;

    scanButton.setBounds (chainCardBounds.getRight() - 20 - 112 - 9 - 135,
                          chainCardBounds.getY() + 15, 112, 34);
    addPluginButton.setBounds (chainCardBounds.getRight() - 20 - 135,
                               chainCardBounds.getY() + 15, 135, 34);

    const auto viewportTop = chainCardBounds.getY() + 67;
    pluginViewport.setBounds (chainCardBounds.getX() + 14, viewportTop,
                              chainCardBounds.getWidth() - 28,
                              juce::jmax (60, chainCardBounds.getBottom() - viewportTop - 15));
    if (pluginRowsPanel != nullptr)
        pluginRowsPanel->setSize (juce::jmax (1, pluginViewport.getWidth() - 8),
                                  juce::jmax (pluginViewport.getHeight(), pluginRowsPanel->getDesiredHeight()));

    inputTrimSlider.setBounds (inputCardBounds.getX() + 18, inputCardBounds.getBottom() - 23,
                               inputCardBounds.getWidth() - 36, 14);
    monitorButton.setBounds (masterCardBounds.getX() + 18, masterCardBounds.getY() + 128,
                             masterCardBounds.getWidth() - 36, 34);
    outputGainSlider.setBounds (masterCardBounds.getX() + 18, masterCardBounds.getY() + 236,
                                masterCardBounds.getWidth() - 36, 16);
    recordButton.setBounds (masterCardBounds.getX() + 18, masterCardBounds.getY() + 286,
                            masterCardBounds.getWidth() - 36, 42);
}

void MainComponent::timerCallback()
{
    displayedInputPeak = juce::jmax (engine.getInputPeak(), displayedInputPeak * 0.78f);
    displayedOutputPeak = juce::jmax (engine.getOutputPeak(), displayedOutputPeak * 0.78f);

    if (engine.isRecording())
    {
        recordButton.setButtonText ("■  STOP RECORDING");
        recordButton.setColour (juce::TextButton::buttonColourId, signalRed.darker (0.48f));
    }
    else
    {
        recordButton.setButtonText ("●  RECORD WET");
        recordButton.setColour (juce::TextButton::buttonColourId, juce::Colour (73, 31, 34));
    }

    repaint();
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (engine.getDeviceManager(),
                                                              1, 16,
                                                              2, 16,
                                                              false,
                                                              false,
                                                              false,
                                                              false);
    selector->setSize (560, 430);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (selector);
    options.dialogTitle = "Audio routing";
    options.dialogBackgroundColour = panel;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.componentToCentreAround = this;
    options.launchAsync();
}

void MainComponent::chooseVst3Folder()
{
    const auto defaultFolder = juce::File ("C:/Program Files/Common Files/VST3");
    fileChooser = std::make_unique<juce::FileChooser> ("Select a VST3 plug-in folder", defaultFolder, "*");
    auto safeThis = juce::Component::SafePointer<MainComponent> (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safeThis] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;
        const auto result = chooser.getResult();
        if (result.isDirectory())
            safeThis->startVst3Scan (result);
    });
}

void MainComponent::startVst3Scan (const juce::File& folder)
{
    setStatus ("Scanning " + folder.getFileName() + " for VST3 plug-ins…");
    scanButton.setEnabled (false);
    auto safeThis = juce::Component::SafePointer<MainComponent> (this);
    engine.scanVst3DirectoryAsync (folder, [safeThis] (juce::String result)
    {
        if (safeThis == nullptr)
            return;
        safeThis->scanButton.setEnabled (true);
        safeThis->addPluginButton.setEnabled (! safeThis->engine.getDiscoveredPlugins().isEmpty());
        safeThis->setStatus (result);
    });
}

void MainComponent::showPluginMenu()
{
    const auto allPlugins = engine.getDiscoveredPlugins();
    juce::Array<juce::PluginDescription> vst3Plugins;
    for (const auto& description : allPlugins)
        if (description.pluginFormatName.containsIgnoreCase ("VST3"))
            vst3Plugins.add (description);

    if (vst3Plugins.isEmpty())
    {
        setStatus ("No VST3 plug-ins indexed yet. Click SCAN VST3 and select your plug-in folder.");
        return;
    }

    juce::PopupMenu menu;
    menu.addSectionHeader ("AVAILABLE VST3 EFFECTS");
    for (int i = 0; i < vst3Plugins.size(); ++i)
    {
        const auto& description = vst3Plugins.getReference (i);
        auto itemText = description.name;
        if (description.manufacturerName.isNotEmpty())
            itemText += "  ·  " + description.manufacturerName;
        menu.addItem (i + 1, itemText);
    }

    auto safeThis = juce::Component::SafePointer<MainComponent> (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addPluginButton),
                        [safeThis, vst3Plugins] (int result)
    {
        if (safeThis == nullptr || result <= 0 || result > vst3Plugins.size())
            return;

        safeThis->setStatus ("Loading VST3 effect…");
        const auto description = vst3Plugins.getReference (result - 1);
        safeThis->engine.loadPluginAsync (description, [safeThis] (juce::String error)
        {
            if (safeThis == nullptr)
                return;
            if (error.isEmpty())
            {
                safeThis->rebuildPluginRows();
                safeThis->setStatus ("VST3 effect loaded into the live mic chain.");
            }
            else
            {
                safeThis->setStatus ("Plug-in load failed: " + error);
            }
        });
    });
}

void MainComponent::chooseRecordingFile()
{
    if (engine.isRecording())
    {
        engine.stopRecording();
        setStatus ("Wet recording saved as a 24-bit WAV file.");
        return;
    }

    auto startFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                           .getChildFile ("FuseFlow Recordings");
    startFolder.createDirectory();
    fileChooser = std::make_unique<juce::FileChooser> ("Save wet recording", startFolder.getChildFile ("Vocal take.wav"), "*.wav");
    auto safeThis = juce::Component::SafePointer<MainComponent> (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        auto file = chooser.getResult();
        if (file == juce::File())
            return;
        if (file.getFileExtension().isEmpty())
            file = file.withFileExtension (".wav");
        else if (! file.hasFileExtension (".wav"))
            file = file.withFileExtension (".wav");

        juce::String error;
        if (safeThis->engine.startRecording (file, error))
            safeThis->setStatus ("Recording wet output to " + file.getFileName());
        else
            safeThis->setStatus ("Record error: " + error);
    });
}

void MainComponent::rebuildPluginRows()
{
    auto safeThis = juce::Component::SafePointer<MainComponent> (this);
    pluginRowsPanel->setRows (engine.getPluginList(),
        [safeThis] (std::uint64_t id) { if (safeThis != nullptr) safeThis->openPluginEditor (id); },
        [safeThis] (std::uint64_t id, bool bypassed) { if (safeThis != nullptr) safeThis->engine.setPluginBypassed (id, bypassed); },
        [safeThis] (std::uint64_t id) { if (safeThis != nullptr) safeThis->removePlugin (id); },
        [safeThis] (std::uint64_t id) { if (safeThis != nullptr) { safeThis->engine.movePlugin (id, -1); safeThis->rebuildPluginRows(); } },
        [safeThis] (std::uint64_t id) { if (safeThis != nullptr) { safeThis->engine.movePlugin (id, 1); safeThis->rebuildPluginRows(); } });

    pluginRowsPanel->setSize (juce::jmax (1, pluginViewport.getWidth() - 8),
                              juce::jmax (pluginViewport.getHeight(), pluginRowsPanel->getDesiredHeight()));
}

void MainComponent::openPluginEditor (std::uint64_t id)
{
    if (auto it = editorWindows.find (id); it != editorWindows.end())
    {
        it->second->setVisible (true);
        it->second->toFront (true);
        return;
    }

    if (auto* processor = engine.getPluginProcessor (id))
    {
        if (auto* editor = processor->createEditorIfNeeded())
        {
            auto window = std::make_unique<PluginEditorWindow> (processor->getName(), editor);
            window->toFront (true);
            editorWindows.emplace (id, std::move (window));
            return;
        }
    }

    setStatus ("This plug-in does not expose a custom editor.");
}

void MainComponent::removePlugin (std::uint64_t id)
{
    // Destroy the plug-in editor while its processor is still alive.
    editorWindows.erase (id);
    engine.removePlugin (id);
    rebuildPluginRows();
    setStatus ("Effect removed from the chain.");
}

void MainComponent::setStatus (const juce::String& message)
{
    statusMessage = message;
    repaint();
}
