/*
 * peko_crypto.c
 * Wrapper implementation over libsodium for the Peko crypto library.
 * Pure C99, cross-platform.
 *
 * Compile with -lsodium.
 * On Windows, define SODIUM_STATIC before including sodium.h.
 */

#include "peko_crypto.h"
#include <stdlib.h>
#include <string.h>

extern void *pgc_alloc_atomic(size_t size);

/* =========================================================================
 * GC safety rule for this file
 *
 * pgc_alloc_atomic can trigger a collection. A collection moves every live
 * GC managed object. The input buffers reach these functions as GC managed
 * pointers (Pointer<void> on the Peko side), and the output buffers come from
 * pgc_alloc_atomic, so they are GC managed too. A raw C pointer to any GC
 * managed buffer becomes stale the moment a collection moves that buffer.
 *
 * Therefore no GC managed pointer, input or output, may be held across a call
 * to pgc_alloc_atomic. The pattern used throughout is:
 *
 *   1. Stage every GC managed input into malloc memory (stable address).
 *   2. Run the libsodium operation entirely on malloc and stack memory.
 *   3. Allocate the GC output with pgc_alloc_atomic and copy the result in.
 *   4. Free the staging memory before returning.
 *
 * Step 3 is the only GC allocation, and at that point nothing else GC managed
 * is held live in a raw pointer, so a collection during it is safe.
 *
 * Functions that take only cstr inputs (stable, not GC managed) and functions
 * that read their GC input into a stack buffer before any allocation are
 * already safe and are left in that form.
 * ====================================================================== */

/*
 * Allocates a GC buffer of size bytes, copies src into it, and returns it.
 * Returns NULL if allocation fails. src must point at stable memory (malloc or
 * stack), never at a GC managed buffer, because this function allocates.
 */
static unsigned char *gc_dup(const unsigned char *src, int size)
{
    unsigned char *buf = (unsigned char *)pgc_alloc_atomic((size_t)(size));
    if (!buf)
        return NULL;
    memcpy(buf, src, (size_t)size);
    return buf;
}

/*
 * Copies len bytes from a GC managed source into a freshly malloc'd buffer so
 * the data has a stable address for the duration of a libsodium call. Returns
 * NULL on allocation failure or when len is zero (callers pass NULL through to
 * libsodium for the zero length case). The caller frees the result.
 */
static unsigned char *stage_in(const unsigned char *src, int len)
{
    if (len <= 0 || src == NULL)
        return NULL;
    unsigned char *buf = (unsigned char *)malloc((size_t)len);
    if (!buf)
        return NULL;
    memcpy(buf, src, (size_t)len);
    return buf;
}

/* =========================================================================
 * Library initialization
 * ====================================================================== */

int peko_crypto_init(void)
{
    return sodium_init() < 0 ? -1 : 0;
}

/* =========================================================================
 * Secure random
 * ====================================================================== */

void peko_random_bytes(void *buf, int len)
{
    randombytes_buf(buf, (size_t)len);
}

uint32_t peko_random_u32(void)
{
    return randombytes_random();
}

/* =========================================================================
 * Secure utilities
 * ====================================================================== */

int peko_memcmp(const void *a, const void *b, int len)
{
    return sodium_memcmp(a, b, (size_t)len);
}

void peko_memzero(void *ptr, int len)
{
    sodium_memzero(ptr, (size_t)len);
}

/* =========================================================================
 * Encoding
 * ====================================================================== */

char *peko_bin_to_hex(const unsigned char *src, int src_len)
{
    int out_len = src_len * 2 + 1;

    /* Stage the GC managed input so it has a stable address across the GC
     * output allocation below. */
    unsigned char *src_copy = stage_in(src, src_len);
    if (src_len > 0 && !src_copy)
        return NULL;

    char *out = (char *)pgc_alloc_atomic((size_t)(out_len));
    if (!out) {
        free(src_copy);
        return NULL;
    }

    sodium_bin2hex(out, (size_t)out_len,
                   src_len > 0 ? src_copy : (const unsigned char *)"",
                   (size_t)src_len);
    free(src_copy);
    return out;
}

unsigned char *peko_hex_to_bin(const char *hex, int *out_len)
{
    size_t         hex_len  = strlen(hex);
    size_t         bin_len  = hex_len / 2;
    unsigned char *out      = (unsigned char *)pgc_alloc_atomic(bin_len);
    size_t         decoded  = 0;

    if (!out)
        return NULL;

    if (sodium_hex2bin(out, bin_len, hex, hex_len,
                       NULL, &decoded, NULL) != 0) {
        /* freed by pgc */
        return NULL;
    }

    *out_len = (int)decoded;
    return out;
}

char *peko_base64_encode(const unsigned char *src, int src_len, int variant)
{
    int   sodium_variant;
    switch (variant) {
        case 1:  sodium_variant = sodium_base64_VARIANT_URLSAFE;             break;
        case 2:  sodium_variant = sodium_base64_VARIANT_URLSAFE_NO_PADDING;  break;
        default: sodium_variant = sodium_base64_VARIANT_ORIGINAL;            break;
    }

    /* Stage the GC managed input so it survives the GC output allocation. */
    unsigned char *src_copy = stage_in(src, src_len);
    if (src_len > 0 && !src_copy)
        return NULL;

    size_t out_len = sodium_base64_encoded_len((size_t)src_len, sodium_variant);
    char  *out     = (char *)pgc_alloc_atomic(out_len);
    if (!out) {
        free(src_copy);
        return NULL;
    }

    sodium_bin2base64(out, out_len,
                      src_len > 0 ? src_copy : (const unsigned char *)"",
                      (size_t)src_len, sodium_variant);
    free(src_copy);
    return out;
}

unsigned char *peko_base64_decode(const char *b64, int b64_len,
                                  int variant, int *out_len)
{
    int   sodium_variant;
    switch (variant) {
        case 1:  sodium_variant = sodium_base64_VARIANT_URLSAFE;             break;
        case 2:  sodium_variant = sodium_base64_VARIANT_URLSAFE_NO_PADDING;  break;
        default: sodium_variant = sodium_base64_VARIANT_ORIGINAL;            break;
    }

    size_t         max_bin  = (size_t)b64_len;
    unsigned char *out      = (unsigned char *)pgc_alloc_atomic(max_bin);
    size_t         decoded  = 0;

    if (!out)
        return NULL;

    if (sodium_base642bin(out, max_bin, b64, (size_t)b64_len,
                          NULL, &decoded, NULL, sodium_variant) != 0) {
        /* freed by pgc */
        return NULL;
    }

    *out_len = (int)decoded;
    return out;
}

/* =========================================================================
 * Hashing - one-shot
 * ====================================================================== */

unsigned char *peko_sha256(const unsigned char *in, int in_len)
{
    unsigned char tmp[crypto_hash_sha256_BYTES];
    if (crypto_hash_sha256(tmp, in, (unsigned long long)in_len) != 0)
        return NULL;
    return gc_dup(tmp, crypto_hash_sha256_BYTES);
}

unsigned char *peko_sha512(const unsigned char *in, int in_len)
{
    unsigned char tmp[crypto_hash_sha512_BYTES];
    if (crypto_hash_sha512(tmp, in, (unsigned long long)in_len) != 0)
        return NULL;
    return gc_dup(tmp, crypto_hash_sha512_BYTES);
}

unsigned char *peko_blake2b(const unsigned char *in, int in_len,
                            const unsigned char *key, int key_len,
                            int out_len)
{
    if (out_len < (int)crypto_generichash_BYTES_MIN ||
        out_len > (int)crypto_generichash_BYTES_MAX)
        return NULL;

    /* Stage the GC managed input and key so they survive the GC output
     * allocation. */
    unsigned char *in_copy  = stage_in(in, in_len);
    unsigned char *key_copy = stage_in(key, key_len);
    if ((in_len > 0 && !in_copy) || (key_len > 0 && !key_copy)) {
        free(in_copy);
        free(key_copy);
        return NULL;
    }

    unsigned char *out = (unsigned char *)pgc_alloc_atomic((size_t)(out_len));
    if (!out) {
        free(in_copy);
        free(key_copy);
        return NULL;
    }

    int rc = crypto_generichash(out, (size_t)out_len,
                                in_len > 0 ? in_copy : (const unsigned char *)"",
                                (unsigned long long)in_len,
                                key_len > 0 ? key_copy : NULL,
                                (size_t)(key_len > 0 ? key_len : 0));
    free(in_copy);
    free(key_copy);
    if (rc != 0) {
        /* freed by pgc */
        return NULL;
    }

    return out;
}

/* =========================================================================
 * Hashing - streaming
 * ====================================================================== */

void *peko_sha256_ctx_new(void)
{
    crypto_hash_sha256_state *ctx =
        (crypto_hash_sha256_state *)malloc(sizeof(crypto_hash_sha256_state));
    if (!ctx)
        return NULL;
    if (crypto_hash_sha256_init(ctx) != 0) {
        free(ctx);
        return NULL;
    }
    return ctx;
}

void peko_sha256_update(void *ctx, const unsigned char *in, int in_len)
{
    if (!ctx || !in || in_len <= 0)
        return;
    crypto_hash_sha256_update((crypto_hash_sha256_state *)ctx,
                              in, (unsigned long long)in_len);
}

unsigned char *peko_sha256_final(void *ctx)
{
    if (!ctx)
        return NULL;
    unsigned char tmp[crypto_hash_sha256_BYTES];
    int rc = crypto_hash_sha256_final((crypto_hash_sha256_state *)ctx, tmp);
    free(ctx);
    if (rc != 0)
        return NULL;
    return gc_dup(tmp, crypto_hash_sha256_BYTES);
}

void *peko_sha512_ctx_new(void)
{
    crypto_hash_sha512_state *ctx =
        (crypto_hash_sha512_state *)malloc(sizeof(crypto_hash_sha512_state));
    if (!ctx)
        return NULL;
    if (crypto_hash_sha512_init(ctx) != 0) {
        free(ctx);
        return NULL;
    }
    return ctx;
}

void peko_sha512_update(void *ctx, const unsigned char *in, int in_len)
{
    if (!ctx || !in || in_len <= 0)
        return;
    crypto_hash_sha512_update((crypto_hash_sha512_state *)ctx,
                              in, (unsigned long long)in_len);
}

unsigned char *peko_sha512_final(void *ctx)
{
    if (!ctx)
        return NULL;
    unsigned char tmp[crypto_hash_sha512_BYTES];
    int rc = crypto_hash_sha512_final((crypto_hash_sha512_state *)ctx, tmp);
    free(ctx);
    if (rc != 0)
        return NULL;
    return gc_dup(tmp, crypto_hash_sha512_BYTES);
}

typedef struct {
    crypto_generichash_state state;
    int                      out_len;
} blake2b_ctx_t;

void *peko_blake2b_ctx_new(const unsigned char *key, int key_len, int out_len)
{
    if (out_len < (int)crypto_generichash_BYTES_MIN ||
        out_len > (int)crypto_generichash_BYTES_MAX)
        return NULL;

    blake2b_ctx_t *ctx = (blake2b_ctx_t *)malloc(sizeof(blake2b_ctx_t));
    if (!ctx)
        return NULL;

    ctx->out_len = out_len;

    if (crypto_generichash_init(&ctx->state,
                                key_len > 0 ? key : NULL,
                                (size_t)(key_len > 0 ? key_len : 0),
                                (size_t)out_len) != 0) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

void peko_blake2b_update(void *ctx, const unsigned char *in, int in_len)
{
    if (!ctx || !in || in_len <= 0)
        return;
    blake2b_ctx_t *c = (blake2b_ctx_t *)ctx;
    crypto_generichash_update(&c->state, in, (unsigned long long)in_len);
}

unsigned char *peko_blake2b_final(void *ctx, int out_len)
{
    if (!ctx)
        return NULL;
    blake2b_ctx_t *c   = (blake2b_ctx_t *)ctx;
    unsigned char *out = (unsigned char *)pgc_alloc_atomic((size_t)(out_len));
    int            rc  = -1;
    if (out)
        rc = crypto_generichash_final(&c->state, out, (size_t)out_len);
    free(ctx);
    if (rc != 0) {
        /* freed by pgc */
        return NULL;
    }
    return out;
}

/* =========================================================================
 * Symmetric - XSalsa20-Poly1305 (secretbox)
 * ====================================================================== */

unsigned char *peko_secretbox_keygen(void)
{
    unsigned char *key = (unsigned char *)pgc_alloc_atomic((size_t)(crypto_secretbox_KEYBYTES));
    if (!key)
        return NULL;
    crypto_secretbox_keygen(key);
    return key;
}

unsigned char *peko_secretbox_nonce(void)
{
    unsigned char *n = (unsigned char *)pgc_alloc_atomic((size_t)(crypto_secretbox_NONCEBYTES));
    if (!n)
        return NULL;
    randombytes_buf(n, crypto_secretbox_NONCEBYTES);
    return n;
}

unsigned char *peko_secretbox_encrypt(const unsigned char *plaintext,
                                      int plaintext_len,
                                      const unsigned char *nonce,
                                      const unsigned char *key,
                                      int *out_len)
{
    int ct_len = plaintext_len + (int)crypto_secretbox_MACBYTES;

    /* Stage the GC managed inputs so they survive the GC output allocation. */
    unsigned char *pt_copy    = stage_in(plaintext, plaintext_len);
    unsigned char *nonce_copy = stage_in(nonce, (int)crypto_secretbox_NONCEBYTES);
    unsigned char *key_copy   = stage_in(key, (int)crypto_secretbox_KEYBYTES);
    if ((plaintext_len > 0 && !pt_copy) || !nonce_copy || !key_copy) {
        free(pt_copy); free(nonce_copy); free(key_copy);
        return NULL;
    }

    unsigned char *ct = (unsigned char *)pgc_alloc_atomic((size_t)(ct_len));
    if (!ct) {
        free(pt_copy); free(nonce_copy); free(key_copy);
        return NULL;
    }

    int rc = crypto_secretbox_easy(
        ct,
        plaintext_len > 0 ? pt_copy : (const unsigned char *)"",
        (unsigned long long)plaintext_len,
        nonce_copy, key_copy);
    free(pt_copy); free(nonce_copy); free(key_copy);
    if (rc != 0) {
        /* freed by pgc */
        return NULL;
    }

    *out_len = ct_len;
    return ct;
}

unsigned char *peko_secretbox_decrypt(const unsigned char *ciphertext,
                                      int ciphertext_len,
                                      const unsigned char *nonce,
                                      const unsigned char *key,
                                      int *out_len)
{
    int pt_len = ciphertext_len - (int)crypto_secretbox_MACBYTES;
    if (pt_len < 0)
        return NULL;

    /* Stage the GC managed inputs so they survive the GC output allocation. */
    unsigned char *ct_copy    = stage_in(ciphertext, ciphertext_len);
    unsigned char *nonce_copy = stage_in(nonce, (int)crypto_secretbox_NONCEBYTES);
    unsigned char *key_copy   = stage_in(key, (int)crypto_secretbox_KEYBYTES);
    if ((ciphertext_len > 0 && !ct_copy) || !nonce_copy || !key_copy) {
        free(ct_copy); free(nonce_copy); free(key_copy);
        return NULL;
    }

    unsigned char *pt = (unsigned char *)pgc_alloc_atomic((size_t)(pt_len + 1));
    if (!pt) {
        free(ct_copy); free(nonce_copy); free(key_copy);
        return NULL;
    }

    int rc = crypto_secretbox_open_easy(
        pt,
        ciphertext_len > 0 ? ct_copy : (const unsigned char *)"",
        (unsigned long long)ciphertext_len,
        nonce_copy, key_copy);
    free(ct_copy); free(nonce_copy); free(key_copy);
    if (rc != 0) {
        sodium_memzero(pt, (size_t)pt_len);
        /* freed by pgc */
        return NULL;
    }

    pt[pt_len] = '\0';
    *out_len   = pt_len;
    return pt;
}

/* =========================================================================
 * Symmetric - ChaCha20-Poly1305 IETF (AEAD)
 * ====================================================================== */

unsigned char *peko_chacha_keygen(void)
{
    unsigned char *key =
        (unsigned char *)pgc_alloc_atomic((size_t)(crypto_aead_chacha20poly1305_ietf_KEYBYTES));
    if (!key)
        return NULL;
    crypto_aead_chacha20poly1305_ietf_keygen(key);
    return key;
}

unsigned char *peko_chacha_nonce(void)
{
    unsigned char *n =
        (unsigned char *)pgc_alloc_atomic((size_t)(crypto_aead_chacha20poly1305_ietf_NPUBBYTES));
    if (!n)
        return NULL;
    randombytes_buf(n, crypto_aead_chacha20poly1305_ietf_NPUBBYTES);
    return n;
}

unsigned char *peko_chacha_encrypt(const unsigned char *plaintext,
                                   int plaintext_len,
                                   const unsigned char *ad, int ad_len,
                                   const unsigned char *nonce,
                                   const unsigned char *key,
                                   int *out_len)
{
    int ct_len = plaintext_len +
                 (int)crypto_aead_chacha20poly1305_ietf_ABYTES;
    unsigned long long actual_len = 0;

    /* Stage the GC managed inputs so they survive the GC output allocation. */
    unsigned char *pt_copy    = stage_in(plaintext, plaintext_len);
    unsigned char *ad_copy    = stage_in(ad, ad_len);
    unsigned char *nonce_copy = stage_in(nonce, (int)crypto_aead_chacha20poly1305_ietf_NPUBBYTES);
    unsigned char *key_copy   = stage_in(key, (int)crypto_aead_chacha20poly1305_ietf_KEYBYTES);
    if ((plaintext_len > 0 && !pt_copy) || (ad_len > 0 && !ad_copy) ||
        !nonce_copy || !key_copy) {
        free(pt_copy); free(ad_copy); free(nonce_copy); free(key_copy);
        return NULL;
    }

    unsigned char *ct = (unsigned char *)pgc_alloc_atomic((size_t)(ct_len));
    if (!ct) {
        free(pt_copy); free(ad_copy); free(nonce_copy); free(key_copy);
        return NULL;
    }

    int rc = crypto_aead_chacha20poly1305_ietf_encrypt(
        ct, &actual_len,
        plaintext_len > 0 ? pt_copy : (const unsigned char *)"",
        (unsigned long long)plaintext_len,
        ad_len > 0 ? ad_copy : NULL, (unsigned long long)(ad_len > 0 ? ad_len : 0),
        NULL, nonce_copy, key_copy);
    free(pt_copy); free(ad_copy); free(nonce_copy); free(key_copy);
    if (rc != 0) {
        /* freed by pgc */
        return NULL;
    }

    *out_len = (int)actual_len;
    return ct;
}

unsigned char *peko_chacha_decrypt(const unsigned char *ciphertext,
                                   int ciphertext_len,
                                   const unsigned char *ad, int ad_len,
                                   const unsigned char *nonce,
                                   const unsigned char *key,
                                   int *out_len)
{
    int pt_len = ciphertext_len -
                 (int)crypto_aead_chacha20poly1305_ietf_ABYTES;
    unsigned long long actual = 0;

    if (pt_len < 0)
        return NULL;

    /* Stage the GC managed inputs so they survive the GC output allocation. */
    unsigned char *ct_copy    = stage_in(ciphertext, ciphertext_len);
    unsigned char *ad_copy    = stage_in(ad, ad_len);
    unsigned char *nonce_copy = stage_in(nonce, (int)crypto_aead_chacha20poly1305_ietf_NPUBBYTES);
    unsigned char *key_copy   = stage_in(key, (int)crypto_aead_chacha20poly1305_ietf_KEYBYTES);
    if ((ciphertext_len > 0 && !ct_copy) || (ad_len > 0 && !ad_copy) ||
        !nonce_copy || !key_copy) {
        free(ct_copy); free(ad_copy); free(nonce_copy); free(key_copy);
        return NULL;
    }

    unsigned char *pt = (unsigned char *)pgc_alloc_atomic((size_t)(pt_len + 1));
    if (!pt) {
        free(ct_copy); free(ad_copy); free(nonce_copy); free(key_copy);
        return NULL;
    }

    int rc = crypto_aead_chacha20poly1305_ietf_decrypt(
        pt, &actual, NULL,
        ciphertext_len > 0 ? ct_copy : (const unsigned char *)"",
        (unsigned long long)ciphertext_len,
        ad_len > 0 ? ad_copy : NULL, (unsigned long long)(ad_len > 0 ? ad_len : 0),
        nonce_copy, key_copy);
    free(ct_copy); free(ad_copy); free(nonce_copy); free(key_copy);
    if (rc != 0) {
        sodium_memzero(pt, (size_t)pt_len);
        /* freed by pgc */
        return NULL;
    }

    pt[actual] = '\0';
    *out_len   = (int)actual;
    return pt;
}

/* =========================================================================
 * Symmetric - AES-256-GCM
 * ====================================================================== */

int peko_aesgcm_available(void)
{
    return crypto_aead_aes256gcm_is_available();
}

unsigned char *peko_aesgcm_keygen(void)
{
    unsigned char *key =
        (unsigned char *)pgc_alloc_atomic((size_t)(crypto_aead_aes256gcm_KEYBYTES));
    if (!key)
        return NULL;
    crypto_aead_aes256gcm_keygen(key);
    return key;
}

unsigned char *peko_aesgcm_nonce(void)
{
    unsigned char *n =
        (unsigned char *)pgc_alloc_atomic((size_t)(crypto_aead_aes256gcm_NPUBBYTES));
    if (!n)
        return NULL;
    randombytes_buf(n, crypto_aead_aes256gcm_NPUBBYTES);
    return n;
}

unsigned char *peko_aesgcm_encrypt(const unsigned char *plaintext,
                                   int plaintext_len,
                                   const unsigned char *ad, int ad_len,
                                   const unsigned char *nonce,
                                   const unsigned char *key,
                                   int *out_len)
{
    int ct_len = plaintext_len + (int)crypto_aead_aes256gcm_ABYTES;
    unsigned long long actual = 0;

    /* Stage the GC managed inputs so they survive the GC output allocation. */
    unsigned char *pt_copy    = stage_in(plaintext, plaintext_len);
    unsigned char *ad_copy    = stage_in(ad, ad_len);
    unsigned char *nonce_copy = stage_in(nonce, (int)crypto_aead_aes256gcm_NPUBBYTES);
    unsigned char *key_copy   = stage_in(key, (int)crypto_aead_aes256gcm_KEYBYTES);
    if ((plaintext_len > 0 && !pt_copy) || (ad_len > 0 && !ad_copy) ||
        !nonce_copy || !key_copy) {
        free(pt_copy); free(ad_copy); free(nonce_copy); free(key_copy);
        return NULL;
    }

    unsigned char *ct = (unsigned char *)pgc_alloc_atomic((size_t)(ct_len));
    if (!ct) {
        free(pt_copy); free(ad_copy); free(nonce_copy); free(key_copy);
        return NULL;
    }

    int rc = crypto_aead_aes256gcm_encrypt(
        ct, &actual,
        plaintext_len > 0 ? pt_copy : (const unsigned char *)"",
        (unsigned long long)plaintext_len,
        ad_len > 0 ? ad_copy : NULL, (unsigned long long)(ad_len > 0 ? ad_len : 0),
        NULL, nonce_copy, key_copy);
    free(pt_copy); free(ad_copy); free(nonce_copy); free(key_copy);
    if (rc != 0) {
        /* freed by pgc */
        return NULL;
    }

    *out_len = (int)actual;
    return ct;
}

unsigned char *peko_aesgcm_decrypt(const unsigned char *ciphertext,
                                   int ciphertext_len,
                                   const unsigned char *ad, int ad_len,
                                   const unsigned char *nonce,
                                   const unsigned char *key,
                                   int *out_len)
{
    int pt_len = ciphertext_len - (int)crypto_aead_aes256gcm_ABYTES;
    unsigned long long actual = 0;

    if (pt_len < 0)
        return NULL;

    /* Stage the GC managed inputs so they survive the GC output allocation. */
    unsigned char *ct_copy    = stage_in(ciphertext, ciphertext_len);
    unsigned char *ad_copy    = stage_in(ad, ad_len);
    unsigned char *nonce_copy = stage_in(nonce, (int)crypto_aead_aes256gcm_NPUBBYTES);
    unsigned char *key_copy   = stage_in(key, (int)crypto_aead_aes256gcm_KEYBYTES);
    if ((ciphertext_len > 0 && !ct_copy) || (ad_len > 0 && !ad_copy) ||
        !nonce_copy || !key_copy) {
        free(ct_copy); free(ad_copy); free(nonce_copy); free(key_copy);
        return NULL;
    }

    unsigned char *pt = (unsigned char *)pgc_alloc_atomic((size_t)(pt_len + 1));
    if (!pt) {
        free(ct_copy); free(ad_copy); free(nonce_copy); free(key_copy);
        return NULL;
    }

    int rc = crypto_aead_aes256gcm_decrypt(
        pt, &actual, NULL,
        ciphertext_len > 0 ? ct_copy : (const unsigned char *)"",
        (unsigned long long)ciphertext_len,
        ad_len > 0 ? ad_copy : NULL, (unsigned long long)(ad_len > 0 ? ad_len : 0),
        nonce_copy, key_copy);
    free(ct_copy); free(ad_copy); free(nonce_copy); free(key_copy);
    if (rc != 0) {
        sodium_memzero(pt, (size_t)pt_len);
        /* freed by pgc */
        return NULL;
    }

    pt[actual] = '\0';
    *out_len   = (int)actual;
    return pt;
}

/* =========================================================================
 * Asymmetric - X25519 box
 * ====================================================================== */

int peko_box_keypair(unsigned char **public_key_out,
                     unsigned char **secret_key_out)
{
    /* Generate into stack buffers first so no GC allocation happens while a
     * raw pointer to GC memory is held. */
    unsigned char pk_tmp[crypto_box_PUBLICKEYBYTES];
    unsigned char sk_tmp[crypto_box_SECRETKEYBYTES];

    if (crypto_box_keypair(pk_tmp, sk_tmp) != 0)
        return -1;

    /* The out parameter slots are raw (address space 0) references, not GC
     * roots, so a pointer stored into them is not updated if a later
     * allocation moves the buffer. A second pgc_alloc_atomic would be able to
     * move the buffer from a first one while only a raw C pointer to it is
     * held. To avoid that, both keys live in a single GC allocation. One
     * allocation means no later collection point can move it before the
     * pointers are published. */
    size_t total = (size_t)crypto_box_PUBLICKEYBYTES +
                   (size_t)crypto_box_SECRETKEYBYTES;
    unsigned char *both = (unsigned char *)pgc_alloc_atomic(total);
    if (!both)
        return -1;

    memcpy(both, pk_tmp, (size_t)crypto_box_PUBLICKEYBYTES);
    memcpy(both + crypto_box_PUBLICKEYBYTES, sk_tmp,
           (size_t)crypto_box_SECRETKEYBYTES);

    *public_key_out = both;
    *secret_key_out = both + crypto_box_PUBLICKEYBYTES;
    return 0;
}

unsigned char *peko_box_nonce(void)
{
    unsigned char *n = (unsigned char *)pgc_alloc_atomic((size_t)(crypto_box_NONCEBYTES));
    if (!n)
        return NULL;
    randombytes_buf(n, crypto_box_NONCEBYTES);
    return n;
}

unsigned char *peko_box_encrypt(const unsigned char *plaintext,
                                int plaintext_len,
                                const unsigned char *nonce,
                                const unsigned char *recipient_pk,
                                const unsigned char *sender_sk,
                                int *out_len)
{
    int ct_len = plaintext_len + (int)crypto_box_MACBYTES;

    /* Stage the GC managed inputs so they survive the GC output allocation. */
    unsigned char *pt_copy    = stage_in(plaintext, plaintext_len);
    unsigned char *nonce_copy = stage_in(nonce, (int)crypto_box_NONCEBYTES);
    unsigned char *pk_copy    = stage_in(recipient_pk, (int)crypto_box_PUBLICKEYBYTES);
    unsigned char *sk_copy    = stage_in(sender_sk, (int)crypto_box_SECRETKEYBYTES);
    if ((plaintext_len > 0 && !pt_copy) || !nonce_copy || !pk_copy || !sk_copy) {
        free(pt_copy); free(nonce_copy); free(pk_copy); free(sk_copy);
        return NULL;
    }

    unsigned char *ct = (unsigned char *)pgc_alloc_atomic((size_t)(ct_len));
    if (!ct) {
        free(pt_copy); free(nonce_copy); free(pk_copy); free(sk_copy);
        return NULL;
    }

    int rc = crypto_box_easy(
        ct,
        plaintext_len > 0 ? pt_copy : (const unsigned char *)"",
        (unsigned long long)plaintext_len,
        nonce_copy, pk_copy, sk_copy);
    free(pt_copy); free(nonce_copy); free(pk_copy); free(sk_copy);
    if (rc != 0) {
        /* freed by pgc */
        return NULL;
    }

    *out_len = ct_len;
    return ct;
}

unsigned char *peko_box_decrypt(const unsigned char *ciphertext,
                                int ciphertext_len,
                                const unsigned char *nonce,
                                const unsigned char *sender_pk,
                                const unsigned char *recipient_sk,
                                int *out_len)
{
    int pt_len = ciphertext_len - (int)crypto_box_MACBYTES;
    if (pt_len < 0)
        return NULL;

    /* Stage the GC managed inputs so they survive the GC output allocation. */
    unsigned char *ct_copy    = stage_in(ciphertext, ciphertext_len);
    unsigned char *nonce_copy = stage_in(nonce, (int)crypto_box_NONCEBYTES);
    unsigned char *pk_copy    = stage_in(sender_pk, (int)crypto_box_PUBLICKEYBYTES);
    unsigned char *sk_copy    = stage_in(recipient_sk, (int)crypto_box_SECRETKEYBYTES);
    if ((ciphertext_len > 0 && !ct_copy) || !nonce_copy || !pk_copy || !sk_copy) {
        free(ct_copy); free(nonce_copy); free(pk_copy); free(sk_copy);
        return NULL;
    }

    unsigned char *pt = (unsigned char *)pgc_alloc_atomic((size_t)(pt_len + 1));
    if (!pt) {
        free(ct_copy); free(nonce_copy); free(pk_copy); free(sk_copy);
        return NULL;
    }

    int rc = crypto_box_open_easy(
        pt,
        ciphertext_len > 0 ? ct_copy : (const unsigned char *)"",
        (unsigned long long)ciphertext_len,
        nonce_copy, pk_copy, sk_copy);
    free(ct_copy); free(nonce_copy); free(pk_copy); free(sk_copy);
    if (rc != 0) {
        sodium_memzero(pt, (size_t)pt_len);
        /* freed by pgc */
        return NULL;
    }

    pt[pt_len] = '\0';
    *out_len   = pt_len;
    return pt;
}

/* =========================================================================
 * Digital signatures - Ed25519
 * ====================================================================== */

int peko_sign_keypair(unsigned char **public_key_out,
                      unsigned char **secret_key_out)
{
    /* Generate into stack buffers first so no GC allocation happens while a
     * raw pointer to GC memory is held. */
    unsigned char pk_tmp[crypto_sign_PUBLICKEYBYTES];
    unsigned char sk_tmp[crypto_sign_SECRETKEYBYTES];

    if (crypto_sign_keypair(pk_tmp, sk_tmp) != 0)
        return -1;

    /* Both keys live in a single GC allocation. One allocation means no later
     * collection point can move it before the pointers are published. The
     * collector resolves the interior secret-key pointer to its containing
     * object, so relocation updates both published references. */
    size_t total = (size_t)crypto_sign_PUBLICKEYBYTES +
                   (size_t)crypto_sign_SECRETKEYBYTES;
    unsigned char *both = (unsigned char *)pgc_alloc_atomic(total);
    if (!both)
        return -1;

    memcpy(both, pk_tmp, (size_t)crypto_sign_PUBLICKEYBYTES);
    memcpy(both + crypto_sign_PUBLICKEYBYTES, sk_tmp,
           (size_t)crypto_sign_SECRETKEYBYTES);

    *public_key_out = both;
    *secret_key_out = both + crypto_sign_PUBLICKEYBYTES;
    return 0;
}

unsigned char *peko_sign(const unsigned char *message, int message_len,
                         const unsigned char *secret_key, int *out_len)
{
    unsigned long long sig_len = 0;

    /* Stage the GC managed inputs so they survive the GC output allocation. */
    unsigned char *msg_copy = stage_in(message, message_len);
    unsigned char *sk_copy  = stage_in(secret_key, (int)crypto_sign_SECRETKEYBYTES);
    if ((message_len > 0 && !msg_copy) || !sk_copy) {
        free(msg_copy); free(sk_copy);
        return NULL;
    }

    unsigned char *sig = (unsigned char *)pgc_alloc_atomic((size_t)(crypto_sign_BYTES));
    if (!sig) {
        free(msg_copy); free(sk_copy);
        return NULL;
    }

    int rc = crypto_sign_detached(
        sig, &sig_len,
        message_len > 0 ? msg_copy : (const unsigned char *)"",
        (unsigned long long)message_len,
        sk_copy);
    free(msg_copy); free(sk_copy);
    if (rc != 0) {
        /* freed by pgc */
        return NULL;
    }

    *out_len = (int)sig_len;
    return sig;
}

int peko_sign_verify(const unsigned char *signature, int signature_len,
                     const unsigned char *message, int message_len,
                     const unsigned char *public_key)
{
    (void)signature_len;
    return crypto_sign_verify_detached(signature, message,
                                       (unsigned long long)message_len,
                                       public_key) == 0 ? 1 : 0;
}

/* =========================================================================
 * Password hashing - Argon2id
 * ====================================================================== */

unsigned char *peko_pwhash_salt(void)
{
    unsigned char *salt = (unsigned char *)pgc_alloc_atomic((size_t)(crypto_pwhash_SALTBYTES));
    if (!salt)
        return NULL;
    randombytes_buf(salt, crypto_pwhash_SALTBYTES);
    return salt;
}

unsigned char *peko_pwhash(const char *password, int password_len,
                           const unsigned char *salt,
                           int key_len,
                           unsigned long long ops_limit,
                           size_t mem_limit)
{
    /* salt is GC managed; stage it so it survives the GC key allocation.
     * password is a cstr (stable) and needs no staging. */
    unsigned char *salt_copy = stage_in(salt, (int)crypto_pwhash_SALTBYTES);
    if (!salt_copy)
        return NULL;

    unsigned char *key = (unsigned char *)pgc_alloc_atomic((size_t)(key_len));
    if (!key) {
        free(salt_copy);
        return NULL;
    }

    int rc = crypto_pwhash(key, (unsigned long long)key_len,
                           password, (unsigned long long)password_len,
                           salt_copy, ops_limit, mem_limit,
                           crypto_pwhash_ALG_DEFAULT);
    free(salt_copy);
    if (rc != 0) {
        sodium_memzero(key, (size_t)key_len);
        /* freed by pgc */
        return NULL;
    }

    return key;
}

char *peko_pwhash_str(const char *password, int password_len,
                      unsigned long long ops_limit, size_t mem_limit)
{
    char *out = (char *)pgc_alloc_atomic((size_t)(crypto_pwhash_STRBYTES));
    if (!out)
        return NULL;

    if (crypto_pwhash_str(out, password,
                          (unsigned long long)password_len,
                          ops_limit, mem_limit) != 0) {
        /* freed by pgc */
        return NULL;
    }

    return out;
}

int peko_pwhash_verify(const char *hash_str, const char *password,
                       int password_len)
{
    return crypto_pwhash_str_verify(hash_str, password,
                                    (unsigned long long)password_len) == 0
           ? 1 : 0;
}

/* =========================================================================
 * MACs - HMAC-SHA256 and HMAC-SHA512
 * ====================================================================== */

unsigned char *peko_hmac256_keygen(void)
{
    unsigned char *key =
        (unsigned char *)pgc_alloc_atomic((size_t)(crypto_auth_hmacsha256_KEYBYTES));
    if (!key)
        return NULL;
    crypto_auth_hmacsha256_keygen(key);
    return key;
}

unsigned char *peko_hmac256(const unsigned char *message, int message_len,
                            const unsigned char *key)
{
    unsigned char tmp[crypto_auth_hmacsha256_BYTES];
    if (crypto_auth_hmacsha256(tmp, message,
                               (unsigned long long)message_len, key) != 0)
        return NULL;
    return gc_dup(tmp, crypto_auth_hmacsha256_BYTES);
}

int peko_hmac256_verify(const unsigned char *tag,
                        const unsigned char *message, int message_len,
                        const unsigned char *key)
{
    return crypto_auth_hmacsha256_verify(tag, message,
                                         (unsigned long long)message_len,
                                         key) == 0 ? 1 : 0;
}

unsigned char *peko_hmac512_keygen(void)
{
    unsigned char *key =
        (unsigned char *)pgc_alloc_atomic((size_t)(crypto_auth_hmacsha512_KEYBYTES));
    if (!key)
        return NULL;
    crypto_auth_hmacsha512_keygen(key);
    return key;
}

unsigned char *peko_hmac512(const unsigned char *message, int message_len,
                            const unsigned char *key)
{
    unsigned char tmp[crypto_auth_hmacsha512_BYTES];
    if (crypto_auth_hmacsha512(tmp, message,
                               (unsigned long long)message_len, key) != 0)
        return NULL;
    return gc_dup(tmp, crypto_auth_hmacsha512_BYTES);
}

int peko_hmac512_verify(const unsigned char *tag,
                        const unsigned char *message, int message_len,
                        const unsigned char *key)
{
    return crypto_auth_hmacsha512_verify(tag, message,
                                         (unsigned long long)message_len,
                                         key) == 0 ? 1 : 0;
}
