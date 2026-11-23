# Parked for later (not part of the personal Lowride Life Radio build)

Everything here is deliberately NOT built now. The code stays single-owner and single-station.

## Product / commercial (parked)
- Multi-tenant hosting (several stations on one server), sign-up, billing, per-station branding.
- Selling the Mac app; code signing + notarization (needs the $99/yr Apple Developer Program).
- Marketplace of deck skins.

## Windows version (later)
- The engine (`mac-app/engine`) is plain C++17 with no OS calls; the app uses JUCE, which builds on Windows.
- Only `mac-app/app/Source/platform/` may contain OS-specific code. Windows needs:
  - `SecretStore_win.cpp` using Credential Manager (`CredWriteW` / `CredReadW`), replacing the dev-only file store.
  - CMake generator: Visual Studio 2022; ASIO is optional (WASAPI exclusive is fine for a DJ deck).
  - Installer: Inno Setup or MSIX; a GitHub Actions `windows-latest` job mirroring `macos-dmg.yml`.
  - Unsigned EXE will trigger SmartScreen ("More info → Run anyway") unless a code-signing certificate is bought.
- Nothing in the server, website or studio remote is Mac-specific.

## Studio app features queued after the first DMG
- GO LIVE encoder (mix → MP3/AAC → server live input over TLS), mic channel with ducking, MIDI jog learn UI.
- Library sync: upload the Mac's local crates through the same resumable `/api/uploads` protocol the web uses.
- KILL spin-down animation and GO LIVE countdown.

## Website
- Service worker / offline mode: intentionally skipped so a cached script can never interfere with the live stream.
- Native app wrappers (the PWA "Add to Home Screen" covers phones for now).
