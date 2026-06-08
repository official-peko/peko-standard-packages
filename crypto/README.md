# crypto

Cryptographic operations for Pekoscript, backed by libsodium.

## Overview

The `crypto` package covers secure random generation, hashing, message authentication, authenticated symmetric encryption, public key encryption and signatures, key derivation, password hashing, and encoding helpers. A small set of broken legacy algorithms is included for compatibility only.

Most functions take and return `fs::Buffer`. A buffer holds the raw bytes of keys, nonces, plaintext, ciphertext, and digests. Functions that can fail return an optional buffer or string and yield `None` on failure.

All functions are defined flat at the package top level and are called with the `crypto::` prefix. The category headers below group them for reading. They are not submodules.

## Import

```
import crypto@"v0.1.0";
```

This package is not auto-imported. It depends on `fs` for the `Buffer` type.

## Constants

### Base64Variant

Selects the base64 alphabet.

| Name | Value | Meaning |
|---|---|---|
| `Base64Variant::Standard` | 0 | Standard base64 with padding |
| `Base64Variant::UrlSafe` | 1 | URL-safe base64 without padding |
| `Base64Variant::UrlSafeNoPadding` | 2 | URL-safe base64 with padding |

### HashSize

Digest sizes in bytes.

| Name | Value |
|---|---|
| `HashSize::Sha256` | 32 |
| `HashSize::Sha512` | 64 |
| `HashSize::Blake2b` | 32 |

### OpsLimit and MemLimit

Argon2id cost presets. Higher values are slower and more resistant to brute force.

| OpsLimit | Value | MemLimit | Value |
|---|---|---|---|
| `Interactive` | 2 | `Interactive` | 64 MB |
| `Moderate` | 3 | `Moderate` | 256 MB |
| `Sensitive` | 4 | `Sensitive` | 1 GB |

Use the `Interactive` presets for login forms. Use the `Sensitive` presets for long-term key storage.

## Secure random

```
fn random_bytes(len: int) => fs::Buffer?
fn random_u32() => int
```

`random_bytes` returns a buffer of cryptographically secure random bytes. `random_u32` returns a random unsigned 32-bit integer.

```
key := crypto::random_bytes(32)?;
```

## Secure utilities

```
fn secure_eq(a: fs::Buffer, b: fs::Buffer) => bool
fn secure_zero(buf: fs::Buffer)
```

`secure_eq` compares two buffers in constant time and returns true if they are equal. Use it instead of `==` when comparing secrets, because a normal comparison can leak information through timing. `secure_zero` overwrites a buffer with zeroes. Use it to clear keys and other secrets from memory after use.

## Encoding

```
fn to_hex(buf: fs::Buffer) => string?
fn from_hex(hex: string) => fs::Buffer?
fn to_base64(buf: fs::Buffer, variant: int) => string?
fn to_base64(buf: fs::Buffer) => string?
fn from_base64(b64: string, variant: int) => fs::Buffer?
fn from_base64(b64: string) => fs::Buffer?
```

`to_hex` encodes a buffer as a lowercase hex string. `from_hex` decodes a hex string back to a buffer. `to_base64` and `from_base64` convert between buffers and base64 strings. The single-argument forms use `Base64Variant::Standard`. The decode variant must match the variant used to encode.

```
digest := crypto::sha256(data)?;
hex    := crypto::to_hex(digest)?;
```

## Hashing

```
fn sha256(data: fs::Buffer) => fs::Buffer?
fn sha512(data: fs::Buffer) => fs::Buffer?
fn blake2b(data: fs::Buffer, out_len: int) => fs::Buffer?
fn blake2b(data: fs::Buffer, key: fs::Buffer, out_len: int) => fs::Buffer?
```

`sha256` and `sha512` return digests of 32 and 64 bytes. `blake2b` returns a digest of `out_len` bytes, in the range 16 to 64. The keyed `blake2b` mixes a secret key into the hash, which produces a MAC-like output.

### Streaming hashers

For data that arrives in chunks or is too large to hold at once, three streaming hasher classes are provided: `Sha256Hasher`, `Sha512Hasher`, and `Blake2bHasher`. Each has an `update(data: fs::Buffer)` method and a `finalize() => fs::Buffer?` method. The hasher must not be used after `finalize`.

`Blake2bHasher` has two constructors: `Blake2bHasher(out_len: int)` for a keyless hash, and `Blake2bHasher(key: fs::Buffer, out_len: int)` for a keyed hash.

```
hasher := crypto::Sha256Hasher();
hasher.update(chunk1);
hasher.update(chunk2);
digest := hasher.finalize()?;
```

## HMAC

```
fn hmac256_keygen() => fs::Buffer?
fn hmac256(message: fs::Buffer, key: fs::Buffer) => fs::Buffer?
fn hmac256_verify(tag: fs::Buffer, message: fs::Buffer, key: fs::Buffer) => bool
fn hmac512_keygen() => fs::Buffer?
fn hmac512(message: fs::Buffer, key: fs::Buffer) => fs::Buffer?
fn hmac512_verify(tag: fs::Buffer, message: fs::Buffer, key: fs::Buffer) => bool
```

The keygen functions produce a random key. The tag functions compute an authentication tag (32 bytes for SHA-256, 64 bytes for SHA-512). The verify functions check a tag against a message in constant time.

## Authenticated symmetric encryption

Three authenticated ciphers are provided. Each encrypt function produces a ciphertext 16 bytes longer than the plaintext because of the appended authentication tag. Each decrypt function returns `None` if the ciphertext was tampered with. A fresh nonce must be generated for every encryption with a given key.

### XSalsa20-Poly1305 (secretbox)

The recommended default for general-purpose secret key encryption.

```
fn secretbox_keygen() => fs::Buffer?          // 32-byte key
fn secretbox_nonce() => fs::Buffer?           // 24-byte nonce
fn secretbox_encrypt(plaintext: fs::Buffer, nonce: fs::Buffer, key: fs::Buffer) => fs::Buffer?
fn secretbox_decrypt(ciphertext: fs::Buffer, nonce: fs::Buffer, key: fs::Buffer) => fs::Buffer?
```

```
key   := crypto::secretbox_keygen()?;
nonce := crypto::secretbox_nonce()?;
ct    := crypto::secretbox_encrypt(plaintext, nonce, key)?;
pt    := crypto::secretbox_decrypt(ct, nonce, key)?;
```

### ChaCha20-Poly1305 IETF

Recommended for network protocols. It supports additional authenticated data (`ad`) that is authenticated but not encrypted, such as a packet header. Pass an empty buffer when no additional data is needed.

```
fn chacha_keygen() => fs::Buffer?             // 32-byte key
fn chacha_nonce() => fs::Buffer?              // 12-byte nonce
fn chacha_encrypt(plaintext: fs::Buffer, ad: fs::Buffer, nonce: fs::Buffer, key: fs::Buffer) => fs::Buffer?
fn chacha_decrypt(ciphertext: fs::Buffer, ad: fs::Buffer, nonce: fs::Buffer, key: fs::Buffer) => fs::Buffer?
```

```
key   := crypto::chacha_keygen()?;
nonce := crypto::chacha_nonce()?;
ct    := crypto::chacha_encrypt(plaintext, fs::Buffer(), nonce, key)?;
```

### AES-256-GCM

Provided for compatibility with systems that require AES. It needs hardware AES-NI support. Call `aes_available()` first and fall back to another cipher when it returns false.

```
fn aes_available() => bool
fn aes_keygen() => fs::Buffer?                // 32-byte key
fn aes_nonce() => fs::Buffer?                 // 12-byte nonce
fn aes_encrypt(plaintext: fs::Buffer, ad: fs::Buffer, nonce: fs::Buffer, key: fs::Buffer) => fs::Buffer?
fn aes_decrypt(ciphertext: fs::Buffer, ad: fs::Buffer, nonce: fs::Buffer, key: fs::Buffer) => fs::Buffer?
```

## Public key encryption and signatures

### Key pair types

`BoxKeyPair` holds an X25519 key pair for authenticated public key encryption. `SignKeyPair` holds an Ed25519 key pair for digital signatures. Both expose `public_key() => fs::Buffer` and `secret_key() => fs::Buffer`. The public key is meant to be shared. The secret key must never be shared.

### X25519 box encryption

Encrypts a message between two parties using their key pairs. It provides confidentiality and mutual authentication.

```
fn box_keypair() => BoxKeyPair?
fn box_nonce() => fs::Buffer?                 // 24-byte nonce
fn box_encrypt(plaintext: fs::Buffer, nonce: fs::Buffer, recipient_pk: fs::Buffer, sender_sk: fs::Buffer) => fs::Buffer?
fn box_decrypt(ciphertext: fs::Buffer, nonce: fs::Buffer, sender_pk: fs::Buffer, recipient_sk: fs::Buffer) => fs::Buffer?
```

```
alice := crypto::box_keypair()?;
bob   := crypto::box_keypair()?;
nonce := crypto::box_nonce()?;

ct := crypto::box_encrypt(message, nonce, bob.public_key(), alice.secret_key())?;
pt := crypto::box_decrypt(ct, nonce, alice.public_key(), bob.secret_key())?;
```

### Ed25519 signatures

Signs a message to prove authenticity and detect tampering.

```
fn sign_keypair() => SignKeyPair?
fn sign(message: fs::Buffer, secret_key: fs::Buffer) => fs::Buffer?   // 64-byte signature
fn verify(signature: fs::Buffer, message: fs::Buffer, public_key: fs::Buffer) => bool
```

```
keypair := crypto::sign_keypair()?;
sig     := crypto::sign(message, keypair.secret_key())?;
valid   := crypto::verify(sig, message, keypair.public_key());
```

## Key derivation and password hashing

Backed by Argon2id. Use `OpsLimit` and `MemLimit` for the cost settings.

```
fn generate_salt() => fs::Buffer?             // 16-byte salt
fn derive_key(password: string, salt: fs::Buffer, key_len: int, ops_limit: int, mem_limit: int) => fs::Buffer?
fn derive_key(password: string, salt: fs::Buffer, key_len: int) => fs::Buffer?
fn hash_password(password: string, ops_limit: int, mem_limit: int) => string?
fn hash_password(password: string) => string?
fn verify_password(hash_str: string, password: string) => bool
```

`generate_salt` returns a fresh random salt. Always use a new salt for each password and never reuse one.

`derive_key` turns a password and salt into an encryption key of `key_len` bytes. The salt must be stored to derive the same key again. The three-argument form uses the interactive presets.

`hash_password` returns a self-contained string that embeds the salt and Argon2id parameters. The string is safe to store as-is in a database. `verify_password` checks a password against a stored hash string in constant time. The one-argument forms of both use the interactive presets.

```
hashed := crypto::hash_password(password)?;
// store hashed in the database

valid := crypto::verify_password(stored_hash, input_password);
if !valid {
    return HttpResponse("Invalid password", 401);
}
```

## Legacy algorithms

MD5 and SHA-1 are cryptographically broken and are provided only for compatibility with existing systems. Use `sha256` or `blake2b` for any new work.

```
fn md5(data: fs::Buffer) => fs::Buffer        // 16-byte digest
fn sha1(data: fs::Buffer) => fs::Buffer       // 20-byte digest
```

Streaming forms `Md5Hasher` and `Sha1Hasher` are also provided, each with `update(data: fs::Buffer)` and `finalize() => fs::Buffer`.

## Notes

Buffers returned by this package hold GC-managed memory and are tracked across collections. Keys, nonces, and secrets should be cleared with `secure_zero` once they are no longer needed. Nonce reuse with the same key breaks the security of every cipher here, so always generate a fresh nonce per message.
