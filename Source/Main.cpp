#include <juce_gui_extra/juce_gui_extra.h>
#include "MainComponent.h"

class FuseFlowApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "FuseFlow"; }
    const juce::String getApplicationVersion() override    { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override             { return false; }

    void initialise (const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override {}

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& title)
            : juce::DocumentWindow (title,
                                    juce::Colour (11, 16, 22),
                                    juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setResizable (true, true);
            setResizeLimits (1000, 680, 1900, 1300);
            setContentOwned (new MainComponent(), true);
            centreWithSize (1220, 790);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (FuseFlowApplication)
