// Link from the studio app to the station server, so the phone remote can drive the decks.
// Plain HTTPS long-poll (POST /api/studio/link, /api/studio/ack) — works through any proxy, no WebSocket/TLS code of our own.
#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>

class StudioLink : private juce::Thread {
public:
    struct Result { bool ok; juce::String detail; };
    std::function<juce::var()> getStatus;                                   // message thread
    std::function<Result(const juce::String& cmd, const juce::String& deck)> onCommand;   // message thread
    std::function<void(bool connected, const juce::String& why)> onLinkState;              // message thread

    StudioLink() : juce::Thread("ETB studio link") {}
    ~StudioLink() override { stop(); }
    void start(const juce::String& serverBase, const juce::String& apiToken);
    void stop() { signalThreadShouldExit(); stopThread(25000); }
    // one-shot authenticated call (KILL, RESTORE …) on a background thread; reply on the message thread
    void post(const juce::String& path, const juce::var& body, std::function<void(int status, juce::var reply)> done);

private:
    void run() override;
    int request(const juce::String& path, const juce::String& json, juce::String& out, int timeoutMs);
    juce::var statusOnMessageThread();
    juce::String base, token;
    juce::CriticalSection lock;
};
