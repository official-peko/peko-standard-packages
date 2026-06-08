/*
 * peko_keychain_android.c
 * Secure credential backend for Android.
 * Android has no secret service, so secrets are stored in an encrypted file
 * under the app private directory using libsodium. The app private directory
 * is sandboxed per application by the operating system.
 */

#ifdef __ANDROID__

#include "peko_keychain_sodium.h"

/* Active namespace recorded for parity with the other backends. The Android
 * data directory is already per-app, so it provides the isolation. */
static char *g_namespace = NULL;

void peko_keychain_set_namespace(const char *app_id)
{
    PEKO_KC_LOG("android: set_namespace app_id=%s", app_id ? app_id : "(null)");
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

static const char *namespace_or_empty(void)
{
    return (g_namespace && g_namespace[0] != '\0') ? g_namespace : "";
}

int peko_keychain_set(const char *key, const char *value)
{
    PEKO_KC_LOG("android: keychain_set key=%s", key ? key : "(null)");
    int r = sodium_kc_set(namespace_or_empty(), key, value);
    PEKO_KC_LOG("android: keychain_set returned %d", r);
    return r;
}

const char *peko_keychain_get(const char *key)
{
    PEKO_KC_LOG("android: keychain_get key=%s", key ? key : "(null)");
    const char *r = sodium_kc_get(namespace_or_empty(), key);
    PEKO_KC_LOG("android: keychain_get returned %s", r ? "value" : "null");
    return r;
}

int peko_keychain_remove(const char *key)
{
    PEKO_KC_LOG("android: keychain_remove key=%s", key ? key : "(null)");
    int r = sodium_kc_remove(namespace_or_empty(), key);
    PEKO_KC_LOG("android: keychain_remove returned %d", r);
    return r;
}

#endif /* __ANDROID__ */
