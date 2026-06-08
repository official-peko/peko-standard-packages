/*
 * peko_keychain_win.c
 * Secure credential backend for Windows.
 * Stores secrets as generic credentials in the Windows Credential Manager.
 * The target name is peko.app/ followed by the key. Each blocking credential
 * call runs inside a pgc_begin_blocking and pgc_end_blocking bracket. The
 * managed return buffer is allocated only after the bracket ends.
 */

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincred.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "advapi32.lib")

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

/* Returns the credential target prefix. The namespace is used when set, with a
 * fixed default otherwise. */
static const char *active_prefix(void)
{
    return (g_namespace && g_namespace[0] != '\0') ? g_namespace : "peko.app";
}

/* Builds a malloc'd wide target name of the form <prefix>/<key>.
 * Returns NULL on failure. The key is read before any blocking call, so the
 * managed source bytes stay in place during the copy. */
static wchar_t *make_target(const char *key)
{
    const char *prefix = active_prefix();
    size_t pn = strlen(prefix);
    size_t kn = strlen(key);

    char *utf8 = (char *)malloc(pn + 1 + kn + 1);
    if (!utf8) {
        return NULL;
    }
    memcpy(utf8, prefix, pn);
    utf8[pn] = '/';
    memcpy(utf8 + pn + 1, key, kn + 1);

    int wn = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (wn <= 0) {
        free(utf8);
        return NULL;
    }
    wchar_t *w = (wchar_t *)malloc((size_t)wn * sizeof(wchar_t));
    if (!w) {
        free(utf8);
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, wn);
    free(utf8);
    return w;
}

/* Stores value under key. Replaces any existing credential. Returns 1 on
 * success. */
int peko_keychain_set(const char *key, const char *value)
{
    if (!key || !value) {
        return 0;
    }

    wchar_t *target = make_target(key);
    if (!target) {
        return 0;
    }

    /* Copy the secret into stable malloc memory before parking. The argument
     * references managed memory that can move during a collection. */
    size_t vn = strlen(value);
    char *v = (char *)malloc(vn + 1);
    if (!v) {
        free(target);
        return 0;
    }
    memcpy(v, value, vn + 1);

    CREDENTIALW cred;
    memset(&cred, 0, sizeof(cred));
    cred.Type              = CRED_TYPE_GENERIC;
    cred.TargetName        = target;
    cred.CredentialBlobSize = (DWORD)vn;
    cred.CredentialBlob    = (LPBYTE)v;
    cred.Persist           = CRED_PERSIST_LOCAL_MACHINE;

    BOOL ok;
    pgc_begin_blocking();
    ok = CredWriteW(&cred, 0);
    pgc_end_blocking();

    SecureZeroMemory(v, vn);
    free(v);
    free(target);
    return ok ? 1 : 0;
}

/* Reads the secret stored under key. Returns a GC-managed string, or NULL
 * when the key is absent. */
const char *peko_keychain_get(const char *key)
{
    if (!key) {
        return NULL;
    }

    wchar_t *target = make_target(key);
    if (!target) {
        return NULL;
    }

    PCREDENTIALW cred = NULL;
    BOOL ok;
    pgc_begin_blocking();
    ok = CredReadW(target, CRED_TYPE_GENERIC, 0, &cred);
    pgc_end_blocking();

    free(target);

    if (!ok || !cred) {
        if (cred) {
            CredFree(cred);
        }
        return NULL;
    }

    size_t n = (size_t)cred->CredentialBlobSize;
    char *gc = (char *)pgc_alloc_atomic(n + 1);
    if (gc) {
        if (n > 0 && cred->CredentialBlob) {
            memcpy(gc, cred->CredentialBlob, n);
        }
        gc[n] = '\0';
    }
    CredFree(cred);
    return gc;
}

/* Removes the secret stored under key. A missing credential is a success.
 * Returns 1 on success. */
int peko_keychain_remove(const char *key)
{
    if (!key) {
        return 0;
    }

    wchar_t *target = make_target(key);
    if (!target) {
        return 0;
    }

    BOOL ok;
    DWORD err = 0;
    pgc_begin_blocking();
    ok = CredDeleteW(target, CRED_TYPE_GENERIC, 0);
    if (!ok) {
        err = GetLastError();
    }
    pgc_end_blocking();

    free(target);
    return (ok || err == ERROR_NOT_FOUND) ? 1 : 0;
}

#endif /* _WIN32 */
