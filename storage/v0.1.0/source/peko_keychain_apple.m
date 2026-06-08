/*
 * peko_keychain_apple.m
 * Secure credential backend for macOS and iOS.
 * Stores secrets as generic password items in the platform keychain. The
 * service attribute is the main bundle identifier. Each blocking keychain
 * call runs inside a pgc_begin_blocking and pgc_end_blocking bracket. The
 * managed return buffer is allocated only after the bracket ends.
 */

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#include <stdlib.h>
#include <string.h>

/* Peko GC interface. These symbols resolve from the pgc runtime object. */
extern void *pgc_alloc_atomic(size_t size);
extern void  pgc_begin_blocking(void);
extern void  pgc_end_blocking(void);

/* Active namespace used to isolate secrets per application. */
static char *g_namespace = NULL;

void peko_keychain_set_namespace(const char *app_id)
{
    free(g_namespace);
    g_namespace = NULL;
    if (app_id && app_id[0] != '\0') {
        size_t n = strlen(app_id) + 1;
        g_namespace = (char *)malloc(n);
        if (g_namespace) {
            memcpy(g_namespace, app_id, n);
        }
    }
}

/* Returns the keychain service name. The namespace is used when set, then the
 * main bundle identifier, then a fixed default. */
static NSString *keychain_service(void)
{
    if (g_namespace && g_namespace[0] != '\0') {
        return [NSString stringWithUTF8String:g_namespace];
    }
    NSString *ident = [[NSBundle mainBundle] bundleIdentifier];
    if (ident == nil) {
        ident = @"peko.app";
    }
    return ident;
}

/* Stores value under key. Replaces any existing item. Returns 1 on success. */
int peko_keychain_set(const char *key, const char *value)
{
    if (!key || !value) {
        return 0;
    }
    @autoreleasepool {
        NSString *account = [NSString stringWithUTF8String:key];
        NSString *service = keychain_service();
        NSData *data = [NSData dataWithBytes:value length:strlen(value)];

        NSDictionary *match = @{
            (__bridge id)kSecClass:       (__bridge id)kSecClassGenericPassword,
            (__bridge id)kSecAttrService: service,
            (__bridge id)kSecAttrAccount: account
        };
        NSDictionary *add = @{
            (__bridge id)kSecClass:        (__bridge id)kSecClassGenericPassword,
            (__bridge id)kSecAttrService:  service,
            (__bridge id)kSecAttrAccount:  account,
            (__bridge id)kSecValueData:    data,
            (__bridge id)kSecAttrAccessible:
                (__bridge id)kSecAttrAccessibleAfterFirstUnlock
        };

        OSStatus st;
        pgc_begin_blocking();
        SecItemDelete((__bridge CFDictionaryRef)match);
        st = SecItemAdd((__bridge CFDictionaryRef)add, NULL);
        pgc_end_blocking();

        return (st == errSecSuccess) ? 1 : 0;
    }
}

/* Reads the secret stored under key. Returns a GC-managed string, or NULL
 * when the key is absent. */
const char *peko_keychain_get(const char *key)
{
    if (!key) {
        return NULL;
    }
    @autoreleasepool {
        NSString *account = [NSString stringWithUTF8String:key];
        NSString *service = keychain_service();
        NSDictionary *query = @{
            (__bridge id)kSecClass:       (__bridge id)kSecClassGenericPassword,
            (__bridge id)kSecAttrService: service,
            (__bridge id)kSecAttrAccount: account,
            (__bridge id)kSecReturnData:  (__bridge id)kCFBooleanTrue,
            (__bridge id)kSecMatchLimit:  (__bridge id)kSecMatchLimitOne
        };

        CFTypeRef out = NULL;
        OSStatus st;
        pgc_begin_blocking();
        st = SecItemCopyMatching((__bridge CFDictionaryRef)query, &out);
        pgc_end_blocking();

        if (st != errSecSuccess || out == NULL) {
            if (out) {
                CFRelease(out);
            }
            return NULL;
        }

        NSData *data = (__bridge_transfer NSData *)out;
        size_t n = (size_t)data.length;

        char *gc = (char *)pgc_alloc_atomic(n + 1);
        if (!gc) {
            return NULL;
        }
        if (n > 0) {
            memcpy(gc, data.bytes, n);
        }
        gc[n] = '\0';
        return gc;
    }
}

/* Removes the secret stored under key. A missing item is a success.
 * Returns 1 on success. */
int peko_keychain_remove(const char *key)
{
    if (!key) {
        return 0;
    }
    @autoreleasepool {
        NSString *account = [NSString stringWithUTF8String:key];
        NSString *service = keychain_service();
        NSDictionary *query = @{
            (__bridge id)kSecClass:       (__bridge id)kSecClassGenericPassword,
            (__bridge id)kSecAttrService: service,
            (__bridge id)kSecAttrAccount: account
        };

        OSStatus st;
        pgc_begin_blocking();
        st = SecItemDelete((__bridge CFDictionaryRef)query);
        pgc_end_blocking();

        return (st == errSecSuccess || st == errSecItemNotFound) ? 1 : 0;
    }
}
