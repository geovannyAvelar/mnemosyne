#include "TokenStore.h"

#include <Security/Security.h>

#include <QByteArray>

namespace {

// Matches MACOSX_BUNDLE_GUI_IDENTIFIER in src/CMakeLists.txt, so tokens
// saved by the packaged app and the build-tree binary land in the same
// Keychain item namespace.
const CFStringRef kService = CFSTR("com.geovannyavelar.Mnemosyne");

} // namespace

namespace TokenStore {

void save(const QString &key, const QString &secret)
{
    const QByteArray account = key.toUtf8();
    const QByteArray data = secret.toUtf8();

    CFStringRef accountRef = CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(account.constData()),
                                                       account.size(), kCFStringEncodingUTF8, false);
    CFDataRef dataRef = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(data.constData()), data.size());

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                               &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, kService);
    CFDictionarySetValue(query, kSecAttrAccount, accountRef);

    // Remove any existing item first — simpler and more robust than
    // SecItemUpdate across the various attribute-matching edge cases.
    SecItemDelete(query);

    CFDictionarySetValue(query, kSecValueData, dataRef);
    CFDictionarySetValue(query, kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlock);
    SecItemAdd(query, nullptr);

    CFRelease(query);
    CFRelease(dataRef);
    CFRelease(accountRef);
}

QString load(const QString &key)
{
    const QByteArray account = key.toUtf8();
    CFStringRef accountRef = CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(account.constData()),
                                                       account.size(), kCFStringEncodingUTF8, false);

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                               &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, kService);
    CFDictionarySetValue(query, kSecAttrAccount, accountRef);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

    CFTypeRef result = nullptr;
    const OSStatus status = SecItemCopyMatching(query, &result);

    QString secret;
    if (status == errSecSuccess && result) {
        CFDataRef dataRef = static_cast<CFDataRef>(result);
        secret = QString::fromUtf8(reinterpret_cast<const char *>(CFDataGetBytePtr(dataRef)), CFDataGetLength(dataRef));
        CFRelease(result);
    }

    CFRelease(query);
    CFRelease(accountRef);
    return secret;
}

void remove(const QString &key)
{
    const QByteArray account = key.toUtf8();
    CFStringRef accountRef = CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(account.constData()),
                                                       account.size(), kCFStringEncodingUTF8, false);

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                               &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, kService);
    CFDictionarySetValue(query, kSecAttrAccount, accountRef);

    SecItemDelete(query);

    CFRelease(query);
    CFRelease(accountRef);
}

} // namespace TokenStore
