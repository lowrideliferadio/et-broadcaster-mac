// One deck: a perfectly round platter (drawn in a square, never stretched) you can grab and scratch.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "etb/Deck.h"
#include <atomic>

struct DeckChannel {
    etb::Deck deck;
    std::vector<float> interleaved;
    juce::SpinLock swapLock;
    std::atomic<bool> motor { false }, touch { false }, killed { false }, loaded { false };
    std::atomic<double> pendingRevs { 0.0 };
    std::atomic<double> angleDeg { 0.0 }, position { 0.0 }, lengthFrames { 0.0 }, trackRate { 44100.0 };
    std::atomic<float> pitch { 1.0f };
    juce::String title { "EMPTY" };
    void addRevs(double r) { double cur = pendingRevs.load(); while (! pendingRevs.compare_exchange_weak(cur, cur + r)) {} }
};

class DeckView : public juce::Component, private juce::Timer {
public:
    DeckView(DeckChannel& c, juce::String name, juce::Colour accent);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    std::function<void()> onLoad;
    juce::TextButton load { "LOAD" }, play { "PLAY / STOP" }, kill { "KILL DECK" };
    juce::Slider pitch;
private:
    void timerCallback() override { repaint(platter.toNearestInt()); }
    juce::Rectangle<float> platter; juce::Point<float> centre; double lastAngle = 0; bool grabbing = false;
    DeckChannel& ch; juce::String name; juce::Colour accent;
};
