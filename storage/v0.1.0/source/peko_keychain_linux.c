/*
 * peko_keychain_linux.c
 * Secure credential backend for desktop Linux.
 * libsecret is loaded at runtime with dlopen. When the secret service client
 * loads and resolves, secrets are stored in the platform secret service. When
 * libsecret cannot be loaded, secrets fall back to an encrypted file store.
 * Each blocking call runs inside a pgc_begin_blocking and pgc_end_blocking
 * bracket. The managed return buffer is allocated only after the bracket ends.
 */

#if defined(__linux__) && !defined(__ANDROID__)

#include <dlfcn.h>
#include <libsecret/secret.h>

#include "peko_keychain_sodium.h"

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

/* Returns the namespace value for the secret service app attribute. */
static const char *active_namespace(void)
{
    return (g_namespace && g_namespace[0] != '\0') ? g_namespace : "default";
}

/* Returns the secrets directory app id. An empty namespace lets the path
 * resolver choose its platform default. */
static const char *namespace_or_empty(void)
{
    return (g_namespace && g_namespace[0] != '\0') ? g_namespace : "";
}

/* libsecret function pointers resolved with dlsym. */
typedef gboolean (*pf_store)(const SecretSchema *, const gchar *, const gchar *,
                             const gchar *, void *, GError **, ...);
typedef gchar   *(*pf_lookup)(const SecretSchema *, void *, GError **, ...);
typedef gboolean (*pf_clear)(const SecretSchema *, void *, GError **, ...);
typedef void     (*pf_free)(gchar *);

static struct {
    int       state;   /* -1 unknown, 0 unavailable, 1 available */
    void     *handle;
    pf_store  store;
    pf_lookup lookup;
    pf_clear  clear;
    pf_free   free_pw;
} g_secret = { -1, NULL, NULL, NULL, NULL, NULL };

/* Loads libsecret on first use and resolves the required functions. The
 * RTLD_NOW flag resolves all symbols at load time, so a mismatch against the
 * loaded glib version reports a load failure rather than a later crash.
 * Returns 1 when the secret service client is available. */
static int load_secret(void)
{
    if (g_secret.state >= 0) {
        return g_secret.state;
    }

    void *h = dlopen("libsecret-1.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        h = dlopen("libsecret-1.so", RTLD_NOW | RTLD_LOCAL);
    }
    if (!h) {
        g_secret.state = 0;
        return 0;
    }

    g_secret.store   = (pf_store) dlsym(h, "secret_password_store_sync");
    g_secret.lookup  = (pf_lookup)dlsym(h, "secret_password_lookup_sync");
    g_secret.clear   = (pf_clear) dlsym(h, "secret_password_clear_sync");
    g_secret.free_pw = (pf_free)  dlsym(h, "secret_password_free");

    if (!g_secret.store || !g_secret.lookup || !g_secret.clear || !g_secret.free_pw) {
        dlclose(h);
        g_secret.state = 0;
        return 0;
    }

    g_secret.handle = h;
    g_secret.state = 1;
    return 1;
}

/* Returns the credential schema with app and key attributes. */
static const SecretSchema *peko_schema(void)
{
    static const SecretSchema schema = {
        "app.peko.Credentials",
        SECRET_SCHEMA_NONE,
        {
            { "app", SECRET_SCHEMA_ATTRIBUTE_STRING },
            { "key", SECRET_SCHEMA_ATTRIBUTE_STRING },
            { NULL, 0 }
        }
    };
    return &schema;
}

/* Stores value under key. Routes to the secret service when available, and to
 * the encrypted file store otherwise. Returns 1 on success. */
int peko_keychain_set(const char *key, const char *value)
{
    if (!key || !value) {
        return 0;
    }
    if (!load_secret()) {
        return sodium_kc_set(namespace_or_empty(), key, value);
    }

    char *k = sodium_dup_cstr(key);
    char *v = sodium_dup_cstr(value);
    const char *ns = active_namespace();
    if (!k || !v) {
        free(k);
        free(v);
        return 0;
    }

    gboolean ok;
    pgc_begin_blocking();
    ok = g_secret.store(peko_schema(), SECRET_COLLECTION_DEFAULT,
                        "Peko credential", v, NULL, NULL,
                        "app", ns, "key", k, NULL);
    pgc_end_blocking();

    free(k);
    free(v);
    return ok ? 1 : 0;
}

/* Reads the secret stored under key. Returns a GC-managed string, or NULL when
 * the key is absent. */
const char *peko_keychain_get(const char *key)
{
    if (!key) {
        return NULL;
    }
    if (!load_secret()) {
        return sodium_kc_get(namespace_or_empty(), key);
    }

    char *k = sodium_dup_cstr(key);
    const char *ns = active_namespace();
    if (!k) {
        return NULL;
    }

    gchar *secret = NULL;
    pgc_begin_blocking();
    secret = g_secret.lookup(peko_schema(), NULL, NULL,
                             "app", ns, "key", k, NULL);
    pgc_end_blocking();

    free(k);

    if (!secret) {
        return NULL;
    }

    size_t n = strlen(secret);
    char *gc = (char *)pgc_alloc_atomic(n + 1);
    if (gc) {
        memcpy(gc, secret, n + 1);
    }
    g_secret.free_pw(secret);
    return gc;
}

/* Removes the secret stored under key. A missing item is a success. Returns 1
 * on success. */
int peko_keychain_remove(const char *key)
{
    if (!key) {
        return 0;
    }
    if (!load_secret()) {
        return sodium_kc_remove(namespace_or_empty(), key);
    }

    char *k = sodium_dup_cstr(key);
    const char *ns = active_namespace();
    if (!k) {
        return 0;
    }

    pgc_begin_blocking();
    g_secret.clear(peko_schema(), NULL, NULL, "app", ns, "key", k, NULL);
    pgc_end_blocking();

    free(k);
    return 1;
}

#endif /* __linux__ && !__ANDROID__ */
