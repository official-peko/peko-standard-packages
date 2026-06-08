/*
 * peko_keychain_sodium.h
 * Encrypted file credential backend shared by the Linux fallback and Android.
 * Secrets are encrypted with libsodium and written to per-app files under the
 * storage data directory. The encryption key derives from a machine
 * identifier when one is readable, and from a per-install random key file
 * otherwise. This header defines static functions and is included by one
 * translation unit per platform.
 */

#ifndef PEKO_KEYCHAIN_SODIUM_H
#define PEKO_KEYCHAIN_SODIUM_H
#define SODIUM_STATIC 1

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <sodium.h>

/* Diagnostic log macro. Writes to the Android log on Android and to stderr on
 * other platforms. Set PEKO_KC_DEBUG to 0 to compile the tracing out. */
#define PEKO_KC_DEBUG 1
#if PEKO_KC_DEBUG
#  ifdef __ANDROID__
#    include <android/log.h>
#    define PEKO_KC_LOG(...) __android_log_print(ANDROID_LOG_INFO, "peko_keychain", __VA_ARGS__)
#  else
#    define PEKO_KC_LOG(...) (fprintf(stderr, "[peko_keychain] "), fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#  endif
#else
#  define PEKO_KC_LOG(...) ((void)0)
#endif

/* Peko GC interface. These symbols resolve from the pgc runtime object. */
extern void *pgc_alloc_atomic(size_t size);
extern void  pgc_begin_blocking(void);
extern void  pgc_end_blocking(void);

/* Resolves the per-app data directory. Implemented in peko_storage_path. */
extern char *peko_storage_data_dir(const char *app_id);

/* Copies a C string into a fresh malloc buffer. Returns NULL on failure. */
static char *sodium_dup_cstr(const char *s)
{
    size_t n = strlen(s) + 1;
    char *c = (char *)malloc(n);
    if (c) {
        memcpy(c, s, n);
    }
    return c;
}

/* Initializes libsodium once. Returns 0 on success. */
static int sodium_ready(void)
{
    static int ready = 0;
    if (ready) {
        return 0;
    }
    PEKO_KC_LOG("sodium_ready: calling sodium_init");
    int rc = sodium_init();
    PEKO_KC_LOG("sodium_ready: sodium_init returned %d", rc);
    if (rc < 0) {
        return -1;
    }
    ready = 1;
    return 0;
}

/* Builds the secrets directory path for app_id and creates it. Returns a
 * malloc'd path or NULL on failure. */
static char *sodium_secrets_dir(const char *app_id)
{
    PEKO_KC_LOG("secrets_dir: enter app_id=%s", app_id ? app_id : "(null)");
    char *data = peko_storage_data_dir(app_id ? app_id : "");
    if (!data) {
        PEKO_KC_LOG("secrets_dir: data_dir returned null");
        return NULL;
    }
    PEKO_KC_LOG("secrets_dir: data_dir=%s", data);
    size_t n = strlen(data) + strlen("/secrets") + 1;
    char *dir = (char *)malloc(n);
    if (!dir) {
        free(data);
        return NULL;
    }
    snprintf(dir, n, "%s/secrets", data);
    free(data);

    if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
        PEKO_KC_LOG("secrets_dir: mkdir failed dir=%s errno=%d", dir, errno);
        free(dir);
        return NULL;
    }
    PEKO_KC_LOG("secrets_dir: result=%s", dir);
    return dir;
}

/* Derives the encryption key into key, which holds crypto_secretbox_KEYBYTES
 * bytes. A readable machine identifier is hashed with a fixed salt as the
 * BLAKE2b key. When no machine identifier is readable, a random key file under
 * sdir is read or created. Returns 0 on success. */
static int sodium_derive_key(unsigned char *key, const char *sdir)
{
    PEKO_KC_LOG("derive_key: enter sdir=%s", sdir);
    static const char salt[] = "peko.storage.keychain.v1";
    const char *id_paths[] = {
        "/etc/machine-id",
        "/var/lib/dbus/machine-id",
        NULL
    };

    char mid[256];
    size_t mid_len = 0;
    for (int i = 0; id_paths[i]; i++) {
        FILE *f = fopen(id_paths[i], "rb");
        if (f) {
            mid_len = fread(mid, 1, sizeof(mid), f);
            fclose(f);
            if (mid_len > 0) {
                break;
            }
        }
    }

    if (mid_len > 0) {
        while (mid_len > 0 && (mid[mid_len - 1] == '\n' || mid[mid_len - 1] == '\r')) {
            mid_len -= 1;
        }
        PEKO_KC_LOG("derive_key: using machine id, len=%zu", mid_len);
        crypto_generichash(key, crypto_secretbox_KEYBYTES,
                           (const unsigned char *)mid, mid_len,
                           (const unsigned char *)salt, sizeof(salt) - 1);
        PEKO_KC_LOG("derive_key: machine id hash done");
        return 0;
    }

    PEKO_KC_LOG("derive_key: no machine id, using key file");
    size_t kn = strlen(sdir) + strlen("/.master") + 1;
    char *kp = (char *)malloc(kn);
    if (!kp) {
        return -1;
    }
    snprintf(kp, kn, "%s/.master", sdir);

    FILE *f = fopen(kp, "rb");
    if (f) {
        PEKO_KC_LOG("derive_key: reading existing key file %s", kp);
        size_t got = fread(key, 1, crypto_secretbox_KEYBYTES, f);
        fclose(f);
        free(kp);
        PEKO_KC_LOG("derive_key: key file read got=%zu", got);
        return (got == crypto_secretbox_KEYBYTES) ? 0 : -1;
    }

    PEKO_KC_LOG("derive_key: generating random key, calling randombytes_buf");
    randombytes_buf(key, crypto_secretbox_KEYBYTES);
    PEKO_KC_LOG("derive_key: randombytes_buf returned");
    f = fopen(kp, "wb");
    if (!f) {
        PEKO_KC_LOG("derive_key: cannot open key file for write %s errno=%d", kp, errno);
        free(kp);
        return -1;
    }
    chmod(kp, 0600);
    size_t wrote = fwrite(key, 1, crypto_secretbox_KEYBYTES, f);
    fclose(f);
    free(kp);
    PEKO_KC_LOG("derive_key: key file wrote=%zu", wrote);
    return (wrote == crypto_secretbox_KEYBYTES) ? 0 : -1;
}

/* Builds the secret file path for name under sdir. The file name is the hex of
 * a hash of name. Returns a malloc'd path or NULL on failure. */
static char *sodium_secret_path(const char *sdir, const char *name)
{
    unsigned char h[32];
    crypto_generichash(h, sizeof(h),
                       (const unsigned char *)name, strlen(name), NULL, 0);
    char hex[2 * sizeof(h) + 1];
    sodium_bin2hex(hex, sizeof(hex), h, sizeof(h));

    size_t n = strlen(sdir) + 1 + strlen(hex) + strlen(".bin") + 1;
    char *path = (char *)malloc(n);
    if (path) {
        snprintf(path, n, "%s/%s.bin", sdir, hex);
    }
    return path;
}

/* Stores value under key in an encrypted file. Returns 1 on success. */
static int sodium_kc_set(const char *app_id, const char *key, const char *value)
{
    PEKO_KC_LOG("kc_set: enter key=%s", key ? key : "(null)");
    if (!key || !value || sodium_ready() != 0) {
        PEKO_KC_LOG("kc_set: precondition failed");
        return 0;
    }
    char *k = sodium_dup_cstr(key);
    char *v = sodium_dup_cstr(value);
    if (!k || !v) {
        free(k);
        free(v);
        return 0;
    }
    size_t vlen = strlen(v);

    int ok = 0;
    PEKO_KC_LOG("kc_set: begin_blocking vlen=%zu", vlen);
    pgc_begin_blocking();
    {
        char *sdir = sodium_secrets_dir(app_id);
        if (sdir) {
            unsigned char keybuf[crypto_secretbox_KEYBYTES];
            if (sodium_derive_key(keybuf, sdir) == 0) {
                char *path = sodium_secret_path(sdir, k);
                if (path) {
                    size_t clen = crypto_secretbox_NONCEBYTES
                                + crypto_secretbox_MACBYTES + vlen;
                    unsigned char *out = (unsigned char *)malloc(clen);
                    if (out) {
                        PEKO_KC_LOG("kc_set: nonce randombytes_buf");
                        randombytes_buf(out, crypto_secretbox_NONCEBYTES);
                        PEKO_KC_LOG("kc_set: encrypting");
                        if (crypto_secretbox_easy(out + crypto_secretbox_NONCEBYTES,
                                                  (const unsigned char *)v, vlen,
                                                  out, keybuf) == 0) {
                            PEKO_KC_LOG("kc_set: writing file %s", path);
                            FILE *f = fopen(path, "wb");
                            if (f) {
                                if (fwrite(out, 1, clen, f) == clen) {
                                    ok = 1;
                                }
                                fclose(f);
                            } else {
                                PEKO_KC_LOG("kc_set: fopen write failed errno=%d", errno);
                            }
                        }
                        sodium_memzero(out, clen);
                        free(out);
                    }
                    free(path);
                }
                sodium_memzero(keybuf, sizeof(keybuf));
            } else {
                PEKO_KC_LOG("kc_set: derive_key failed");
            }
            free(sdir);
        } else {
            PEKO_KC_LOG("kc_set: secrets_dir null");
        }
    }
    pgc_end_blocking();
    PEKO_KC_LOG("kc_set: end_blocking ok=%d", ok);

    sodium_memzero(v, vlen);
    free(k);
    free(v);
    return ok;
}

/* Reads the secret stored under key. Returns a GC-managed string, or NULL when
 * the key is absent or the file does not decrypt. */
static const char *sodium_kc_get(const char *app_id, const char *key)
{
    PEKO_KC_LOG("kc_get: enter key=%s", key ? key : "(null)");
    if (!key || sodium_ready() != 0) {
        PEKO_KC_LOG("kc_get: precondition failed");
        return NULL;
    }
    char *k = sodium_dup_cstr(key);
    if (!k) {
        return NULL;
    }

    unsigned char keybuf[crypto_secretbox_KEYBYTES];
    int have_key = 0;
    unsigned char *cipher = NULL;
    size_t clen = 0;

    PEKO_KC_LOG("kc_get: begin_blocking");
    pgc_begin_blocking();
    {
        char *sdir = sodium_secrets_dir(app_id);
        if (sdir) {
            if (sodium_derive_key(keybuf, sdir) == 0) {
                have_key = 1;
                char *path = sodium_secret_path(sdir, k);
                if (path) {
                    FILE *f = fopen(path, "rb");
                    if (f) {
                        fseek(f, 0, SEEK_END);
                        long sz = ftell(f);
                        fseek(f, 0, SEEK_SET);
                        if (sz > 0) {
                            cipher = (unsigned char *)malloc((size_t)sz);
                            if (cipher
                                && fread(cipher, 1, (size_t)sz, f) == (size_t)sz) {
                                clen = (size_t)sz;
                            } else {
                                free(cipher);
                                cipher = NULL;
                            }
                        }
                        fclose(f);
                    } else {
                        PEKO_KC_LOG("kc_get: secret file absent path=%s", path);
                    }
                    free(path);
                }
            } else {
                PEKO_KC_LOG("kc_get: derive_key failed");
            }
            free(sdir);
        } else {
            PEKO_KC_LOG("kc_get: secrets_dir null");
        }
    }
    pgc_end_blocking();
    PEKO_KC_LOG("kc_get: end_blocking have_key=%d clen=%zu", have_key, clen);

    free(k);

    size_t overhead = crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES;
    if (!have_key || !cipher || clen < overhead) {
        sodium_memzero(keybuf, sizeof(keybuf));
        free(cipher);
        PEKO_KC_LOG("kc_get: returning null (no usable cipher)");
        return NULL;
    }

    size_t mlen = clen - overhead;
    unsigned char *plain = (unsigned char *)malloc(mlen + 1);
    if (!plain) {
        sodium_memzero(keybuf, sizeof(keybuf));
        free(cipher);
        return NULL;
    }

    int bad = crypto_secretbox_open_easy(plain,
                                         cipher + crypto_secretbox_NONCEBYTES,
                                         clen - crypto_secretbox_NONCEBYTES,
                                         cipher, keybuf);
    sodium_memzero(keybuf, sizeof(keybuf));
    free(cipher);
    PEKO_KC_LOG("kc_get: decrypt bad=%d", bad);

    if (bad != 0) {
        sodium_memzero(plain, mlen + 1);
        free(plain);
        PEKO_KC_LOG("kc_get: returning null (decrypt failed)");
        return NULL;
    }
    plain[mlen] = '\0';

    char *gc = (char *)pgc_alloc_atomic(mlen + 1);
    if (gc) {
        memcpy(gc, plain, mlen + 1);
    }
    sodium_memzero(plain, mlen + 1);
    free(plain);
    PEKO_KC_LOG("kc_get: returning value len=%zu", mlen);
    return gc;
}

/* Removes the secret file for key. A missing file is a success. Returns 1 on
 * success. */
static int sodium_kc_remove(const char *app_id, const char *key)
{
    PEKO_KC_LOG("kc_remove: enter key=%s", key ? key : "(null)");
    if (!key || sodium_ready() != 0) {
        PEKO_KC_LOG("kc_remove: precondition failed");
        return 0;
    }
    char *k = sodium_dup_cstr(key);
    if (!k) {
        return 0;
    }

    int ok = 0;
    PEKO_KC_LOG("kc_remove: begin_blocking");
    pgc_begin_blocking();
    {
        char *sdir = sodium_secrets_dir(app_id);
        if (sdir) {
            char *path = sodium_secret_path(sdir, k);
            if (path) {
                if (remove(path) == 0 || errno == ENOENT) {
                    ok = 1;
                }
                free(path);
            }
            free(sdir);
        }
    }
    pgc_end_blocking();
    PEKO_KC_LOG("kc_remove: end_blocking ok=%d", ok);

    free(k);
    return ok;
}

#endif /* PEKO_KEYCHAIN_SODIUM_H */
