#include "IcecastSource.h"
#include <juce_audio_formats/juce_audio_formats.h>
#ifndef JUCE_APPLICATION_VERSION_STRING
 #define JUCE_APPLICATION_VERSION_STRING "dev"
#endif

namespace {
// OutputStream over the socket; any write failure marks the stream dead so the session ends and reconnects.
struct SocketOut : juce::OutputStream {
    juce::StreamingSocket& s; std::atomic<juce::int64>& counter; bool dead = false; juce::int64 pos = 0;
    SocketOut(juce::StreamingSocket& so, std::atomic<juce::int64>& c) : s(so), counter(c) {}
    void flush() override {}
    bool setPosition(juce::int64) override { return false; }
    juce::int64 getPosition() override { return pos; }
    bool write(const void* data, size_t n) override {
        if (dead) return false;
        auto* p = static_cast<const char*>(data); size_t done = 0;
        while (done < n) {
            const int r = s.waitUntilReady(false, 10000);
            if (r <= 0) { dead = true; return false; }       // 10 s without the network accepting data
            const int w = s.write(p + done, (int) juce::jmin<size_t>(n - done, 65536));
            if (w <= 0) { dead = true; return false; }
            done += (size_t) w;
        }
        pos += (juce::int64) n; counter += (juce::int64) n; return true;
    }
};

juce::String readHttpReply(juce::StreamingSocket& s, int timeoutMs) {
    juce::String head; char c = 0; const auto until = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    while (! head.endsWith("\r\n\r\n") && head.length() < 4096) {
        const int left = (int) (until - juce::Time::getMillisecondCounter());
        if (left <= 0 || s.waitUntilReady(true, left) <= 0) break;
        if (s.read(&c, 1, false) != 1) break;
        head += juce::String::charToString((juce::juce_wchar) (unsigned char) c);
    }
    return head;
}

juce::String explain(int code, const juce::String& firstLine) {
    switch (code) {
        case 401: return "Wrong username or password";
        case 403: return "Server refused (mount busy - someone else is live?)";
        case 404: return "Mount not found on the server";
        case 0:   return "No reply from server";
        default:  return "Server said: " + firstLine.trim();
    }
}
}

IcecastSource::IcecastSource() : juce::Thread("ETB go-live") {
    ringL.assign((size_t) fifoFrames, 0.f); ringR.assign((size_t) fifoFrames, 0.f);
}
IcecastSource::~IcecastSource() { stop(); }

void IcecastSource::start(const Settings& s, double sampleRate) {
    stop();
    cfg = s; rate = sampleRate > 0 ? sampleRate : 44100.0;
    if (! cfg.mount.startsWith("/")) cfg.mount = "/" + cfg.mount;
    fifo.reset(); sent = 0; wanted = true;
    setState(State::connecting, "CONNECTING...");
    startThread();
}

void IcecastSource::stop() {
    wanted = false;
    { const juce::ScopedLock sl(sockLock); if (activeSocket) activeSocket->close(); }   // unblocks a stuck write
    signalThreadShouldExit(); notify(); stopThread(8000);
    if (st.load() != State::idle) setState(State::idle, "OFF AIR");
}

juce::String IcecastSource::lastMessage() const { const juce::ScopedLock sl(msgLock); return msg; }
double IcecastSource::secondsOnAir() const {
    return st.load() == State::live ? (juce::Time::getMillisecondCounter() - liveSince.load()) / 1000.0 : 0.0;
}

void IcecastSource::setState(State s, const juce::String& m) {
    st = s; { const juce::ScopedLock sl(msgLock); msg = m; }
    if (s == State::live) liveSince = juce::Time::getMillisecondCounter();
}

void IcecastSource::pushAudio(const float* L, const float* R, int n) noexcept {
    if (! wanted.load() || st.load() != State::live) return;
    int s1, n1, s2, n2; fifo.prepareToWrite(n, s1, n1, s2, n2);   // full fifo -> excess is dropped, never blocks
    if (n1 > 0) { std::copy(L, L + n1, ringL.data() + s1); std::copy(R, R + n1, ringR.data() + s1); }
    if (n2 > 0) { std::copy(L + n1, L + n1 + n2, ringL.data() + s2); std::copy(R + n1, R + n1 + n2, ringR.data() + s2); }
    fifo.finishedWrite(n1 + n2);
}

std::unique_ptr<juce::StreamingSocket> IcecastSource::openAndHandshake(const Settings& c, juce::String& why, const juce::String& contentType) {
    auto sock = std::make_unique<juce::StreamingSocket>();
    if (! sock->connect(c.host, c.port, 8000)) { why = "Can't reach " + c.host + ":" + juce::String(c.port) + " (check internet / host / port)"; return nullptr; }
    const auto auth = juce::Base64::toBase64(c.user + ":" + c.password);
    juce::String req;
    req << "PUT " << c.mount << " HTTP/1.1\r\n"
        << "Host: " << c.host << ":" << c.port << "\r\n"
        << "Authorization: Basic " << auth << "\r\n"
        << "User-Agent: ET Broadcaster/" << JUCE_APPLICATION_VERSION_STRING << "\r\n"
        << "Content-Type: " << contentType << "\r\n"
        << "Ice-Public: 0\r\n"
        << "Ice-Name: " << c.stationName << "\r\n"
        << "Expect: 100-continue\r\n\r\n";
    const auto utf8 = req.toStdString();
    if (sock->write(utf8.data(), (int) utf8.size()) != (int) utf8.size()) { why = "Connection dropped while logging in"; return nullptr; }
    const auto reply = readHttpReply(*sock, 8000);
    const auto first = reply.upToFirstOccurrenceOf("\r\n", false, false);
    const int code = first.fromFirstOccurrenceOf(" ", false, false).getIntValue();
    if (code != 100 && code != 200) { why = explain(code, first); return nullptr; }
    return sock;
}

juce::String IcecastSource::probe(const Settings& s) {
    // Logs in, sends nothing, disconnects. Icecast drops the empty mount at once, so this never touches the air.
    juce::String why; auto sock = openAndHandshake(s, why, "application/ogg");
    if (! sock) return why;
    sock->close(); return {};
}

bool IcecastSource::session() {
    juce::String why;
    auto sock = openAndHandshake(cfg, why, "application/ogg");
    if (! sock) { setState(State::error, why.toUpperCase() + " - RETRYING"); return false; }
    { const juce::ScopedLock sl(sockLock); activeSocket = sock.get(); }

    juce::OggVorbisAudioFormat ogg;
    auto out = std::unique_ptr<juce::OutputStream>(new SocketOut(*sock, sent));
    auto* raw = static_cast<SocketOut*>(out.get());
    auto opts = juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(2).withBitsPerSample(32)
                    .withQualityOptionIndex(cfg.qualityIndex)
                    .withMetadata(juce::OggVorbisAudioFormat::encoderName, "ET Broadcaster")
                    .withMetadata(juce::OggVorbisAudioFormat::id3title, cfg.stationName);
    auto writer = ogg.createWriterFor(out, opts);
    if (! writer) { setState(State::error, "ENCODER FAILED (sample rate " + juce::String(rate) + ")"); const juce::ScopedLock sl(sockLock); activeSocket = nullptr; return false; }

    fifo.reset();
    setState(State::live, "ON AIR");
    juce::AudioBuffer<float> buf(2, 4096);
    while (! threadShouldExit() && wanted.load() && ! raw->dead) {
        int s1, n1, s2, n2; fifo.prepareToRead(4096, s1, n1, s2, n2);
        const int n = n1 + n2;
        if (n == 0) { wait(10); continue; }
        if (n1 > 0) { buf.copyFrom(0, 0, ringL.data() + s1, n1); buf.copyFrom(1, 0, ringR.data() + s1, n1); }
        if (n2 > 0) { buf.copyFrom(0, n1, ringL.data() + s2, n2); buf.copyFrom(1, n1, ringR.data() + s2, n2); }
        fifo.finishedRead(n);
        writer->writeFromAudioSampleBuffer(buf, 0, n);
    }
    const bool dropped = raw->dead;
    { const juce::ScopedLock sl(sockLock); activeSocket = nullptr; }
    writer.reset();            // writes the Ogg end-of-stream page (ignored if the socket is gone)
    sock->close();
    if (dropped && wanted.load()) setState(State::error, "CONNECTION LOST - RECONNECTING");
    return true;
}

void IcecastSource::run() {
    while (! threadShouldExit() && wanted.load()) {
        session();
        if (threadShouldExit() || ! wanted.load()) break;
        wait(3000);                                   // reconnect backoff
        if (wanted.load() && ! threadShouldExit()) setState(State::connecting, "RECONNECTING...");
    }
}
