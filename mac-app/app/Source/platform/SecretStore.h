// Platform layer: the ONLY place secrets (stream/API tokens) are stored.
//   macOS   -> Keychain (SecretStore_mac.mm)
//   Linux   -> file in the user config dir, mode 0600 (SecretStore_generic.cpp) — dev builds
//   Windows -> later: Credential Manager (CredWrite/CredRead) — see ROADMAP.md
#pragma once
#include <juce_core/juce_core.h>
namespace etb::platform {
bool storeSecret(const juce::String& key, const juce::String& value);
juce::String loadSecret(const juce::String& key);
bool deleteSecret(const juce::String& key);
}
