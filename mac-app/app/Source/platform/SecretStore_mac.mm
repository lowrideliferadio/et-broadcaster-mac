// macOS Keychain (generic password items, service "com.lowrideliferadio.etbroadcaster").
#include "SecretStore.h"
#import <Foundation/Foundation.h>
#import <Security/Security.h>
namespace etb::platform {
static NSString* const kService = @"com.lowrideliferadio.etbroadcaster";
static NSMutableDictionary* query(const juce::String& key) {
    return [@{ (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
               (__bridge id)kSecAttrService: kService,
               (__bridge id)kSecAttrAccount: [NSString stringWithUTF8String: key.toRawUTF8()] } mutableCopy];
}
bool storeSecret(const juce::String& key, const juce::String& value) {
    NSData* data = [[NSString stringWithUTF8String: value.toRawUTF8()] dataUsingEncoding: NSUTF8StringEncoding];
    NSMutableDictionary* q = query(key);
    SecItemDelete((__bridge CFDictionaryRef) q);
    q[(__bridge id)kSecValueData] = data;
    q[(__bridge id)kSecAttrAccessible] = (__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;
    return SecItemAdd((__bridge CFDictionaryRef) q, nullptr) == errSecSuccess;
}
juce::String loadSecret(const juce::String& key) {
    NSMutableDictionary* q = query(key);
    q[(__bridge id)kSecReturnData] = @YES; q[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
    CFTypeRef out = nullptr;
    if (SecItemCopyMatching((__bridge CFDictionaryRef) q, &out) != errSecSuccess || out == nullptr) return {};
    NSData* d = (__bridge_transfer NSData*) out;
    NSString* s = [[NSString alloc] initWithData: d encoding: NSUTF8StringEncoding];
    return juce::String::fromUTF8([s UTF8String]);
}
bool deleteSecret(const juce::String& key) { return SecItemDelete((__bridge CFDictionaryRef) query(key)) == errSecSuccess; }
}
