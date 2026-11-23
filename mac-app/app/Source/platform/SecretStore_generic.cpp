// Non-Apple fallback (Linux dev builds): one JSON file in the user config dir, chmod 0600.
// Windows will get a Credential Manager implementation instead (ROADMAP.md) — this file is not meant for it.
#include "SecretStore.h"
#if ! JUCE_WINDOWS
 #include <sys/stat.h>
#endif
namespace etb::platform {
static juce::File file() {
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("ETBroadcaster");
    dir.createDirectory(); return dir.getChildFile("secrets.json");
}
static juce::DynamicObject::Ptr readAll() {
    auto v = juce::JSON::parse(file()); if (auto* o = v.getDynamicObject()) return o; return new juce::DynamicObject();
}
static bool writeAll(juce::DynamicObject::Ptr o) {
    auto f = file(); bool ok = f.replaceWithText(juce::JSON::toString(juce::var(o.get())));
   #if ! JUCE_WINDOWS
    ::chmod(f.getFullPathName().toRawUTF8(), 0600);
   #endif
    return ok;
}
bool storeSecret(const juce::String& k, const juce::String& v) { auto o = readAll(); o->setProperty(k, v); return writeAll(o); }
juce::String loadSecret(const juce::String& k) { return readAll()->getProperty(k).toString(); }
bool deleteSecret(const juce::String& k) { auto o = readAll(); o->removeProperty(k); return writeAll(o); }
}
