#include <juce_gui_extra/juce_gui_extra.h>
#include "MainComponent.h"

class ETBApp : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String&) override { window = std::make_unique<Window>(getApplicationName()); }
    void shutdown() override { window = nullptr; }
    void systemRequestedQuit() override { quit(); }
    struct Window : juce::DocumentWindow {
        explicit Window(const juce::String& n) : juce::DocumentWindow(n, juce::Colour(0xff0b0b0e), juce::DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true); setContentOwned(new MainComponent(), true); setResizable(true, true);
            setResizeLimits(900, 520, 3000, 2000); centreWithSize(getWidth(), getHeight()); setVisible(true); }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(ETBApp)
