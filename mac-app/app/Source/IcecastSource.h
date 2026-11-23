// GO LIVE: sends the studio mix to the station's Icecast DJ input (default stream.lowrideliferadio.com:8000/live).
// Plain Icecast source protocol (HTTP PUT + Basic auth, the same as BUTT / ffmpeg), so it also works with any
// Icecast 2.4 server. Encoder: Ogg Vorbis ~160 kbps (built into JUCE, no LAME / no third-party licences).
// The station server decodes it and re-encodes the public stream (MP3 128k + AAC 96k), so listeners are unaffected.
//
// Threading: pushAudio() is called on the AUDIO thread and never blocks or allocates (lock-free FIFO, drops samples
// when the network can't keep up). Connecting, encoding and socket writes all happen on this object's own thread.
// It reconnects automatically (every 3 s) while GO LIVE is on, e.g. after a Wi-Fi drop.
#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <functional>
#include <memory>

class IcecastSource : private juce::Thread {
public:
    struct Settings {
        juce::String host { "stream.lowrideliferadio.com" };
        int port = 8000;
        juce::String mount { "/live" };
        juce::String user { "source" };
        juce::String password;
        juce::String stationName { "Lowride Life Radio - LIVE" };
        int qualityIndex = 5;   // JUCE Ogg Vorbis option 5 = ~160 kbps
    };
    enum class State { idle, connecting, live, error };

    IcecastSource();
    ~IcecastSource() override;

    void start(const Settings& s, double sampleRate);   // message thread
    void stop();                                        // message thread
    bool isOn() const noexcept { return wanted.load(); }
    State state() const noexcept { return st.load(); }
    juce::String lastMessage() const;                   // human text for the UI
    double secondsOnAir() const;
    juce::int64 bytesSent() const noexcept { return sent.load(); }

    // audio thread: non-blocking, no allocation
    void pushAudio(const float* L, const float* R, int numSamples) noexcept;

    // one-shot connection test (blocking, call from a background thread or accept a short UI freeze)
    static juce::String probe(const Settings& s);

    // The UI polls state()/lastMessage() from its 30 Hz timer (no cross-thread callbacks -> safe at app exit).

private:
    void run() override;
    bool session();                                       // one connection; returns when it drops / stops
    static std::unique_ptr<juce::StreamingSocket> openAndHandshake(const Settings&, juce::String& why, const juce::String& contentType);
    void setState(State s, const juce::String& msg);

    Settings cfg; double rate = 44100.0;
    std::atomic<bool> wanted { false };
    std::atomic<State> st { State::idle };
    std::atomic<juce::int64> sent { 0 };
    std::atomic<juce::uint32> liveSince { 0 };
    mutable juce::CriticalSection msgLock; juce::String msg { "OFF AIR" };
    juce::CriticalSection sockLock; juce::StreamingSocket* activeSocket = nullptr;

    static constexpr int fifoFrames = 44100 * 8;       // ~8 s of slack for network hiccups
    juce::AbstractFifo fifo { fifoFrames };
    std::vector<float> ringL, ringR;
};
