#include "MainComponent.h"
#include "BinaryData.h"
#include "platform/SecretStore.h"

static const char* kServerKey = "server_url";
static const char* kTokenKey = "api_token";
// GO LIVE (Icecast DJ input). Password lives in the Keychain like the API token; never in the repo or a file.
static const char* kLiveHost = "live_host"; static const char* kLivePort = "live_port"; static const char* kLiveMount = "live_mount";
static const char* kLiveUser = "live_user"; static const char* kLivePass = "live_password";
static juce::String orDefault(const char* key, const juce::String& d) { auto v = etb::platform::loadSecret(key); return v.isEmpty() ? d : v; }

MainComponent::MainComponent() {
    formats.registerBasicFormats();
    logo = juce::ImageCache::getFromMemory(BinaryData::logo256_png, BinaryData::logo256_pngSize);
    for (auto* d : { &va, &vb }) addAndMakeVisible(d);
    va.onLoad = [this] { chooseFile(a); }; vb.onLoad = [this] { chooseFile(b); };
    xfader.setSliderStyle(juce::Slider::LinearHorizontal); xfader.setRange(0, 1, 0.001); xfader.setValue(0.5);
    xfader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); xfader.setDoubleClickReturnValue(true, 0.5);
    xfader.onValueChange = [this] { xf = (float) xfader.getValue(); };
    addAndMakeVisible(xfader);
    killBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffc41d39));
    killBtn.setTooltip("Tap: cut live + mic (safe).  Hold 2 s: OFF AIR.  Works on the server, so it also works if this app crashes.");
    killBtn.addMouseListener(&killMouse, false);
    restoreBtn.onClick = [this] { kill("restore"); };
    settingsBtn.onClick = [this] { openSettings(); };
    goLiveBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2d36));
    goLiveBtn.setTooltip("Send your mix to the station (replaces the AutoDJ while you're on). Click again to end; the AutoDJ comes back by itself.");
    goLiveBtn.onClick = [this] { toggleLive(); };
    addAndMakeVisible(goLiveBtn);
    for (auto* c : { (juce::Component*) &killBtn, (juce::Component*) &restoreBtn, (juce::Component*) &settingsBtn, (juce::Component*) &linkLbl, (juce::Component*) &airLbl }) addAndMakeVisible(c);
    linkLbl.setText("SERVER: not set", juce::dontSendNotification); linkLbl.setColour(juce::Label::textColourId, juce::Colour(0xffffb84a));
    airLbl.setColour(juce::Label::textColourId, juce::Colour(0xff5dff86));
    link.getStatus = [this] { return status(); };
    link.onCommand = [this](const juce::String& c, const juce::String& d) { return command(c, d); };
    link.onLinkState = [this](bool ok, const juce::String& why) {
        linkLbl.setText(ok ? "REMOTE LINK: CONNECTED" : "REMOTE LINK: " + why.toUpperCase(), juce::dontSendNotification);
        linkLbl.setColour(juce::Label::textColourId, ok ? juce::Colour(0xff5dff86) : juce::Colour(0xffffb84a)); };
    setSize(1180, 640);
    setAudioChannels(0, 2);
    connect(); startTimerHz(30);
}

MainComponent::~MainComponent() { live.stop(); link.stop(); shutdownAudio(); }

void MainComponent::connect() {
    const auto url = etb::platform::loadSecret(kServerKey), tok = etb::platform::loadSecret(kTokenKey);
    if (url.isEmpty() || tok.isEmpty()) { linkLbl.setText("SERVER: not set (click SERVER)", juce::dontSendNotification); return; }
    linkLbl.setText("REMOTE LINK: connecting...", juce::dontSendNotification);
    link.start(url, tok);
}

void MainComponent::openSettings() {
    auto* w = new juce::AlertWindow("Station server", "Passwords and tokens are stored in the macOS Keychain (never in a file).\n\nGO LIVE = your DJ login (Icecast source). Defaults work for Lowride Life Radio; just type the DJ password.", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("url", orDefault(kServerKey, "https://stream.lowrideliferadio.com"), "Server URL (remote link)");
    w->addTextEditor("tok", "", "API token (leave empty to keep)", true);
    w->addTextEditor("lhost", orDefault(kLiveHost, "stream.lowrideliferadio.com"), "GO LIVE host");
    w->addTextEditor("lport", orDefault(kLivePort, "8000"), "GO LIVE port");
    w->addTextEditor("lmount", orDefault(kLiveMount, "/live"), "GO LIVE mount");
    w->addTextEditor("luser", orDefault(kLiveUser, "source"), "GO LIVE username");
    w->addTextEditor("lpass", "", etb::platform::loadSecret(kLivePass).isEmpty() ? "GO LIVE password (DJ password)" : "GO LIVE password (saved - leave empty to keep)", true);
    w->addButton("SAVE", 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("TEST GO LIVE LOGIN", 2); w->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create([this, w](int r) {
        if (r == 1 || r == 2) {
            auto url = w->getTextEditorContents("url").trim(); auto tok = w->getTextEditorContents("tok").trim();
            if (! url.startsWithIgnoreCase("https://") && ! url.startsWithIgnoreCase("http://127.0.0.1") && ! url.startsWithIgnoreCase("http://localhost"))
                { juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "HTTPS only", "Use an https:// server URL."); return; }
            auto lhost = w->getTextEditorContents("lhost").trim().replace("https://", "").replace("http://", "").upToFirstOccurrenceOf("/", false, false);
            auto lport = w->getTextEditorContents("lport").trim().getIntValue();
            auto lmount = w->getTextEditorContents("lmount").trim(); if (! lmount.startsWith("/")) lmount = "/" + lmount;
            if (lhost.isEmpty() || lport <= 0 || lport > 65535)
                { juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "GO LIVE", "Check the GO LIVE host and port (Lowride Life Radio: stream.lowrideliferadio.com, 8000)."); return; }
            etb::platform::storeSecret(kServerKey, url); if (tok.isNotEmpty()) etb::platform::storeSecret(kTokenKey, tok);
            etb::platform::storeSecret(kLiveHost, lhost); etb::platform::storeSecret(kLivePort, juce::String(lport));
            etb::platform::storeSecret(kLiveMount, lmount); etb::platform::storeSecret(kLiveUser, w->getTextEditorContents("luser").trim().isEmpty() ? "source" : w->getTextEditorContents("luser").trim());
            if (w->getTextEditorContents("lpass").isNotEmpty()) etb::platform::storeSecret(kLivePass, w->getTextEditorContents("lpass"));
            if (r == 2) {
                const auto s = liveSettings();
                juce::Thread::launch([s] {
                    const auto why = IcecastSource::probe(s);
                    juce::MessageManager::callAsync([why] { juce::AlertWindow::showMessageBoxAsync(why.isEmpty() ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                        "GO LIVE login test", why.isEmpty() ? "Login OK - you're ready to GO LIVE. (Nothing was sent on air.)" : why); }); });
            }
            connect();
        } }), true);
}

IcecastSource::Settings MainComponent::liveSettings() const {
    IcecastSource::Settings s;
    s.host = orDefault(kLiveHost, s.host); s.port = orDefault(kLivePort, juce::String(s.port)).getIntValue();
    s.mount = orDefault(kLiveMount, s.mount); s.user = orDefault(kLiveUser, s.user);
    s.password = etb::platform::loadSecret(kLivePass);
    return s;
}

void MainComponent::startLive() {
    if (liveSettings().password.isEmpty()) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "GO LIVE", "Click SERVER and type your DJ password under \"GO LIVE password\" first.");
        return;
    }
    live.start(liveSettings(), sr);
}

void MainComponent::toggleLive() {
    if (live.isOn()) live.stop(); else startLive();
    timerCallback();
}

void MainComponent::kill(const juce::String& mode) {   // mode: safe | off | restore
    auto body = new juce::DynamicObject(); body->setProperty("stage", mode); body->setProperty("reason", "studio app");
    airLbl.setText("KILL: sending " + mode.toUpperCase() + "...", juce::dontSendNotification);
    link.post("/api/kill", juce::var(body), [this, mode](int code, juce::var) {
        airLbl.setText(code == 200 ? "AIR: " + mode.toUpperCase() : "KILL FAILED (HTTP " + juce::String(code) + ") - use the phone remote", juce::dontSendNotification);
        airLbl.setColour(juce::Label::textColourId, code == 200 && mode == "restore" ? juce::Colour(0xff5dff86) : juce::Colour(0xffff4f6a)); });
    // local safety too: silence both decks immediately when going off air
    if (mode == "off") { a.motor = false; b.motor = false; }
}

void MainComponent::KillMouse::mouseUp(const juce::MouseEvent&) {
    if (! m.killHeld)
        return;
    m.killHeld = false;
    m.kill(juce::Time::getMillisecondCounter() - m.killDownAt >= 2000 ? "off" : "safe");
}

void MainComponent::chooseFile(DeckChannel& ch) {
    chooser = std::make_unique<juce::FileChooser>("Load a track", juce::File::getSpecialLocation(juce::File::userMusicDirectory), formats.getWildcardForAllFormats());
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, &ch](const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) loadInto(ch, fc.getResult()); });
}

void MainComponent::loadInto(DeckChannel& ch, const juce::File& f) {
    std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(f));
    if (! r || r->lengthInSamples <= 0) { juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Can't read file", f.getFileName()); return; }
    const int n = (int) juce::jmin<juce::int64>(r->lengthInSamples, (juce::int64) r->sampleRate * 60 * 20);   // 20 min cap per deck
    juce::AudioBuffer<float> buf(2, n); r->read(&buf, 0, n, 0, true, r->numChannels > 1);
    if (r->numChannels == 1) buf.copyFrom(1, 0, buf, 0, 0, n);
    std::vector<float> inter((size_t) n * 2);
    for (int i = 0; i < n; ++i) { inter[(size_t) i * 2] = buf.getSample(0, i); inter[(size_t) i * 2 + 1] = buf.getSample(1, i); }
    {
        const juce::SpinLock::ScopedLockType sl(ch.swapLock);
        ch.motor = false; ch.interleaved.swap(inter);
        ch.deck.config().trackRate = r->sampleRate; ch.deck.config().sampleRate = sr;
        ch.deck.setTrack(ch.interleaved.data(), (size_t) n, 2); ch.deck.cue(0);
        ch.lengthFrames = n; ch.trackRate = r->sampleRate; ch.loaded = true;
    }
    ch.title = f.getFileNameWithoutExtension();
}

void MainComponent::prepareToPlay(int block, double rate) {
    sr = rate; smooth.prepare(rate);
    for (int i = 0; i < 2; ++i) { tmpL[i].assign((size_t) block * 2, 0.f); tmpR[i].assign((size_t) block * 2, 0.f); }
    for (auto* c : { &a, &b }) { const juce::SpinLock::ScopedLockType sl(c->swapLock); c->deck.config().sampleRate = rate; }
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& info) {
    info.clearActiveBufferRegion();
    const int n = info.numSamples; if (info.buffer->getNumChannels() < 2) return;
    DeckChannel* chans[2] = { &a, &b };
    for (int k = 0; k < 2; ++k) {
        auto& c = *chans[k];
        if ((int) tmpL[k].size() < n) { std::fill(tmpL[k].begin(), tmpL[k].end(), 0.f); std::fill(tmpR[k].begin(), tmpR[k].end(), 0.f); continue; }   // never allocate here
        const juce::SpinLock::ScopedTryLockType sl(c.swapLock);
        if (! sl.isLocked() || ! c.loaded) { std::fill_n(tmpL[k].data(), n, 0.f); std::fill_n(tmpR[k].data(), n, 0.f); continue; }
        c.deck.setMotor(c.motor); c.deck.setPitch(c.pitch);
        if (c.touch != c.deck.touched()) c.deck.touch(c.touch);
        const double revs = c.pendingRevs.exchange(0.0); if (std::abs(revs) > 1e-12) c.deck.handMove(revs, 0);
        c.deck.process(tmpL[k].data(), tmpR[k].data(), n);
        c.angleDeg = c.deck.recordAngleDeg(); c.position = c.deck.position();
    }
    double gA, gB; curve.gains(xf.load(), gA, gB);
    if (a.killed) gA = 0;
    if (b.killed) gB = 0;
    auto* L = info.buffer->getWritePointer(0, info.startSample); auto* R = info.buffer->getWritePointer(1, info.startSample);
    for (int i = 0; i < n; ++i) { smooth.step(gA, gB);
        L[i] = (float) (tmpL[0][(size_t) i] * smooth.a + tmpL[1][(size_t) i] * smooth.b);
        R[i] = (float) (tmpR[0][(size_t) i] * smooth.a + tmpR[1][(size_t) i] * smooth.b); }
    live.pushAudio(L, R, n);   // GO LIVE: lock-free hand-off to the encoder thread (no-op when off air)
}

juce::var MainComponent::status() {
    auto deck = [](DeckChannel& c) { auto o = new juce::DynamicObject(); o->setProperty("title", c.title); o->setProperty("playing", c.motor.load() && c.loaded.load());
        o->setProperty("killed", c.killed.load()); return juce::var(o); };
    auto decks = new juce::DynamicObject(); decks->setProperty("A", deck(a)); decks->setProperty("B", deck(b));
    auto liveObj = new juce::DynamicObject(); liveObj->setProperty("on", live.state() == IcecastSource::State::live); liveObj->setProperty("available", true); liveObj->setProperty("state", live.lastMessage());
    auto s = new juce::DynamicObject(); s->setProperty("decks", juce::var(decks)); s->setProperty("live", juce::var(liveObj)); s->setProperty("mic", false);
    s->setProperty("app", "ET Broadcaster Studio"); s->setProperty("version", JUCE_APPLICATION_VERSION_STRING);
    return juce::var(s);
}

StudioLink::Result MainComponent::command(const juce::String& cmd, const juce::String& deck) {
    DeckChannel* c = deck == "A" ? &a : deck == "B" ? &b : nullptr;
    if (cmd == "deck.play" && c) { if (! c->loaded) return { false, "deck " + deck + " is empty" }; c->killed = false; c->motor = true; (c == &a ? va : vb).kill.setToggleState(false, juce::dontSendNotification); return { true, "playing" }; }
    if (cmd == "deck.stop" && c) { c->motor = false; return { true, "stopped" }; }
    if (cmd == "deck.kill" && c) { c->killed = true; (c == &a ? va : vb).kill.setToggleState(true, juce::dontSendNotification); return { true, "deck muted" }; }
    if (cmd == "golive.start") {
        if (liveSettings().password.isEmpty()) return { false, "no DJ password saved in the Mac app (SERVER)" };
        if (! live.isOn()) startLive();
        return { true, "going live" };
    }
    if (cmd == "golive.stop") { live.stop(); timerCallback(); return { true, "live ended - AutoDJ resumes" }; }
    if (cmd.startsWith("mic."))
        return { false, "mic is not in this build yet" };
    return { false, "unknown command" };
}

void MainComponent::timerCallback() {
    if (killHeld) killBtn.setButtonText(juce::Time::getMillisecondCounter() - killDownAt >= 2000 ? "RELEASE = OFF AIR" : "KILL (hold 2s = OFF)");
    else killBtn.setButtonText("KILL");
    using S = IcecastSource::State; const auto ls = live.state();
    juce::String t = "GO LIVE"; juce::Colour col(0xff2a2d36);
    if (ls == S::live) { const int secs = (int) live.secondsOnAir(); t = juce::String::formatted("ON AIR %d:%02d", secs / 60, secs % 60); col = juce::Colour(0xffc41d39); }
    else if (ls == S::connecting) { t = "CONNECTING..."; col = juce::Colour(0xffb5761c); }
    else if (ls == S::error) { t = "RETRYING..."; col = juce::Colour(0xffb5761c); }
    if (goLiveBtn.getButtonText() != t) goLiveBtn.setButtonText(t);
    goLiveBtn.setColour(juce::TextButton::buttonColourId, col);
    static S shown = S::idle; static juce::String shownMsg;
    if (ls != shown || (ls == S::error && live.lastMessage() != shownMsg)) {
        shown = ls; shownMsg = live.lastMessage();
        airLbl.setText(ls == S::idle ? juce::String("LIVE: OFF (AutoDJ on air)") : "LIVE: " + shownMsg, juce::dontSendNotification);
        airLbl.setColour(juce::Label::textColourId, ls == S::live ? juce::Colour(0xffff4f6a) : ls == S::idle ? juce::Colour(0xff98a1b3) : juce::Colour(0xffffb84a));
    }
}

void MainComponent::paint(juce::Graphics& g) {
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1b1c21), 0, 0, juce::Colour(0xff0a0a0d), 0, (float) getHeight(), false)); g.fillAll();
    if (logo.isValid()) g.drawImageWithin(logo, 16, 10, 48, 48, juce::RectanglePlacement::centred);
    g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(22.0f, juce::Font::bold)); g.drawText("E.T.TATTOO", 72, 12, 220, 26, juce::Justification::left);
    g.setColour(juce::Colour(0xff98a1b3)); g.setFont(juce::FontOptions(12.0f)); g.drawText("ET BROADCASTER STUDIO", 72, 36, 220, 16, juce::Justification::left);
    g.setColour(juce::Colour(0xff98a1b3)); g.drawText("A", xfader.getBounds().withWidth(20).translated(-24, 0), juce::Justification::centred);
    g.drawText("B", xfader.getBounds().withX(xfader.getRight() + 4).withWidth(20), juce::Justification::centred);
}

void MainComponent::resized() {
    auto r = getLocalBounds().reduced(12); auto head = r.removeFromTop(56);
    head.removeFromLeft(300);
    settingsBtn.setBounds(head.removeFromRight(90).reduced(0, 10)); head.removeFromRight(8);
    restoreBtn.setBounds(head.removeFromRight(100).reduced(0, 10)); head.removeFromRight(8);
    killBtn.setBounds(head.removeFromRight(170).reduced(0, 4)); head.removeFromRight(12);
    goLiveBtn.setBounds(head.removeFromRight(140).reduced(0, 4)); head.removeFromRight(12);
    linkLbl.setBounds(head.removeFromTop(28)); airLbl.setBounds(head);
    auto bottom = r.removeFromBottom(44); xfader.setBounds(bottom.withSizeKeepingCentre(juce::jmin(420, bottom.getWidth() - 60), 32));
    r.removeFromBottom(8);
    va.setBounds(r.removeFromLeft(r.getWidth() / 2).reduced(0, 0).withTrimmedRight(6)); vb.setBounds(r.withTrimmedLeft(6));
}
