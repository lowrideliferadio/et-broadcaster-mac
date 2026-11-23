#include "StudioLink.h"

void StudioLink::start(const juce::String& serverBase, const juce::String& apiToken) {
    stop();
    { const juce::ScopedLock sl(lock); base = serverBase.trimCharactersAtEnd("/"); token = apiToken; }
    if (base.isNotEmpty() && token.isNotEmpty()) startThread();
}

int StudioLink::request(const juce::String& path, const juce::String& json, juce::String& out, int timeoutMs) {
    juce::String b, t; { const juce::ScopedLock sl(lock); b = base; t = token; }
    int status = 0;
    auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders("Authorization: Bearer " + t + "\r\nContent-Type: application/json\r\nX-ETB: 1")
                    .withConnectionTimeoutMs(timeoutMs).withStatusCode(&status);
    if (auto in = juce::URL(b + path).withPOSTData(json).createInputStream(opts)) { out = in->readEntireStreamAsString(); return status; }
    return 0;
}

juce::var StudioLink::statusOnMessageThread() {
    juce::var v; juce::WaitableEvent done;
    juce::MessageManager::callAsync([&] { if (getStatus) v = getStatus(); done.signal(); });
    done.wait(2000); return v;
}

void StudioLink::run() {
    int backoff = 1000; bool connected = false;
    auto setState = [this, &connected](bool c, juce::String why) {
        if (c == connected)
            return;
        connected = c;
        juce::MessageManager::callAsync([this, c, why] { if (onLinkState) onLinkState(c, why); });
    };
    while (! threadShouldExit()) {
        auto st = new juce::DynamicObject(); st->setProperty("status", statusOnMessageThread());
        juce::String reply; int code = request("/api/studio/link", juce::JSON::toString(juce::var(st), true), reply, 30000);
        if (threadShouldExit()) break;
        if (code != 200) {
            setState(false, code == 401 ? "server rejected the API token" : code == 0 ? "server unreachable" : "HTTP " + juce::String(code));
            wait(backoff); backoff = juce::jmin(15000, backoff * 2); continue;
        }
        backoff = 1000; setState(true, {});
        auto cmds = juce::JSON::parse(reply)["cmds"];
        if (auto* arr = cmds.getArray()) for (auto& c : *arr) {
            const auto id = c["id"].toString(), cmd = c["cmd"].toString(), deck = c["deck"].toString();
            Result r { false, "no handler" }; juce::WaitableEvent done;
            juce::MessageManager::callAsync([&] { if (onCommand) r = onCommand(cmd, deck); done.signal(); });
            done.wait(3000);
            auto ack = new juce::DynamicObject(); ack->setProperty("id", id); ack->setProperty("ok", r.ok); ack->setProperty("detail", r.detail);
            ack->setProperty("status", statusOnMessageThread());
            juce::String ignored; request("/api/studio/ack", juce::JSON::toString(juce::var(ack), true), ignored, 8000);
        }
    }
}

void StudioLink::post(const juce::String& path, const juce::var& body, std::function<void(int, juce::var)> done) {
    auto json = juce::JSON::toString(body, true);
    juce::Thread::launch([this, path, json, done] {
        juce::String out; int code = request(path, json, out, 8000); auto v = juce::JSON::parse(out);
        juce::MessageManager::callAsync([done, code, v] { if (done) done(code, v); });
    });
}
