#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "DeckView.h"
#include "StudioLink.h"

class MainComponent : public juce::AudioAppComponent, private juce::Timer {
public:
    MainComponent();
    ~MainComponent() override;
    void prepareToPlay(int samplesPerBlock, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;
    void releaseResources() override {}
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void loadInto(DeckChannel&, const juce::File&);
    void chooseFile(DeckChannel&);
    void openSettings();
    void connect();
    void kill(const juce::String& mode);
    juce::var status();
    StudioLink::Result command(const juce::String& cmd, const juce::String& deck);

    DeckChannel a, b; DeckView va { a, "A", juce::Colour(0xff2f8bff) }, vb { b, "B", juce::Colour(0xffff4fa0) };
    juce::Slider xfader; std::atomic<float> xf { 0.5f };
    etb::Crossfader curve; etb::GainSmoother smooth;
    juce::TextButton killBtn { "KILL" }, restoreBtn { "RESTORE" }, settingsBtn { "SERVER" };
    juce::Label linkLbl, airLbl;
    juce::AudioFormatManager formats; std::unique_ptr<juce::FileChooser> chooser;
    std::vector<float> tmpL[2], tmpR[2];
    double sr = 44100.0; bool killHeld = false; juce::uint32 killDownAt = 0;
    StudioLink link; juce::Image logo; juce::String airMode { "?" };
    struct KillMouse : juce::MouseListener { MainComponent& m; explicit KillMouse(MainComponent& o) : m(o) {}
        void mouseDown(const juce::MouseEvent&) override { m.killDownAt = juce::Time::getMillisecondCounter(); m.killHeld = true; }
        void mouseUp(const juce::MouseEvent&) override; } killMouse { *this };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
