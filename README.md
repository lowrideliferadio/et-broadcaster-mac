# ET Broadcaster — Mac studio app

Source for the unsigned macOS `.dmg`. JUCE is cloned at build time (not vendored).

## Get the .dmg

1. GitHub → Actions → **macOS DMG** → *Run workflow* (tick *universal* for Intel + Apple Silicon).
2. After 10–15 minutes, download the **ET-Broadcaster-dmg** artifact.

Or on a Mac: `xcode-select --install`, `brew install cmake create-dmg`, then `cd mac-app && ./build-dmg.sh`.

The app is unsigned (no paid Apple Developer ID). First launch: right-click → Open. See `mac-app/README-MAC.md`.

This repository is public so standard GitHub-hosted macOS runners stay free. Do not commit tokens, `.env` files, or server credentials. The Mac app stores the station API token in the Keychain at runtime.
