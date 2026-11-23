# ET Broadcaster Studio — Mac app (.dmg)

## Get the .dmg (no Mac build tools needed)
1. Push this project to a GitHub repo (private is fine).
2. GitHub → Actions → **macOS DMG** → *Run workflow* (tick *universal* if you also want Intel Macs).
3. When it finishes (~10–15 min), download the **ET-Broadcaster-dmg** artifact → `ET-Broadcaster-0.1.0-arm64.dmg`.

Or build on the M1 yourself: `xcode-select --install`, `brew install cmake create-dmg`, then `cd mac-app && ./build-dmg.sh`.

## Install
Open the .dmg and drag **ET Broadcaster** onto **Applications** (the E.T.TATTOO window shows where).

### First launch (unsigned app — Gatekeeper)
The app is not signed with a paid Apple Developer ID ($99/yr), so macOS blocks a normal double-click the first time:
1. In Applications, **right-click (or Control-click) ET Broadcaster → Open**.
2. Click **Open** in the warning. macOS remembers this; after that it opens normally.
3. On macOS 15+, if there is no Open button: try to open it once, then go to **System Settings → Privacy & Security**, scroll down to
   "ET Broadcaster was blocked…" and click **Open Anyway**.
(Terminal alternative: `xattr -dr com.apple.quarantine "/Applications/ET Broadcaster.app"`.)

## Connect it to the station
Click **SERVER** → enter `https://stream.lowrideliferadio.com` and the **Mac app API token** issued by the station
server (owner studio). The token is stored in the **macOS Keychain** (never in a file).
The header then shows **REMOTE LINK: CONNECTED** and the phone remote's *Mac studio* card can play/stop/kill the decks.
The link is plain HTTPS long-polling (`/api/studio/link`), so it works on any network that allows websites.

## What's in this build (honest)
| Works | Not yet |
|---|---|
| Two decks: load a file, play/stop with motor start/brake, pitch ±8 %, grab-and-scratch on perfectly round platters (sinc resampler engine) | GO LIVE encoder (sending the mix to the server's live input) — use your current encoder app for now |
| Crossfader (click-free gain smoothing) | Mic channel, MIDI jog mapping UI (engine supports MIDI jog decoding) |
| KILL: tap = safe (cut live + mic), hold 2 s = OFF AIR, RESTORE — runs on the server | Library sync / upload from the Mac (use the studio web page's Library → upload for now; same resumable API) |
| Remote link: phone remote can play/stop/kill decks; status shows on the phone | Spin-down animation on KILL |

## Can it be built from Linux (osxcross)? — No, not reliably
osxcross needs the macOS SDK extracted from Xcode (Apple's licence only allows using it on Apple hardware), JUCE's
CMake build then needs `juceaide`, Objective-C++ frameworks and `actool`/`ibtool`-style resource steps that osxcross
does not provide, and the result still has to be ad-hoc signed for Apple Silicon (`codesign` is macOS-only; `rcodesign`
can do it but is another moving part). The GitHub macOS runner is free, legal and produces a real build, so that is the
supported path. What *is* verified on Linux: the same sources compile and link as a Linux app against JUCE 9.0.5
(the only Mac-specific file, `Source/platform/SecretStore_mac.mm`, is swapped for the portable one).

## Cross-platform rule
Everything outside `app/Source/platform/` must stay portable C++ (engine has no JUCE at all). Windows comes later:
add `SecretStore_win.cpp` (Credential Manager) and an Inno Setup/MSIX installer — see `../PRODUCT-LATER.md`.
