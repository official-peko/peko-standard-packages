/*
 * peko_crypto.h
 * Wrapper declarations for the Peko crypto library over libsodium.
 * Functions that return binary data write into GC-managed buffers.
 * All lengths are in bytes unless noted otherwise.
 */

#ifndef PEKO_CRYPTO_H
#define PEKO_CRYPTO_H
#define SODIUM_STATIC 1

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "libsodium/sodium.h"

/* -------------------------------------------------------------------------
 * Output size constants exposed to Peko
 * ---------------------------------------------------------------------- */

/* Hashing */
#define PEKO_SHA256_SIZE          crypto_hash_sha256_BYTES
#define PEKO_SHA512_SIZE          crypto_hash_sha512_BYTES
#define PEKO_BLAKE2B_SIZE         crypto_generichash_BYTES
#define PEKO_BLAKE2B_SIZE_MIN     crypto_generichash_BYTES_MIN
#define PEKO_BLAKE2B_SIZE_MAX     crypto_generichash_BYTES_MAX

/* Symmetric encryption - XSalsa20-Poly1305 (secretbox) */
#define PEKO_SECRETBOX_KEY_SIZE   crypto_secretbox_KEYBYTES
#define PEKO_SECRETBOX_NONCE_SIZE crypto_secretbox_NONCEBYTES
#define PEKO_SECRETBOX_MAC_SIZE   crypto_secretbox_MACBYTES

/* Symmetric encryption - ChaCha20-Poly1305 IETF (AEAD) */
#define PEKO_CHACHA_KEY_SIZE      crypto_aead_chacha20poly1305_ietf_KEYBYTES
#define PEKO_CHACHA_NONCE_SIZE    crypto_aead_chacha20poly1305_ietf_NPUBBYTES
#define PEKO_CHACHA_TAG_SIZE      crypto_aead_chacha20poly1305_ietf_ABYTES

/* Symmetric encryption - AES-256-GCM */
#define PEKO_AESGCM_KEY_SIZE      crypto_aead_aes256gcm_KEYBYTES
#define PEKO_AESGCM_NONCE_SIZE    crypto_aead_aes256gcm_NPUBBYTES
#define PEKO_AESGCM_TAG_SIZE      crypto_aead_aes256gcm_ABYTES

/* Asymmetric encryption - X25519 + XSalsa20-Poly1305 (box) */
#define PEKO_BOX_PUBLIC_KEY_SIZE  crypto_box_PUBLICKEYBYTES
#define PEKO_BOX_SECRET_KEY_SIZE  crypto_box_SECRETKEYBYTES
#define PEKO_BOX_NONCE_SIZE       crypto_box_NONCEBYTES
#define PEKO_BOX_MAC_SIZE         crypto_box_MACBYTES

/* Digital signatures - Ed25519 */
#define PEKO_SIGN_PUBLIC_KEY_SIZE crypto_sign_PUBLICKEYBYTES
#define PEKO_SIGN_SECRET_KEY_SIZE crypto_sign_SECRETKEYBYTES
#define PEKO_SIGN_SIZE            crypto_sign_BYTES

/* Password hashing - Argon2id */
#define PEKO_PWHASH_SALT_SIZE     crypto_pwhash_SALTBYTES
#define PEKO_PWHASH_STR_SIZE      crypto_pwhash_STRBYTES

/* MACs - HMAC-SHA256 and HMAC-SHA512 */
#define PEKO_HMAC256_KEY_SIZE     crypto_auth_hmacsha256_KEYBYTES
#define PEKO_HMAC256_SIZE         crypto_auth_hmacsha256_BYTES
#define PEKO_HMAC512_KEY_SIZE     crypto_auth_hmacsha512_KEYBYTES
#define PEKO_HMAC512_SIZE         crypto_auth_hmacsha512_BYTES

/* -------------------------------------------------------------------------
 * Library initialization
 * Called automatically by the Peko runtime at program start.
 * ---------------------------------------------------------------------- */

int peko_crypto_init(void);

/* -------------------------------------------------------------------------
 * Secure random
 * ---------------------------------------------------------------------- */

/* Fills buf with len cryptographically secure random bytes. */
void peko_random_bytes(void *buf, int len);

/* Returns a random 32-bit unsigned integer. */
uint32_t peko_random_u32(void);

/* -------------------------------------------------------------------------
 * Secure utilities
 * ---------------------------------------------------------------------- */

/* Constant-time comparison of two buffers. Returns 0 if equal. */
int peko_memcmp(const void *a, const void *b, int len);

/* Zeroes len bytes at ptr. Not optimized away by the compiler. */
void peko_memzero(void *ptr, int len);

/* -------------------------------------------------------------------------
 * Encoding
 * ---------------------------------------------------------------------- */

/*
 * Encodes src (src_len bytes) as a hex string.
 * Returns a GC-managed null-terminated string, or NULL on failure.
 */
char *peko_bin_to_hex(const unsigned char *src, int src_len);

/*
 * Decodes a hex string into binary.
 * Returns a GC-managed buffer and writes the byte count to out_len.
 * Returns NULL on failure.
 */
unsigned char *peko_hex_to_bin(const char *hex, int *out_len);

/*
 * Encodes src as base64.
 * variant: 0 = standard, 1 = url-safe without padding, 2 = url-safe with padding.
 * Returns a GC-managed null-terminated string, or NULL on failure.
 */
char *peko_base64_encode(const unsigned char *src, int src_len, int variant);

/*
 * Decodes base64 into binary.
 * Returns a GC-managed buffer and writes the byte count to out_len.
 * Returns NULL on failure.
 */
unsigned char *peko_base64_decode(const char *b64, int b64_len,
                                  int variant, int *out_len);

/* -------------------------------------------------------------------------
 * Hashing - one-shot
 * ---------------------------------------------------------------------- */

/* SHA-256: returns 32-byte GC buffer, or NULL on failure. */
unsigned char *peko_sha256(const unsigned char *in, int in_len);

/* SHA-512: returns 64-byte GC buffer, or NULL on failure. */
unsigned char *peko_sha512(const unsigned char *in, int in_len);

/*
 * BLAKE2b: out_len sets the digest size (16 to 64 bytes).
 * key and key_len are optional. Pass NULL and 0 for keyless hashing.
 * Returns a GC buffer of out_len bytes, or NULL on failure.
 */
unsigned char *peko_blake2b(const unsigned char *in, int in_len,
                            const unsigned char *key, int key_len,
                            int out_len);

/* -------------------------------------------------------------------------
 * Hashing - streaming (init / update / final)
 * ---------------------------------------------------------------------- */

/* SHA-256 streaming context. Allocate with peko_sha256_ctx_new(). */
void *peko_sha256_ctx_new(void);
void  peko_sha256_update(void *ctx, const unsigned char *in, int in_len);
/* Returns 32-byte GC buffer, or NULL on failure. Frees ctx. */
unsigned char *peko_sha256_final(void *ctx);

/* SHA-512 streaming context. */
void *peko_sha512_ctx_new(void);
void  peko_sha512_update(void *ctx, const unsigned char *in, int in_len);
/* Returns 64-byte GC buffer, or NULL on failure. Frees ctx. */
unsigned char *peko_sha512_final(void *ctx);

/* BLAKE2b streaming context. out_len, key/key_len same as one-shot. */
void *peko_blake2b_ctx_new(const unsigned char *key, int key_len, int out_len);
void  peko_blake2b_update(void *ctx, const unsigned char *in, int in_len);
/* Returns out_len-byte GC buffer, or NULL on failure. Frees ctx. */
unsigned char *peko_blake2b_final(void *ctx, int out_len);

/* -------------------------------------------------------------------------
 * Symmetric encryption - XSalsa20-Poly1305 (secretbox)
 * The recommended default for secret key authenticated encryption.
 * ---------------------------------------------------------------------- */

/* Generates a random secretbox key into a 32-byte GC buffer. */
unsigned char *peko_secretbox_keygen(void);

/* Generates a random nonce into a 24-byte GC buffer. */
unsigned char *peko_secretbox_nonce(void);

/*
 * Encrypts plaintext with key and nonce.
 * Returns ciphertext (plaintext_len + PEKO_SECRETBOX_MAC_SIZE bytes) in a
 * GC buffer and writes its length to out_len. Returns NULL on failure.
 */
unsigned char *peko_secretbox_encrypt(const unsigned char *plaintext,
                                      int plaintext_len,
                                      const unsigned char *nonce,
                                      const unsigned char *key,
                                      int *out_len);

/*
 * Decrypts ciphertext with key and nonce.
 * Returns plaintext in a GC buffer and writes its length to out_len.
 * Returns NULL if authentication fails or on error.
 */
unsigned char *peko_secretbox_decrypt(const unsigned char *ciphertext,
                                      int ciphertext_len,
                                      const unsigned char *nonce,
                                      const unsigned char *key,
                                      int *out_len);

/* -------------------------------------------------------------------------
 * Symmetric encryption - ChaCha20-Poly1305 IETF (AEAD)
 * Recommended for network protocols. Supports additional data.
 * ---------------------------------------------------------------------- */

/* Generates a random ChaCha20 key into a 32-byte GC buffer. */
unsigned char *peko_chacha_keygen(void);

/* Generates a random ChaCha20 nonce into a 12-byte GC buffer. */
unsigned char *peko_chacha_nonce(void);

/*
 * Encrypts plaintext with optional additional authenticated data (ad).
 * ad and ad_len may be NULL/0 if no additional data is needed.
 * Returns ciphertext + tag in a GC buffer and writes length to out_len.
 */
unsigned char *peko_chacha_encrypt(const unsigned char *plaintext,
                                   int plaintext_len,
                                   const unsigned char *ad, int ad_len,
                                   const unsigned char *nonce,
                                   const unsigned char *key,
                                   int *out_len);

/*
 * Decrypts ciphertext. ad must match what was used during encryption.
 * Returns NULL if authentication fails or on error.
 */
unsigned char *peko_chacha_decrypt(const unsigned char *ciphertext,
                                   int ciphertext_len,
                                   const unsigned char *ad, int ad_len,
                                   const unsigned char *nonce,
                                   const unsigned char *key,
                                   int *out_len);

/* -------------------------------------------------------------------------
 * Symmetric encryption - AES-256-GCM
 * Use this for compatibility with existing systems. Needs AES hardware.
 * peko_aesgcm_available() returns -1 when the CPU has no AES-NI.
 * ---------------------------------------------------------------------- */

/* Returns 1 if AES-256-GCM is available on this CPU, 0 otherwise. */
int peko_aesgcm_available(void);

/* Generates a random AES-GCM key into a 32-byte GC buffer. */
unsigned char *peko_aesgcm_keygen(void);

/* Generates a random AES-GCM nonce into a 12-byte GC buffer. */
unsigned char *peko_aesgcm_nonce(void);

unsigned char *peko_aesgcm_encrypt(const unsigned char *plaintext,
                                   int plaintext_len,
                                   const unsigned char *ad, int ad_len,
                                   const unsigned char *nonce,
                                   const unsigned char *key,
                                   int *out_len);

unsigned char *peko_aesgcm_decrypt(const unsigned char *ciphertext,
                                   int ciphertext_len,
                                   const unsigned char *ad, int ad_len,
                                   const unsigned char *nonce,
                                   const unsigned char *key,
                                   int *out_len);

/* -------------------------------------------------------------------------
 * Asymmetric encryption - X25519 + XSalsa20-Poly1305 (box)
 * Encrypts between two parties using their public/secret key pairs.
 * ---------------------------------------------------------------------- */

/*
 * Generates an X25519 key pair.
 * public_key_out and secret_key_out are GC-allocated on success.
 * Returns 0 on success, -1 on failure.
 */
int peko_box_keypair(unsigned char **public_key_out,
                     unsigned char **secret_key_out);

/* Generates a random box nonce into a 24-byte GC buffer. */
unsigned char *peko_box_nonce(void);

/*
 * Encrypts a message from sender to recipient.
 * recipient_pk: recipient public key
 * sender_sk:    sender secret key
 */
unsigned char *peko_box_encrypt(const unsigned char *plaintext,
                                int plaintext_len,
                                const unsigned char *nonce,
                                const unsigned char *recipient_pk,
                                const unsigned char *sender_sk,
                                int *out_len);

/*
 * Decrypts a message from sender.
 * sender_pk:    sender public key
 * recipient_sk: recipient secret key
 * Returns NULL if authentication fails or on error.
 */
unsigned char *peko_box_decrypt(const unsigned char *ciphertext,
                                int ciphertext_len,
                                const unsigned char *nonce,
                                const unsigned char *sender_pk,
                                const unsigned char *recipient_sk,
                                int *out_len);

/* -------------------------------------------------------------------------
 * Digital signatures - Ed25519
 * ---------------------------------------------------------------------- */

/*
 * Generates an Ed25519 key pair.
 * Returns 0 on success, -1 on failure.
 */
int peko_sign_keypair(unsigned char **public_key_out,
                      unsigned char **secret_key_out);

/*
 * Signs message with secret_key.
 * Returns a GC buffer containing the signature (64 bytes) and writes
 * its length to out_len. Returns NULL on failure.
 */
unsigned char *peko_sign(const unsigned char *message, int message_len,
                         const unsigned char *secret_key, int *out_len);

/*
 * Verifies a signature against a message using public_key.
 * Returns 1 if valid, 0 if invalid.
 */
int peko_sign_verify(const unsigned char *signature, int signature_len,
                     const unsigned char *message, int message_len,
                     const unsigned char *public_key);

/* -------------------------------------------------------------------------
 * Password hashing - Argon2id
 * ---------------------------------------------------------------------- */

/*
 * Generates a random Argon2id salt into a 16-byte GC buffer.
 * Generate a fresh salt for each password. Store it next to the
 * derived key or hash string.
 */
unsigned char *peko_pwhash_salt(void);

/*
 * Derives a key of key_len bytes from password using Argon2id.
 * salt must be PEKO_PWHASH_SALT_SIZE bytes.
 * ops_limit: computation cost. Use crypto_pwhash_OPSLIMIT_INTERACTIVE for
 *            login and crypto_pwhash_OPSLIMIT_SENSITIVE for key storage.
 * mem_limit: memory cost in bytes. Use crypto_pwhash_MEMLIMIT_INTERACTIVE
 *            or crypto_pwhash_MEMLIMIT_SENSITIVE.
 * Returns a GC buffer of key_len bytes, or NULL on failure.
 */
unsigned char *peko_pwhash(const char *password, int password_len,
                           const unsigned char *salt,
                           int key_len,
                           unsigned long long ops_limit,
                           size_t mem_limit);

/*
 * Hashes a password into a self-contained string that includes the
 * salt and parameters. Use peko_pwhash_verify to check it later.
 * Returns a GC-managed null-terminated string, or NULL on failure.
 */
char *peko_pwhash_str(const char *password, int password_len,
                      unsigned long long ops_limit, size_t mem_limit);

/*
 * Verifies a password against a hash string from peko_pwhash_str.
 * Returns 1 if the password matches, 0 otherwise.
 */
int peko_pwhash_verify(const char *hash_str, const char *password,
                       int password_len);

/* -------------------------------------------------------------------------
 * MACs - HMAC-SHA256 and HMAC-SHA512
 * ---------------------------------------------------------------------- */

/* Generates a random HMAC-SHA256 key into a GC buffer. */
unsigned char *peko_hmac256_keygen(void);

/*
 * Computes HMAC-SHA256 of message using key.
 * Returns a 32-byte GC buffer, or NULL on failure.
 */
unsigned char *peko_hmac256(const unsigned char *message, int message_len,
                            const unsigned char *key);

/*
 * Verifies an HMAC-SHA256 tag in constant time.
 * Returns 1 if valid, 0 otherwise.
 */
int peko_hmac256_verify(const unsigned char *tag,
                        const unsigned char *message, int message_len,
                        const unsigned char *key);

/* Generates a random HMAC-SHA512 key into a GC buffer. */
unsigned char *peko_hmac512_keygen(void);

/*
 * Computes HMAC-SHA512 of message using key.
 * Returns a 64-byte GC buffer, or NULL on failure.
 */
unsigned char *peko_hmac512(const unsigned char *message, int message_len,
                            const unsigned char *key);

/*
 * Verifies an HMAC-SHA512 tag in constant time.
 * Returns 1 if valid, 0 otherwise.
 */
int peko_hmac512_verify(const unsigned char *tag,
                        const unsigned char *message, int message_len,
                        const unsigned char *key);

/* -------------------------------------------------------------------------
 * Legacy algorithms (MD5 and SHA-1). These are cryptographically broken
 * and provided only for compatibility. Implemented in peko_crypto_legacy.c.
 * ---------------------------------------------------------------------- */

void *md5_context_allocate(void);
void  md5_init_binded(void *ctx);
void  md5_update_binded(void *ctx, const void *data, int len);
void  md5_final_binded(void *ctx, void *hash);

void *sha1_context_allocate(void);
void  sha1_init_binded(void *ctx);
void  sha1_update_binded(void *ctx, const void *data, int len);
void  sha1_final_binded(void *ctx, void *hash);

#endif /* PEKO_CRYPTO_H */
